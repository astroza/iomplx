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

#include <time.h>
#include <unistd.h>
#include <iomplx.h>

static void iomplx_item_timeout_check(iomplx_instance *mplx, iomplx_item *item, unsigned long cur_time, iomplx_items_dump *dump)
{
	int ret;

	switch(item->timeout.stage) {
		case 0:
			if(item->timeout.time_limit > 0) {
				if(cur_time - item->timeout.start_time >= item->timeout.time_limit) {
					item->timeout.high = 1;
					item->timeout.stage = 1;
					item->disabled = 1;
					uqueue_unwatch(&mplx->n_uqueue, item);
				}
			}
			break;
		case 1:
			if(item->disabled == 0 || item->active == 1) {
				item->disabled = 1;
				uqueue_unwatch(&mplx->n_uqueue, item);
			} else {
				ret = item->cb.ev_timeout(item);
				if(ret == 0) {
					item->timeout.high = 0;
					item->timeout.start_time = cur_time;
					item->timeout.stage = 0;
					item->disabled = 0;
					uqueue_watch(&mplx->n_uqueue, item);
				} else {
					item->cb.ev_close(item);
					close(item->fd);
					item->closed = 1;
					iomplx_item_throw_away(mplx, dump, item);
				}
			}
			break;
	}
}

void iomplx_thread_T(iomplx_instance *mplx)
{
	iomplx_item *item;
	int sleep_time = 0;
	iomplx_items_dump dump;
	
	iomplx_items_dump_init(&dump);
	do {
		if(sleep_time < IOMPLX_MAINTENANCE_PERIOD)
			sleep_time = IOMPLX_MAINTENANCE_PERIOD;

		sleep(sleep_time);

		DLIST_FOREACH(&mplx->items_to_check AS item) {
			if(item->closed)
				continue;

			iomplx_item_timeout_check(mplx, item, time(NULL), &dump);
			if(item->timeout.time_limit > 0 && item->timeout.time_limit < sleep_time)
				sleep_time = item->timeout.time_limit;
		}
		iomplx_items_recycle(mplx, &dump);
	} while(1);
}
