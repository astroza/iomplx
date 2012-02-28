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

#ifndef MPLX3_H
#include <dlist.h>
#include <sys/socket.h>

#define MPLX3_CONF_BACKLOG	10

#define MPLX3_RECEIVE_EVENT	1
#define MPLX3_SEND_EVENT	2
#define MPLX3_TIMEOUT_EVENT	3
#define MPLX3_DISCONNECT_EVENT	4

typedef struct _mplx3_endpoint mplx3_endpoint;

typedef int (*ev_call1)(mplx3_endpoint *);
typedef void *(*alloc_func)(unsigned long);
typedef void (*free_func)(void *);

typedef union {
	struct {
		union {
			ev_call1 ev_connect;
			ev_call1 ev_accept;
		};
		ev_call1 ev_receive;
		ev_call1 ev_send;
		ev_call1 ev_timeout;
		ev_call1 ev_disconnect;
	};
	ev_call1 calls_arr[5];
} mplx3_callbacks;

struct _mplx3_endpoint {
	DLIST_NODE(endpoint);
	int sockfd;
	int new_filter;
	unsigned int sa_size;
	struct sockaddr sa;
	mplx3_callbacks cb;
	int timeout;
	int elapsed_time;
	void *data;
};

typedef struct {
	DLIST(endpoints);
	int timeout_granularity;
} mplx3_monitor;

#include "glue.h"

typedef struct {
	uqueue_events data;
	unsigned char type;
	mplx3_endpoint *ep;
} mplx3_event;

typedef struct {
	uqueue n_queue;
	uqueue accept_queue;
	int threads;
	mplx3_monitor monitor;
	alloc_func ep_alloc;
	free_func ep_free;
} mplx3_multiplexer;

#define MPLX3_RECEIVE	UQUEUE_RECEIVE_EVENT
#define MPLX3_SEND	UQUEUE_SEND_EVENT

void uqueue_init(uqueue *);
void uqueue_event_init(mplx3_event *);
int uqueue_wait(uqueue *, mplx3_event *, int);
void uqueue_watch(uqueue *, mplx3_endpoint *);
void uqueue_unwatch(uqueue *, mplx3_endpoint *);
void uqueue_filter_set(uqueue *, mplx3_endpoint *);
int accept_and_set(int, struct sockaddr *, unsigned int *);

int mplx3_listen(mplx3_multiplexer *, const char *, unsigned short, ev_call1, void *);
int mplx3_connect(const char *, unsigned short, mplx3_callbacks *, void *);
void mplx3_filter_set(mplx3_endpoint *, int);
void mplx3_specialize();

void mplx3_init(mplx3_multiplexer *, alloc_func, free_func, unsigned int, unsigned int);
void mplx3_launch(mplx3_multiplexer *);

#endif
