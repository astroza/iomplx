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

#include <iomplx.h>
#include <sys/types.h>
#define _GNU_SOURCE
#include <sys/socket.h>
#include <errno.h>
extern int errno;

/*
 * Edge-Triggered (ET) VS Level-Triggered (LT)
 *
 * Debido a que todos los hilos comparten la misma interfaz epoll, LT provocaria que el mismo evento
 * se repitiese en otros hilos, es por eso que uso ET.
 */

void uqueue_init(uqueue *q)
{
	q->epoll_iface = epoll_create(EPOLL_QUEUE_SIZE);
}

void uqueue_event_init(iomplx_event *ev)
{
	ev->data.events_count = 0;
	ev->data.current_event = 0;
	ev->max_events = EVENTS;
}

int uqueue_event_get(uqueue *q, iomplx_event *ev, int timeout)
{
	struct epoll_event *ee;
	int wait_ret;

	ev->data.current_event++;

	if(ev->data.events_count <= ev->data.current_event) {
		do
			wait_ret = epoll_wait(q->epoll_iface, ev->data.events, ev->max_events, timeout*1000);
		while(wait_ret == -1 && errno == EINTR);
		
		if(wait_ret == 0)
			return -1;

		ev->data.events_count = wait_ret;
		ev->data.current_event = 0;
	}

	ee = ev->data.events + ev->data.current_event;
	if(ee->events & (EPOLLRDHUP|EPOLLERR|EPOLLHUP))
		ev->type = IOMPLX_CLOSE_EVENT;
	else if(ee->events & EPOLLIN)
		ev->type = IOMPLX_READ_EVENT;
	else if(ee->events & EPOLLOUT)
		ev->type = IOMPLX_WRITE_EVENT;
	ev->item = ee->data.ptr;

	return ev->data.events_count - ev->data.current_event - 1;
}

void uqueue_watch(uqueue *q, iomplx_item *item)
{
	struct epoll_event ev;

	ev.data.ptr = item;
	ev.events = EPOLLRDHUP|EPOLLET|item->new_filter;
	if(item->oneshot)
		ev.events |= EPOLLONESHOT;
	epoll_ctl(q->epoll_iface, EPOLL_CTL_ADD, item->fd, &ev);
	item->filter = item->new_filter;
	item->new_filter = -1;
}

void uqueue_unwatch(uqueue *q, iomplx_item *item)
{
	epoll_ctl(q->epoll_iface, EPOLL_CTL_DEL, item->fd, NULL);
}

void uqueue_activate(uqueue *q, iomplx_item *item)
{
	item->new_filter = item->filter;
	uqueue_filter_set(q, item);
}

void uqueue_filter_set(uqueue *q, iomplx_item *item)
{
	struct epoll_event ev;

	ev.data.ptr = item;
	ev.events = EPOLLRDHUP|EPOLLET|item->new_filter;
	if(item->oneshot)
		ev.events |= EPOLLONESHOT;
	epoll_ctl(q->epoll_iface, EPOLL_CTL_MOD, item->fd, &ev);
	item->filter = item->new_filter;
	item->new_filter = -1;
}

int accept_and_set(int sockfd, struct sockaddr *sa, unsigned int *sa_size)
{
	return accept4(sockfd, sa, sa_size, SOCK_NONBLOCK);
}
