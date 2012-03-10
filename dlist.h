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

#ifndef DLIST_H
#define DLIST_H

typedef struct _dlist_node {
	struct _dlist_node *next, *prev;
} dlist_node; 

typedef struct {
	dlist_node *head, *tail;
} dlist;

static inline int __copy_dlist_node(dlist_node *dst, dlist_node *src)
{
	*dst = *src;
	return 1;
}

#ifndef NULL
#define NULL ((void *)0)
#endif

#define DLIST_INIT(list)		((dlist *)list)->head = NULL; ((dlist *)list)->tail = NULL
#define DLIST_APPEND(list, node)	dlist_append((dlist *)(list), (dlist_node *)(node))
#define DLIST_DEL(list, node)		dlist_del((dlist *)(list), (dlist_node *)(node))
#define DLIST_NODE_INIT(a)
#define DLIST(name)			dlist name##_list
#define DLIST_NODE(name)		dlist_node name##_node
#define AS ,
#define RCOPY_ID(line) node_copy_ ## line
#define COPY_ID(line) RCOPY_ID(line)
#define _FOREACH(node_copy, list, node)	for(node = (void *)((dlist *)list)->head; \
					node != NULL && __copy_dlist_node(&node_copy, (dlist_node *)node); \
					node = (void *)node_copy.next)
#define DLIST_FOREACH(exp)		dlist_node COPY_ID(__LINE__); _FOREACH(COPY_ID(__LINE__), exp)

void dlist_append(dlist *, dlist_node *);
void dlist_del(dlist *, dlist_node *);

#endif
