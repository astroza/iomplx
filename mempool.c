#include <stdlib.h>
#include "mempool.h"

void mempool_init(mempool_instance *mempool, unsigned int obj_size, unsigned int pool_size)
{
	void *block;
	int i;

	/* Block: [Header, data]
	 * Memory pool: [Block.0, Block.1, Block.2, ..., Block.pool_size-1]
	 */
	/* In the future it could use mmap() instead malloc() */
	mempool->pool = malloc((sizeof(mempool_block_header) + obj_size) * pool_size);
	DLIST_INIT(mempool);
	for(block = mempool->pool, i = 0; i < pool_size; i++) {
		DLIST_APPEND(mempool, block);
		block += sizeof(mempool_block_header) + obj_size;
	}
}

void *mempool_alloc(mempool_instance *mempool)
{
	void *block;

	block = DLIST_TAIL(mempool);
	if(!block)
		return NULL;

	DLIST_DEL(mempool, block);
	return ((mempool_block_header *)block)->data_addr;
}

void mempool_free(mempool_instance *mempool, void *obj)
{
	void *block;

	block = obj - sizeof(mempool_block_header);
	DLIST_APPEND(mempool, block);
}

void mempool_destroy(mempool_instance *mempool)
{
	free(mempool->pool);
	mempool->pool = NULL;
}
