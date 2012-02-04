#ifndef __MEMPOOL_H__
#define __MEMPOOL_H__

#include "dlist.h"

typedef struct {
	DLIST_NODE(node);
	char data_addr[];
} mempool_block_header;

typedef struct {
	DLIST(nodes);
	void *pool;
} mempool_instance;

void mempool_init(mempool_instance *, unsigned int, unsigned int);
void *mempool_alloc(mempool_instance *);
void mempool_free(mempool_instance *, void *);
void mempool_destroy(mempool_instance *);

#endif
