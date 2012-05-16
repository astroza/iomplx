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
#include <string.h>
#include <iomplx.h>

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

static int iomplx_do_recycle(iomplx_instance *mplx, int recycler_fd)
{
	iomplx_item *items[IOMPLX_ITEMS_DUMP_MAX_SIZE * 4];
	int ret, i;

	do {
		ret = read(recycler_fd, items, sizeof(items));
		if(ret == -1)
			return IOMPLX_ITEM_WOULDBLOCK;
		for(i = 0; i < ret/sizeof(void *); i++) {
			items[i]->parent->item_free(items[i]);
			DLIST_DEL(&mplx->items_to_check, items[i]);
		}

	} while(1);

	return 0;
}

void iomplx_thread_0(iomplx_instance *mplx)
{
	iomplx_item *item, *new_item;
	iomplx_item_call *item_call;
	iomplx_active_list active_list;
	struct sockaddr sa;
	socklen_t sa_size;
	int fd;

	iomplx_active_list_init(&active_list, mplx->active_list_size[THREAD_0]);

	do {
		iomplx_active_list_populate(&active_list, &mplx->accept_uqueue, -1);

		DLIST_FOREACH(&active_list AS item_call) {
			item = item_call->item;
			if(item == &mplx->recycler_item) {
				if(iomplx_do_recycle(mplx, item->fd) == IOMPLX_ITEM_WOULDBLOCK) {
					iomplx_active_list_call_del(&active_list, item_call);
					uqueue_enable(&mplx->accept_uqueue, item);
				}
				continue;
			}

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
				new_item->parent = item;
				if(item->cb.ev_accept(new_item) < 0) {
					item->item_free(new_item);
					close(fd);
				} else {
					iomplx_item_add(mplx, new_item, 0);
					DLIST_APPEND(&mplx->items_to_check, new_item);
				}
			}
		}
	} while(1);
}
