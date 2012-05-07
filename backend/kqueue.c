/*
 * iomplx
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

#include <iomplx.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <assert.h>
extern int errno;

void uqueue_init(uqueue *q)
{
	q->kqueue_iface = kqueue();
}

void uqueue_event_init(iomplx_waiter *waiter)
{
	waiter->data.events_count = 0;
	waiter->data.current_event = 0;
	waiter->max_events = EVENTS;
}

int uqueue_event_get(uqueue *q, iomplx_waiter *waiter, int timeout)
{
	struct kevent *ke;
	struct timespec ts, *pts;
	int wait_ret;

	waiter->data.current_event++;
	if(waiter->data.events_count <= waiter->data.current_event) {
		do {
			if(timeout < 0)
				pts = NULL;
			else {
				ts.tv_sec = timeout;
				ts.tv_nsec = 0;
				pts = &ts;
			}
			wait_ret = kevent(q->kqueue_iface, NULL, 
			0, waiter->data.events, EVENTS, pts);
		} while(wait_ret == -1 && errno == EINTR);

		if(wait_ret == 0) 
			return -1;

		waiter->data.events_count = wait_ret;
		waiter->data.current_event = 0;
	}

	ke = waiter->data.events + waiter->data.current_event;
	waiter->item = ke->udata;
	assert(waiter->item != NULL);
	if(ke->flags & EV_EOF)
		waiter->type = IOMPLX_CLOSE_EVENT;
	else if(ke->filter == EVFILT_READ)
		waiter->type = IOMPLX_READ_EVENT;
	else if(ke->filter == EVFILT_WRITE)
		waiter->type = IOMPLX_WRITE_EVENT;

	return waiter->data.events_count - waiter->data.current_event - 1;
}

void uqueue_watch(uqueue *q, iomplx_item *item)
{
	struct kevent c;
	int ext_flags = 0;

	if(item->oneshot)
		ext_flags |= EV_ONESHOT;

	EV_SET(&c, item->fd, item->filter, EV_ADD|EV_CLEAR|EV_ENABLE|EV_RECEIPT|ext_flags, 0, 0, item);
	kevent(q->kqueue_iface, &c, 1, NULL, 0, NULL);
	item->applied_filter = item->filter;
}

void uqueue_unwatch(uqueue *q, iomplx_item *item)
{
	struct kevent c;

	EV_SET(&c, item->fd, 0, EV_DELETE, 0, 0, item);
	kevent(q->kqueue_iface, &c, 1, NULL, 0, NULL);
}

void uqueue_enable(uqueue *q, iomplx_item *item)
{
	uqueue_watch(q, item);
}

void uqueue_filter_set(uqueue *q, iomplx_item *item)
{
	uqueue_watch(q, item);
}

int accept_and_set(int fd, struct sockaddr *sa, unsigned int *sa_size)
{
	int new_fd;
	int set = 1;

	new_fd = accept(fd, sa, sa_size);
	if(new_fd != -1)
		ioctl(new_fd, FIONBIO, &set);

	return new_fd;
}
