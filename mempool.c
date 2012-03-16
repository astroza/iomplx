#include <stdlib.h>
#include "mempool.h"

void *mempool_init(mempool_instance *mempool, unsigned int obj_size, unsigned int pool_size)
{
	void *block;

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
	mempool_block_header *hdr;

	hdr = DLIST_TAIL(mempool);
	if(!hdr)
		return NULL;

	DLIST_DEL(hdr);
	return hdr->data_addr;
}

void mempool_free(mempool_instance *mempool, void *obj)
{
	mempool_block_header *hdr;

	hdr = obj - sizeof(mempool_block_header);
	DLIST_APPEND(mempool, hdr);
}

void mempool_destroy(mempool_instance *mempool)
{
	free(mempool->pool);
	mempool->pool = NULL;
}
