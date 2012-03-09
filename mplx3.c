/*
 * MPLX3
 * Copyright © 2011 Felipe Astroza
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
#include <mplx3.h>
#include <time.h>
#include <unistd.h>

static inline void mplx3_monitor_add(mplx3_monitor *mon, mplx3_endpoint *ep)
{
	DLIST_APPEND(mon, ep);
}

static inline void mplx3_monitor_del(mplx3_monitor *mon, mplx3_endpoint *ep)
{
	DLIST_DEL(mon, ep);
}

static inline void mplx3_monitor_init(mplx3_monitor *mon, int timeout_granularity)
{
	DLIST_INIT(mon);
	mon->timeout_granularity = timeout_granularity;
}

static mplx3_endpoint *mplx3_endpoint_clone(mplx3_multiplexer *mplx, mplx3_endpoint *ep)
{
	mplx3_endpoint *new_ep;

	new_ep = mplx->ep_alloc(sizeof(mplx3_endpoint));
	if(new_ep)
		memcpy(new_ep, ep, sizeof(mplx3_endpoint));

	return new_ep;
}

static void mplx3_endpoint_init(mplx3_endpoint *ep, unsigned int sa_size, const mplx3_callbacks *cb)
{
	ep->sa_size = sa_size;
	ep->new_filter = MPLX3_RECEIVE;
	DLIST_NODE_INIT(ep);
	memcpy(&ep->cb, cb, sizeof(mplx3_callbacks));
}

static void mplx3_event_init(mplx3_event *ev)
{
	ev->ep = NULL;
	uqueue_event_init(ev);
}

static int mplx3_dummy_call1(mplx3_endpoint *ep)
{
	return 0;
}

static const mplx3_callbacks mplx3_dummy_calls = {
	.calls_arr = {mplx3_dummy_call1, mplx3_dummy_call1, mplx3_dummy_call1, mplx3_dummy_call1, mplx3_dummy_call1}
};

static void mplx3_thread_0(mplx3_multiplexer *mplx)
{
	mplx3_endpoint *ep, local_ep;
	mplx3_event ev;
	unsigned long start_time = time(NULL);

	mplx3_event_init(&ev);
	do {
		if(uqueue_wait(&mplx->accept_queue, &ev, mplx->monitor.timeout_granularity)) {

			mplx3_endpoint_init(&local_ep, ev.ep->sa_size, &mplx3_dummy_calls);
			local_ep.sockfd = accept_and_set(ev.ep->sockfd, &local_ep.sa, &local_ep.sa_size);

			if(local_ep.sockfd != -1) {
				if(ev.ep->cb.ev_accept(&local_ep) < 0 || !(ep = mplx3_endpoint_clone(mplx, &local_ep)))
					close(local_ep.sockfd);
				else {
					ep->elapsed_time = 0;
					mplx3_monitor_add(&mplx->monitor, ep);
					uqueue_watch(&mplx->n_queue, ep);
					ep->new_filter = -1;
				}
			}
		}

		if(time(NULL) - start_time >= mplx->monitor.timeout_granularity) {
			DLIST_FOREACH(&mplx->monitor AS ep) {
				if(ep->sockfd == -1) {
					mplx3_monitor_del(&mplx->monitor, ep);
					mplx->ep_free(ep);
					continue;
				}

				ep->elapsed_time++;
				if(ep->elapsed_time >= ep->timeout && ep->cb.ev_timeout(ep) == -1)
					shutdown(ep->sockfd, SHUT_RDWR);
			}
			start_time = time(NULL);
		}
	} while(1);
}

static void mplx3_thread_n(mplx3_multiplexer *mplx)
{
	mplx3_event ev;

	mplx3_event_init(&ev);
	do {
		uqueue_wait(&mplx->n_queue, &ev, -1);

		ev.ep->elapsed_time = 0;
		if(ev.type == MPLX3_DISCONNECT_EVENT) {
			uqueue_unwatch(&mplx->n_queue, ev.ep);
			close(ev.ep->sockfd);
			ev.ep->sockfd = -1;
		}

		ev.ep->cb.calls_arr[ev.type](ev.ep);

		if(ev.ep->new_filter != -1) {
			uqueue_filter_set(&mplx->n_queue, ev.ep);
			ev.ep->new_filter = -1;
		}
	} while(1);
}

inline void mplx3_endpoint_filter_set(mplx3_endpoint *ep, int filter)
{
	ep->new_filter = filter;
}

int mplx3_listen(mplx3_multiplexer *mplx, const char *addr, unsigned short port, ev_call1 ev_accept, void *data)
{
	struct sockaddr_in sa;
	mplx3_endpoint *ep;
	mplx3_callbacks cb;
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

	if (listen(sockfd, MPLX3_CONF_BACKLOG) == -1)
		goto error;

	ep = mplx->ep_alloc(sizeof(mplx3_endpoint));
	if(!ep)
		goto error;

	cb.ev_accept = ev_accept;
	mplx3_endpoint_init(ep, sa_size, &cb);
	ep->sockfd = sockfd;
	uqueue_watch(&mplx->accept_queue, ep);

	return 0;
error:
	close(sockfd);
	return -1;
}
/*
int mplx3_connect(const char *addr, unsigned short port, mplx3_callbacks *cb, void *data)
{

}

void mplx3_specialize()
{

}
*/
void mplx3_init(mplx3_multiplexer *mplx, alloc_func alloc, free_func free, unsigned int threads, unsigned int timeout_granularity)
{
	mplx->ep_alloc = alloc;
	mplx->ep_free = free;
	mplx->threads = threads;
	mplx3_monitor_init(&mplx->monitor, timeout_granularity);
	uqueue_init(&mplx->accept_queue);
	uqueue_init(&mplx->n_queue);
}

void mplx3_launch(mplx3_multiplexer *mplx)
{
	{
		unsigned int i;
		pthread_attr_t attr;
		pthread_t unused;

		pthread_attr_init(&attr);
		pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

		for(i = 0; i < mplx->threads; i++)
			pthread_create(&unused, &attr, (void *(*)(void *))mplx3_thread_n, (void *)mplx);
	}
	mplx3_thread_0(mplx);
}
