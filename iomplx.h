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

#define IOMPLX_MAX_ACTIVE_ITEMS 200

#define IOMPLX_READ_EVENT	1
#define IOMPLX_WRITE_EVENT	2
#define IOMPLX_TIMEOUT_EVENT	3
#define IOMPLX_CLOSE_EVENT	4

#define IOMPLX_ITEM_CALLS	EVENTS

#define IOMPLX_ITEM_WOULDBLOCK	-2
#define IOMPLX_ITEM_CLOSE	-1

typedef struct _iomplx_item iomplx_item;

typedef int (*ev_call1)(iomplx_item *);
typedef void *(*alloc_func)(unsigned long);
typedef void (*free_func)(void *);
typedef void (*init_func)();

typedef union {
	struct {
		union {
			ev_call1 ev_connect;
			ev_call1 ev_accept;
		};
		ev_call1 ev_read;
		ev_call1 ev_write;
		ev_call1 ev_timeout;
		ev_call1 ev_close;
	};
	ev_call1 calls_arr[5];
} iomplx_callbacks;

struct _iomplx_item {
	DLIST_NODE(item);
	int fd;
	int filter;
	int new_filter;
	unsigned char oneshot;
	iomplx_callbacks cb;
	int timeout;
	int elapsed_time;
	int active;
	void *data;
	union {
		struct {
			struct sockaddr sa;
			unsigned int sa_size;
		};
	};
};

typedef struct {
	DLIST(items);
	int timeout_granularity;
} iomplx_monitor;

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
	iomplx_monitor monitor;
	alloc_func item_alloc;
	free_func item_free;
	init_func thread_init;
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

#define IOMPLX_READ	UQUEUE_READ_EVENT
#define IOMPLX_WRITE	UQUEUE_WRITE_EVENT
#define IOMPLX_NONE	UQUEUE_NONE

void uqueue_init(uqueue *);
void uqueue_event_init(iomplx_waiter *);
int uqueue_event_get(uqueue *, iomplx_waiter *, int);
void uqueue_watch(uqueue *, iomplx_item *);
void uqueue_unwatch(uqueue *, iomplx_item *);
void uqueue_rewatch(uqueue *q, iomplx_item *item);
void uqueue_filter_set(uqueue *, iomplx_item *);
int accept_and_set(int, struct sockaddr *, unsigned int *);
void iomplx_callbacks_init(iomplx_item *);

iomplx_item *iomplx_item_add(iomplx_instance *, iomplx_item *, int);

void iomplx_init(iomplx_instance *, alloc_func, free_func, init_func, unsigned int, unsigned int);
void iomplx_launch(iomplx_instance *);

static inline void iomplx_item_filter_set(iomplx_item *item, int filter)
{
        item->new_filter = filter;
}

static inline int iomplx_timeout_tryset(iomplx_item *item)
{
	return __sync_bool_compare_and_swap(&item->active, 0, -1);
}

static inline int iomplx_active_set(iomplx_item *item)
{
	return __sync_bool_compare_and_swap(&item->active, 0, 1);
}

static inline int iomplx_active_unset(iomplx_item *item)
{
	return __sync_bool_compare_and_swap(&item->active, 1, 0);
}

#endif
