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

#include <unistd.h>
#include <pthread.h>
#include <sys/ioctl.h>
#include <iomplx.h>
#include <stdio.h>

static void iomplx_waiter_init(iomplx_waiter *waiter)
{
	waiter->item = NULL;
	uqueue_event_init(waiter);
}

void iomplx_active_list_init(iomplx_active_list *active_list, unsigned int calls_number)
{
	DLIST_INIT(active_list);
	mempool_init(&active_list->item_calls_pool, sizeof(iomplx_item_call), calls_number);
	active_list->available_item_calls = calls_number;
}

int iomplx_active_list_call_add(iomplx_active_list *active_list, iomplx_item *item, unsigned char call_idx)
{
	iomplx_item_call *item_call;

	/* Is it timeouted? */
	if(pthread_mutex_trylock(&item->timeout.lock) != 0)
		return -1;

	/* Refresh the timeout counter */
	item->timeout.start_time = time(NULL);

	item_call = mempool_alloc(&active_list->item_calls_pool);
	if(!item_call)
		return -1;

	item_call->call_idx = call_idx;
	item_call->item = item;

	DLIST_APPEND(active_list, item_call);
	active_list->available_item_calls--;

	return 0;
}

void iomplx_active_list_call_del(iomplx_active_list *active_list, iomplx_item_call *call)
{
	DLIST_DEL(active_list, call);
	mempool_free(&active_list->item_calls_pool, call);
	active_list->available_item_calls++;

	/* It can timeout now */
	pthread_mutex_unlock(&call->item->timeout.lock);
}

void iomplx_active_list_populate(iomplx_active_list *active_list, uqueue *q, int wait_timeout)
{
	int timeout;
	int rmg;
	iomplx_waiter waiter;

	if(active_list->available_item_calls == 0)
		return;

	iomplx_waiter_init(&waiter);

	/* We won't wait an item if the list is already populated
	 */
	if(DLIST_ISEMPTY(active_list))
		timeout = wait_timeout;
	else
		timeout = 0;

	/* We say to waiter that there is a limited space for item_calls
	 */
	uqueue_max_events_set(&waiter, active_list->available_item_calls);
	do {
		rmg = uqueue_event_get(q, &waiter, timeout);
		if(rmg != -1) {
			if(iomplx_active_list_call_add(active_list, waiter.item, waiter.type) == -1) {
				fprintf(stdout, "Can't add item->fd=%d to active_list(%p)\n", waiter.item->fd, active_list);
			}
		}
	} while(rmg > 0);
}

void iomplx_items_recycle(iomplx_instance *mplx, iomplx_items_dump *dump)
{
	if(dump->size > 0) {
		write(mplx->recycler[1], dump->items, dump->size * sizeof(void *));
		dump->size = 0;
	}
}

void iomplx_item_throw_away(iomplx_instance *mplx, iomplx_items_dump *dump, iomplx_item *item)
{
	pthread_mutex_destroy(&item->timeout.lock);
	if(dump->size < IOMPLX_ITEMS_DUMP_MAX_SIZE)
		dump->items[dump->size++] = item;
	else {
		iomplx_items_recycle(mplx, dump);
		iomplx_item_throw_away(mplx, dump, item);
	}
}

void iomplx_item_add(iomplx_instance *mplx, iomplx_item *item, int listening)
{
	DLIST_NODE_INIT(item);
	item->closed = 0;
	pthread_mutex_init(&item->timeout.lock, NULL);
	if(listening)
		uqueue_watch(&mplx->accept_uqueue, item);
	else
		uqueue_watch(&mplx->n_uqueue, item);
}

void iomplx_init(iomplx_instance *mplx, init_func init, unsigned int threads)
{
	int set = 1;

	mplx->thread_init = init;
	mplx->threads = threads;
	mplx->active_list_size[THREAD_0] = 10;
	mplx->active_list_size[THREAD_N] = IOMPLX_MAX_ACTIVE_ITEMS/threads + (IOMPLX_MAX_ACTIVE_ITEMS%threads != 0? 1 : 0);
	uqueue_init(&mplx->accept_uqueue);
	uqueue_init(&mplx->n_uqueue);

	pipe(mplx->recycler);
	ioctl(mplx->recycler[0], FIONBIO, &set);
	mplx->recycler_item.fd = mplx->recycler[0];
	mplx->recycler_item.oneshot = 1;
	iomplx_item_filter_set(&mplx->recycler_item, IOMPLX_READ);
	iomplx_item_add(mplx, &mplx->recycler_item, 1);
	DLIST_INIT(&mplx->items_to_check);
}

static void iomplx_start_threads(iomplx_instance *mplx)
{
	unsigned int i;
	pthread_attr_t attr;
	pthread_t unused;

	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

	for(i = 0; i < mplx->threads; i++)
		pthread_create(&unused, &attr, (void *(*)(void *))iomplx_thread_N, (void *)mplx);

	pthread_create(&unused, &attr, (void *(*)(void *))iomplx_thread_T, (void *)mplx);
}

void iomplx_launch(iomplx_instance *mplx)
{
	iomplx_start_threads(mplx);
	iomplx_thread_0(mplx);
}
