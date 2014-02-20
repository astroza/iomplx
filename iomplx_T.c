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
	if(item->timeout.time_limit > 0 && pthread_mutex_trylock(&item->timeout.lock) == 0) {
		if(cur_time - item->timeout.start_time >= item->timeout.time_limit) {
			item->cb.ev_timeout(item);
			item->cb.ev_close(item);
			close(item->fd);
			iomplx_item_throw_away(mplx, dump, item);
		}
		pthread_mutex_unlock(&item->timeout.lock);
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
