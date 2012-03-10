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

#ifndef KQUEUE_H

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/event.h>

#define EVENTS 			10

typedef struct {
	struct kevent events[EVENTS];
	unsigned int events_count;
	unsigned int current_event;
} uqueue_events;

typedef struct {
	int kqueue_iface;
	struct kevent changes[EVENTS];
	unsigned int changes_count;
} uqueue;

#define UQUEUE_READ_EVENT	EVFILT_READ
#define UQUEUE_WRITE_EVENT	EVFILT_WRITE

#endif
