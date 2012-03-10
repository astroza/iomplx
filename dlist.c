/*
 * IOMPLX
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

#include "dlist.h"

void dlist_append(dlist *list, dlist_node *node)
{
	node->prev = list->tail;
	node->next = NULL;

	if(!list->tail)
		list->head = node;
	else
		list->tail->next = node;

	list->tail = node;
}

void dlist_del(dlist *list, dlist_node *node)
{
	if(node->prev)
		node->prev->next = node->next;
	if(node->next)
		node->next->prev = node->prev;

	if(list->head == node)
		list->head = node->next;
	if(list->tail == node)
		list->tail = node->prev;
}
