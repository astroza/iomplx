/*
 * IOMPLX
 * Copyright © 2012 Felipe Astroza
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <pthread.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <iomplx.h>
#include <time.h>
#include <unistd.h>

static inline void iomplx_monitor_add(iomplx_monitor *mon, iomplx_item *item)
{
	DLIST_APPEND(mon, item);
}

static inline void iomplx_monitor_del(iomplx_monitor *mon, iomplx_item *item)
{
	DLIST_DEL(mon, item);
}

static inline void iomplx_monitor_init(iomplx_monitor *mon, int timeout_granularity)
{
	DLIST_INIT(mon);
	mon->timeout_granularity = timeout_granularity;
}

static iomplx_item *iomplx_item_clone(iomplx_instance *mplx, iomplx_item *item)
{
	iomplx_item *new_item;

	new_item = mplx->item_alloc(sizeof(iomplx_item));
	if(new_item)
		memcpy(new_item, item, sizeof(iomplx_item));

	return new_item;
}

static void iomplx_item_init(iomplx_item *item, unsigned int sa_size, const iomplx_callbacks *cb)
{
	item->sa_size = sa_size;
	item->new_filter = IOMPLX_READ;
	DLIST_NODE_INIT(item);
	memcpy(&item->cb, cb, sizeof(iomplx_callbacks));
}

static void iomplx_event_init(iomplx_event *ev)
{
	ev->item = NULL;
	uqueue_event_init(ev);
}

static int iomplx_dummy_call1(iomplx_item *item)
{
	return 0;
}

static const iomplx_callbacks iomplx_dummy_calls = {
	.calls_arr = {iomplx_dummy_call1, iomplx_dummy_call1, iomplx_dummy_call1, iomplx_dummy_call1, iomplx_dummy_call1}
};

static void iomplx_do_maintenance(iomplx_instance *mplx, unsigned long *start_time)
{
	iomplx_item *item;
	
	if(time(NULL) - *start_time >= mplx->monitor.timeout_granularity) {
		DLIST_FOREACH(&mplx->monitor AS item) {
			/* Free closed item */
			if(item->fd == -1) {
				iomplx_monitor_del(&mplx->monitor, item);
				mplx->item_free(item);
				continue;
			}
			/* Timeout check */
			item->elapsed_time++;
			if(item->elapsed_time >= item->timeout && item->cb.ev_timeout(item) == -1)
				shutdown(item->fd, SHUT_RDWR);
		}
		*start_time = time(NULL);
	}
}

static void iomplx_thread_0(iomplx_instance *mplx)
{
	iomplx_item *item, local_item;
	iomplx_event ev;
	unsigned long start_time = time(NULL);

	iomplx_event_init(&ev);
	do {
		if(uqueue_wait(&mplx->accept_queue, &ev, mplx->monitor.timeout_granularity)) {

			iomplx_item_init(&local_item, ev.item->sa_size, &iomplx_dummy_calls);
			local_item.fd = accept_and_set(ev.item->fd, &local_item.sa, &local_item.sa_size);

			if(local_item.fd != -1) {
				if(ev.item->cb.ev_accept(&local_item) < 0 || !(item = iomplx_item_clone(mplx, &local_item)))
					close(local_item.fd);
				else {
					item->elapsed_time = 0;
					iomplx_monitor_add(&mplx->monitor, item);
					uqueue_watch(&mplx->n_queue, item);
					item->new_filter = -1;
				}
			}
		}

		iomplx_do_maintenance(mplx, &start_time);
		
	} while(1);
}

static void iomplx_thread_n(iomplx_instance *mplx)
{
	iomplx_event ev;

	iomplx_event_init(&ev);
	do {
		uqueue_wait(&mplx->n_queue, &ev, -1);

		ev.item->elapsed_time = 0;
		if(ev.type == IOMPLX_CLOSE_EVENT) {
			uqueue_unwatch(&mplx->n_queue, ev.item);
			close(ev.item->fd);
			ev.item->fd = -1;
		}

		ev.item->cb.calls_arr[ev.type](ev.item);

		if(ev.item->new_filter != -1) {
			uqueue_filter_set(&mplx->n_queue, ev.item);
			ev.item->new_filter = -1;
		}
	} while(1);
}

inline void iomplx_item_filter_set(iomplx_item *item, int filter)
{
	item->new_filter = filter;
}

int iomplx_listen(iomplx_instance *mplx, const char *addr, unsigned short port, ev_call1 ev_accept, void *data)
{
	struct sockaddr_in sa;
	iomplx_item *item;
	iomplx_callbacks cb;
	socklen_t sa_size;
	int sockfd;
	int ret;
	int status = 1;

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if ( sockfd == -1 )
		return -1;

	sa.sin_family = AF_INET;
	sa.sin_port = htons(port);
	sa.sin_addr.s_addr = inet_addr(addr);
	sa_size = sizeof(struct sockaddr_in);

	if(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &status, sizeof(status)) == -1)
		goto error;

	ret = bind(sockfd, (struct sockaddr *)&sa, sa_size);
	if ( ret == -1 )
		goto error;

	if (listen(sockfd, IOMPLX_CONF_BACKLOG) == -1)
		goto error;

	item = mplx->item_alloc(sizeof(iomplx_item));
	if(!item)
		goto error;

	cb.ev_accept = ev_accept;
	iomplx_item_init(item, sa_size, &cb);
	item->fd = sockfd;
	uqueue_watch(&mplx->accept_queue, item);

	return 0;
error:
	close(sockfd);
	return -1;
}

int iomplx_fd_add(iomplx_instance *mplx, int fd, iomplx_callbacks *cb, int filter, void *data)
{
	iomplx_item *item;

	item = mplx->item_alloc(sizeof(iomplx_item));
	if(item == NULL)
		return -1;

	item->new_filter = filter;
        DLIST_NODE_INIT(item);
        memcpy(&item->cb, cb, sizeof(iomplx_callbacks));
	uqueue_watch(&mplx->n_queue, item);

	return 1;
}

/*
int iomplx_connect(const char *addr, unsigned short port, iomplx_callbacks *cb, void *data)
{

}

void iomplx_specialize()
{

}
*/
void iomplx_init(iomplx_instance *mplx, alloc_func alloc, free_func free, unsigned int threads, unsigned int timeout_granularity)
{
	mplx->item_alloc = alloc;
	mplx->item_free = free;
	mplx->threads = threads;
	iomplx_monitor_init(&mplx->monitor, timeout_granularity);
	uqueue_init(&mplx->accept_queue);
	uqueue_init(&mplx->n_queue);
}

static void iomplx_start_threads(iomplx_instance *mplx)
{
	unsigned int i;
	pthread_attr_t attr;
	pthread_t unused;

	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

	for(i = 0; i < mplx->threads; i++)
		pthread_create(&unused, &attr, (void *(*)(void *))iomplx_thread_n, (void *)mplx);
}

void iomplx_launch(iomplx_instance *mplx)
{
	iomplx_start_threads(mplx);
	iomplx_thread_0(mplx);
}
