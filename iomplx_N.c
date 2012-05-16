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
#include <signal.h>
#include <iomplx.h>

static void iomplx_items_recycle(iomplx_instance *mplx, iomplx_items_dump *dump)
{
	if(dump->size > 0) {
		write(mplx->recycler[1], dump->items, dump->size * sizeof(void *));
		dump->size = 0;
	}
}

static void iomplx_item_throw_away(iomplx_instance *mplx, iomplx_items_dump *dump, iomplx_item *item)
{
	if(dump->size < IOMPLX_ITEMS_DUMP_MAX_SIZE)
		dump->items[dump->size++] = item;
	else {
		iomplx_items_recycle(mplx, dump);
		iomplx_item_throw_away(mplx, dump, item);
	}
}

void iomplx_thread_N(iomplx_instance *mplx)
{
	iomplx_active_list active_list;
	iomplx_item_call *item_call;
	iomplx_items_dump dump;
	iomplx_item *item;
	int ret;

	signal(SIGPIPE, SIG_IGN);
	if(mplx->thread_init)
		mplx->thread_init();

	iomplx_active_list_init(&active_list, mplx->active_list_size[THREAD_N]);
	iomplx_items_dump_init(&dump);

	do {
		iomplx_items_recycle(mplx, &dump);
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
				iomplx_active_list_call_del(&active_list, item_call);
				iomplx_item_throw_away(mplx, &dump, item);
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