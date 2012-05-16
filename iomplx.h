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

#ifndef IOMPLX_H
#include <dlist.h>
#include <mempool.h>
#include <sys/socket.h>
#include <time.h>

#define IOMPLX_MAX_ACTIVE_ITEMS 	200
#define IOMPLX_MAINTENANCE_PERIOD 	4	/* Seconds */

#define IOMPLX_READ_EVENT	0
#define IOMPLX_WRITE_EVENT	1
#define IOMPLX_TIMEOUT_EVENT	2
#define IOMPLX_CLOSE_EVENT	3
#define IOMPLX_ACCEPT_EVENT	4
#define IOMPLX_CONNECT_EVENT	4

#define IOMPLX_ITEM_WOULDBLOCK	-2
#define IOMPLX_ITEM_CLOSE	-1

typedef struct _iomplx_item iomplx_item;

typedef int (*ev_call1)(iomplx_item *);
typedef void *(*alloc_func)(unsigned long);
typedef void (*free_func)(void *);
typedef void (*init_func)();

typedef union {
	struct {
		struct {
			ev_call1 ev_read;
			ev_call1 ev_write;
			ev_call1 ev_timeout;
			ev_call1 ev_close;
		};
		union {
			ev_call1 ev_connect;
			ev_call1 ev_accept;
		};
	};
	ev_call1 calls_arr[5];
} iomplx_callbacks;

struct _iomplx_item {
	DLIST_NODE(item);
	int fd;
	int filter;
	int applied_filter;
	unsigned char oneshot:1;
	unsigned char disabled:1;
	unsigned char active:1;
	unsigned char closed:1;

	union {
		iomplx_callbacks cb;
		struct {
			alloc_func item_alloc;
			free_func item_free;
		};
	};

	struct {
		unsigned char high:1;
		unsigned char stage:2;
		unsigned int time_limit;
		unsigned int start_time;
	} timeout;

	struct _iomplx_item *parent;
	void *data;

	union {
		struct {
			struct sockaddr sa;
			unsigned int sa_size;
		};
	};
};

#include "glue.h"

typedef struct {
	uqueue_events data;

	unsigned int max_events;
	iomplx_item *item;
	unsigned char type;
} iomplx_waiter;

typedef struct {
	uqueue n_uqueue;
	uqueue accept_uqueue;
	unsigned int threads;
#define THREAD_0 0
#define THREAD_N 1
	unsigned int active_list_size[2];
	init_func thread_init;
	int recycler[2];
	iomplx_item recycler_item;
	dlist items_to_check;
} iomplx_instance;

typedef struct {
	DLIST_NODE(node);
	iomplx_item *item;
	unsigned char call_idx;
} iomplx_item_call;

typedef struct {
	dlist item_calls_list;
	mempool_instance item_calls_pool;
	unsigned int available_item_calls;
} iomplx_active_list;

#define IOMPLX_ITEMS_DUMP_MAX_SIZE 	UQUEUE_MAX_EVENTS

typedef struct {
	void *items[IOMPLX_ITEMS_DUMP_MAX_SIZE];
	unsigned int size;
} iomplx_items_dump;

#define IOMPLX_READ	UQUEUE_READ_EVENT
#define IOMPLX_WRITE	UQUEUE_WRITE_EVENT
#define IOMPLX_NONE	UQUEUE_NONE

void uqueue_init(uqueue *);
void uqueue_event_init(iomplx_waiter *);
int uqueue_event_get(uqueue *, iomplx_waiter *, int);
int uqueue_watch(uqueue *, iomplx_item *);
int uqueue_unwatch(uqueue *, iomplx_item *);
int uqueue_enable(uqueue *q, iomplx_item *item);
int uqueue_filter_set(uqueue *, iomplx_item *);
int accept_and_set(int, struct sockaddr *, unsigned int *);
void iomplx_callbacks_init(iomplx_item *);

void iomplx_item_add(iomplx_instance *, iomplx_item *, int);

void iomplx_init(iomplx_instance *, init_func, unsigned int);
void iomplx_launch(iomplx_instance *);

void iomplx_active_list_init(iomplx_active_list *, unsigned int);
int iomplx_active_list_call_add(iomplx_active_list *, iomplx_item *, unsigned char);
void iomplx_active_list_call_del(iomplx_active_list *, iomplx_item_call *);
void iomplx_active_list_populate(iomplx_active_list *, uqueue *, int);
void iomplx_items_recycle(iomplx_instance *, iomplx_items_dump *);
void iomplx_item_throw_away(iomplx_instance *, iomplx_items_dump *, iomplx_item *);

/* Main functions */
void iomplx_thread_0(iomplx_instance *);
void iomplx_thread_N(iomplx_instance *);
void iomplx_thread_T(iomplx_instance *);

static inline void iomplx_item_filter_set(iomplx_item *item, int filter)
{
        item->filter = filter;
}

static inline void iomplx_item_timeout_set(iomplx_item *item, unsigned long time_limit)
{
	item->timeout.start_time = time(NULL);
	item->timeout.time_limit = time_limit;
}

static inline void iomplx_items_dump_init(iomplx_items_dump *items_dump)
{
	items_dump->size = 0;
}

static inline void uqueue_max_events_set(iomplx_waiter *waiter, int max_events)
{ 
	if(max_events > UQUEUE_MAX_EVENTS)
		waiter->max_events = UQUEUE_MAX_EVENTS;
	else
		waiter->max_events = max_events;  
}

#endif
