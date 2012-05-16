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

#include <stdlib.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <iomplx.h>
#include <iomplx_inet.h>
#include <sys/ioctl.h>
#include <unistd.h>

int iomplx_inet_listen(iomplx_instance *mplx, const char *addr, unsigned short port, ev_call1 ev_accept, alloc_func item_alloc, free_func item_free, void *data)
{
	iomplx_item *item;
	struct sockaddr_in *sa;
	int ret, set = 1;

	item = malloc(sizeof(iomplx_item));
	if(!item)
		return -1;

	item->fd = socket(AF_INET, SOCK_STREAM, 0);
	if(item->fd == -1) {
		free(item);
		return -1;
	}

	sa = (struct sockaddr_in *)&item->sa;
	sa->sin_family = AF_INET;
	sa->sin_port = htons(port);
	sa->sin_addr.s_addr = inet_addr(addr);
	item->sa_size = sizeof(struct sockaddr_in);

	if(setsockopt(item->fd, SOL_SOCKET, SO_REUSEADDR, &set, sizeof(set)) == -1)
		goto error;

	ret = bind(item->fd, &item->sa, item->sa_size);
	if(ret == -1)
		goto error;

	if(listen(item->fd, IOMPLX_CONF_BACKLOG) == -1)
		goto error;

	if(ioctl(item->fd, FIONBIO, &set) == -1)
		goto error;

	item->item_alloc = item_alloc;
	item->item_free = item_free;
	item->data = data;
	item->oneshot = 0;
	item->filter = IOMPLX_READ;
	item->cb.ev_accept = ev_accept;
	item->parent = NULL;

	iomplx_item_add(mplx, item, 1);
	return 0;
error:
	close(item->fd);
	free(item);
	return -1;
}
