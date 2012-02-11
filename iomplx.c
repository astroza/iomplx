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
#include <signal.h>

static void iomplx_waiter_init(iomplx_waiter *waiter)
{
	waiter->item = NULL;
	uqueue_event_init(waiter);
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

static void iomplx_active_list_init(iomplx_active_list *active_list, unsigned int calls_number)
{
	DLIST_INIT(active_list);
	mempool_init(&active_list->item_calls_pool, sizeof(iomplx_item_call), calls_number);
	active_list->available_item_calls = calls_number;
}

static int iomplx_active_list_call_add(iomplx_active_list *active_list, iomplx_item *item, unsigned char call_idx)
{
	iomplx_item_call *item_call;

	item_call = mempool_alloc(&active_list->item_calls_pool);
	if(!item_call)
		return -1;

	item_call->call_idx = call_idx;
	item_call->item = item;

	DLIST_APPEND(active_list, item_call);
	active_list->available_item_calls--;

	return 0;
}

static void iomplx_active_list_call_del(iomplx_active_list *active_list, iomplx_item_call *call)
{
	DLIST_DEL(active_list, call);
	mempool_free(&active_list->item_calls_pool, call);
	active_list->available_item_calls++;
}

static void iomplx_active_list_populate(iomplx_active_list *active_list, uqueue *q, int wait_timeout)
{
	int timeout;
	int rmg;
	iomplx_waiter waiter;

	if(active_list->available_item_calls == 0)
		return;

	iomplx_waiter_init(&waiter);

	if(DLIST_ISEMPTY(active_list))
		timeout = wait_timeout;
	else
		timeout = 0;

	uqueue_max_events_set(&waiter, active_list->available_item_calls);
	do {
		rmg = uqueue_event_get(q, &waiter, timeout);
		if(rmg != -1) {
			waiter.item->active = 1;
			iomplx_active_list_call_add(active_list, waiter.item, waiter.type);
		}
	} while(rmg > 0);
}

static inline void iomplx_item_timeout_check(uqueue *n_uqueue, iomplx_item *item, unsigned long cur_time)
{
	int ret;

	switch(item->timeout.stage) {
		case 0:
			if(item->timeout.time_limit > 0) {
				if(cur_time - item->timeout.start_time >= item->timeout.time_limit) {
					item->timeout.high = 1;
					item->timeout.stage = 1;
					item->disabled = 1;
					uqueue_unwatch(n_uqueue, item);
				}
			}
			break;
		case 1:
			if(item->disabled == 0 || item->active == 1) {
				item->disabled = 1;
				uqueue_unwatch(n_uqueue, item);
			} else {
				ret = item->cb.ev_timeout(item);
				if(ret == 0) {
					item->timeout.high = 0;
					item->timeout.start_time = time(NULL);
					item->timeout.stage = 0;
					item->disabled = 0;
					uqueue_watch(n_uqueue, item);
				} else {
					item->cb.ev_close(item);
					close(item->fd);
					item->fd = -1;
				}
			}
			break;
	}
}

static void iomplx_guard_maintenance(uqueue *n_uqueue, dlist *items, free_func item_free, unsigned long cur_time)
{
	iomplx_item *item;

	DLIST_FOREACH(items AS item) {
		/* Free closed item */
		if(item->fd == -1) {
			DLIST_DEL(items, item);
			item_free(item);
			continue;
		}
		iomplx_item_timeout_check(n_uqueue, item, cur_time);
	}
}

static void iomplx_do_maintenance(uqueue *n_uqueue, dlist *guards, unsigned long *start_time)
{
	unsigned long cur_time = time(NULL);
	iomplx_item *item;

	if(cur_time - *start_time >= IOMPLX_MAINTENANCE_PERIOD) {
		DLIST_FOREACH(guards AS item)
			iomplx_guard_maintenance(n_uqueue, &item->guard, item->item_free, cur_time);
		*start_time = cur_time;
	}
}

static void iomplx_thread_0(iomplx_instance *mplx)
{
	iomplx_item *item, *new_item;
	iomplx_item_call *item_call;
	iomplx_active_list active_list;
	struct sockaddr sa;
	socklen_t sa_size;
	int fd;
	unsigned long start_time = time(NULL);

	iomplx_active_list_init(&active_list, mplx->active_list_size[THREAD_0]);

	do {
		iomplx_active_list_populate(&active_list, &mplx->accept_uqueue, IOMPLX_MAINTENANCE_PERIOD);

		DLIST_FOREACH(&active_list AS item_call) {
			item = item_call->item;
			sa_size = item->sa_size;
			fd = accept_and_set(item->fd, &sa, &sa_size);
			if(fd == -1) {
				iomplx_active_list_call_del(&active_list, item_call);
				continue;
			}

			if(!(new_item = item->item_alloc(sizeof(iomplx_item))))
				close(fd);
			else {
				iomplx_callbacks_init(new_item);
				new_item->fd = fd;
				new_item->filter = IOMPLX_READ;
				new_item->sa = sa;
				new_item->sa_size = sa_size;
				new_item->oneshot = 1;
				if(item->cb.ev_accept(new_item) < 0) {
					item->item_free(new_item);
					close(fd);
				} else {
					iomplx_item_add(mplx, new_item, 0);
					DLIST_APPEND(&item->guard, new_item);
				}
			}
		}
		iomplx_do_maintenance(&mplx->n_uqueue, &mplx->guards, &start_time);

	} while(1);
}

static void iomplx_thread_n(iomplx_instance *mplx)
{
	iomplx_active_list active_list;
	iomplx_item_call *item_call;
	iomplx_item *item;
	int ret;

	signal(SIGPIPE, SIG_IGN);
	if(mplx->thread_init)
		mplx->thread_init();

	iomplx_active_list_init(&active_list, mplx->active_list_size[THREAD_N]);

	do {
		iomplx_active_list_populate(&active_list, &mplx->n_uqueue, -1);

		DLIST_FOREACH(&active_list AS item_call) {
			item = item_call->item;
			ret = item->cb.calls_arr[item_call->call_idx](item);

			if(ret == IOMPLX_ITEM_CLOSE && item_call->call_idx != IOMPLX_CLOSE_EVENT)  {
				item_call->call_idx = IOMPLX_CLOSE_EVENT;
				continue;
			}

			if(item_call->call_idx == IOMPLX_CLOSE_EVENT) {
				close(item->fd);
				item->fd = -1;
				iomplx_active_list_call_del(&active_list, item_call);
			} else if(ret == IOMPLX_ITEM_WOULDBLOCK) {
				if(!item->timeout.high) {
					if(uqueue_enable(&mplx->n_uqueue, item) == 0)
						item->disabled = 0;
				}
				iomplx_active_list_call_del(&active_list, item_call);
				item->active = 0;
			} else if(item->filter != item->applied_filter) {
				if(item->filter == IOMPLX_WRITE)
					item_call->call_idx = IOMPLX_WRITE_EVENT;
				else if(item->filter == IOMPLX_READ)
					item_call->call_idx = IOMPLX_READ_EVENT;

				item->applied_filter = item->filter;
			}
		}

	} while(1);
}

void iomplx_item_add(iomplx_instance *mplx, iomplx_item *item, int listening)
{
	DLIST_NODE_INIT(item);
	item->disabled = 0;
	item->active = 0;
	item->timeout.stage = 0;
	item->timeout.high = 0;
	if(listening)
		uqueue_watch(&mplx->accept_uqueue, item);
	else
		uqueue_watch(&mplx->n_uqueue, item);
}

void iomplx_init(iomplx_instance *mplx, init_func init, unsigned int threads)
{
	mplx->thread_init = init;
	mplx->threads = threads;
	mplx->active_list_size[THREAD_0] = 10;
	mplx->active_list_size[THREAD_N] = IOMPLX_MAX_ACTIVE_ITEMS/threads + (IOMPLX_MAX_ACTIVE_ITEMS%threads != 0? 1 : 0);
	DLIST_INIT(&mplx->guards);
	uqueue_init(&mplx->accept_uqueue);
	uqueue_init(&mplx->n_uqueue);
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
