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

void uqueue_event_init(iomplx_waiter *waiter)
{
	waiter->data.events_count = 0;
	waiter->data.current_event = 0;
	waiter->max_events = EVENTS;
}

int uqueue_event_get(uqueue *q, iomplx_waiter *waiter, int timeout)
{
	struct epoll_event *ee;
	int wait_ret;

	waiter->data.current_event++;

	if(waiter->data.events_count <= waiter->data.current_event) {
		do
			wait_ret = epoll_wait(q->epoll_iface, waiter->data.events, waiter->max_events, timeout*1000);
		while(wait_ret == -1 && errno == EINTR);
		
		if(wait_ret == 0)
			return -1;

		waiter->data.events_count = wait_ret;
		waiter->data.current_event = 0;
	}

	ee = waiter->data.events + waiter->data.current_event;
	if(ee->events & (EPOLLRDHUP|EPOLLERR|EPOLLHUP))
		waiter->type = IOMPLX_CLOSE_EVENT;
	else if(ee->events & EPOLLIN)
		waiter->type = IOMPLX_READ_EVENT;
	else if(ee->events & EPOLLOUT)
		waiter->type = IOMPLX_WRITE_EVENT;
	waiter->item = ee->data.ptr;

	return waiter->data.events_count - waiter->data.current_event - 1;
}

int uqueue_watch(uqueue *q, iomplx_item *item)
{
	struct epoll_event ev;
	int ret;

	ev.data.ptr = item;
	ev.events = EPOLLRDHUP|EPOLLET|item->filter;
	if(item->oneshot)
		ev.events |= EPOLLONESHOT;
	ret = epoll_ctl(q->epoll_iface, EPOLL_CTL_ADD, item->fd, &ev);
	item->applied_filter = item->filter;
	return ret;
}

int uqueue_unwatch(uqueue *q, iomplx_item *item)
{
	return epoll_ctl(q->epoll_iface, EPOLL_CTL_DEL, item->fd, NULL);
}

int uqueue_enable(uqueue *q, iomplx_item *item)
{
	return uqueue_filter_set(q, item);
}

int uqueue_filter_set(uqueue *q, iomplx_item *item)
{
	struct epoll_event ev;
	int ret;

	ev.data.ptr = item;
	ev.events = EPOLLRDHUP|EPOLLET|item->filter;
	if(item->oneshot)
		ev.events |= EPOLLONESHOT;
	ret = epoll_ctl(q->epoll_iface, EPOLL_CTL_MOD, item->fd, &ev);
	item->applied_filter = item->filter;
	return ret;
}

int accept_and_set(int sockfd, struct sockaddr *sa, unsigned int *sa_size)
{
	return accept4(sockfd, sa, sa_size, SOCK_NONBLOCK);
}
