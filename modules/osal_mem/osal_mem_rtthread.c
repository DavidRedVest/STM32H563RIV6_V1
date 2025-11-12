#include "osal_mem.h"
#include <rtthread.h>
#include <string.h>

void *osal_malloc(size_t size)
{
	return rt_malloc(size);
}
void osal_free(void *ptr)
{
	rt_free(ptr);
}
void *osal_calloc(size_t n, size_t size)
{
	void *ptr = rt_malloc(n*size);
	if(ptr) {
		memset(ptr, 0, n * size);
	}
	return ptr;
}
void *osal_realloc(void *ptr, size_t new_size)
{
	return rt_realloc(ptr, new_size);
}


