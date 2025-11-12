#include "osal_mem.h"
#include "tx_api.h"
#include <string.h>

static TX_BYTE_POOL os_byte_pool;
static UCHAR os_mem_pool[20*1024]; /* 自定义内存池大小 */

void osal_mem_init(void)
{
	tx_byte_pool_create(&os_byte_pool,"OSAL_MEM_POOL", os_mem_pool, sizeof(os_mem_pool));	
}

void *osal_malloc(size_t size)
{
	void *ptr = NULL;
	if(tx_byte_allocate(&os_byte_pool, (VOID**)&ptr, size, TX_NO_WAIT) == TX_SUCCESS) {
		return ptr;
	}
	return NULL;
}

void osal_free(void *ptr)
{
	tx_byte_release(ptr);
}

void *osal_calloc(size_t n, size_t size)
{
	void *ptr = osal_malloc(n * size);
	if(ptr) {
		memset(ptr, 0, n * size);
	}
	return ptr;
}

void *soal_realloc(void *ptr, size_t new_size)
{
	void *new_ptr = osal_malloc(new_size);
	if(new_ptr & ptr) {
		memcpy(new_ptr, ptr, new_size);
		osal_free(ptr);
	}
	return new_ptr;
}

