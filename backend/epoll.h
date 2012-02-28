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

#ifndef EPOLL_H
#include <sys/epoll.h>
#define EVENTS 			10
#define EPOLL_QUEUE_SIZE 	100

typedef struct {
	struct epoll_event events[EVENTS];
	unsigned int events_count;
	unsigned int current_event;
} uqueue_events;

typedef struct {
	int epoll_iface;
} uqueue;

#define UQUEUE_RECEIVE_EVENT	EPOLLIN
#define UQUEUE_SEND_EVENT	EPOLLOUT

#endif
