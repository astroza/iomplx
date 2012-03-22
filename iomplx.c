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

void iomplx_callbacks_init(iomplx_item *item)
{
	memcpy(&item->cb, &iomplx_dummy_calls, sizeof(iomplx_callbacks));
}

static void iomplx_do_maintenance(iomplx_instance *mplx, unsigned long *start_time)
{
	iomplx_item *item;
	time_t cur_time = time(NULL);
	
	if(cur_time - *start_time >= mplx->monitor.timeout_granularity) {
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
		*start_time = cur_time;
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

			iomplx_callbacks_init(&local_item);
			local_item.new_filter = IOMPLX_READ;
			local_item.sa_size = ev.item->sa_size;
			local_item.fd = accept_and_set(ev.item->fd, &local_item.sa, &local_item.sa_size);

			if(local_item.fd != -1) {
				if(ev.item->cb.ev_accept(&local_item) < 0 || !(item = iomplx_item_add(mplx, &local_item, 0)))
					close(local_item.fd);
				else
					iomplx_monitor_add(&mplx->monitor, item);
			}
		}

		iomplx_do_maintenance(mplx, &start_time);
		
	} while(1);
}

static void iomplx_thread_n(iomplx_instance *mplx)
{
	iomplx_event ev;

	if(mplx->thread_init)
		mplx->thread_init();

	iomplx_event_init(&ev);
	do {
		uqueue_wait(&mplx->n_queue, &ev, -1);

		ev.item->elapsed_time = 0;

		if(ev.item->cb.calls_arr[ev.type](ev.item) == -1 || ev.type == IOMPLX_CLOSE_EVENT) {
			uqueue_unwatch(&mplx->n_queue, ev.item);
			close(ev.item->fd);
			ev.item->fd = -1;
		}

		if(ev.item->new_filter != -1) {
			uqueue_filter_set(&mplx->n_queue, ev.item);
			ev.item->new_filter = -1;
		}
	} while(1);
}

iomplx_item *iomplx_item_add(iomplx_instance *mplx, iomplx_item *item, int listening)
{
	iomplx_item *item_copy;

	item_copy = mplx->item_alloc(sizeof(iomplx_item));
	if(item_copy == NULL)
		return NULL;

	DLIST_NODE_INIT(item_copy);
	item_copy->new_filter = item->new_filter;
	memcpy(&item_copy->cb, &item->cb, sizeof(iomplx_callbacks));
	item_copy->fd = item->fd;
	item_copy->timeout = item->timeout;

	if(listening)
		uqueue_watch(&mplx->accept_queue, item_copy);
	else
		uqueue_watch(&mplx->n_queue, item_copy);

	item_copy->elapsed_time = 0;
	item_copy->new_filter = -1;

	return item_copy;
}

void iomplx_init(iomplx_instance *mplx, alloc_func alloc, free_func free, init_func init, unsigned int threads, unsigned int timeout_granularity)
{
	mplx->item_alloc = alloc;
	mplx->item_free = free;
	mplx->thread_init = init;
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
