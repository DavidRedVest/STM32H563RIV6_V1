#include "osal_mem.h"
#include "FreeRTOS.h"
#include "task.h"
#include <string.h>

void *osal_malloc(size_t size)
{
	return pvPortMalloc(size);
}
void osal_free(void *ptr)
{
	vPortFree(ptr);
}
void *osal_calloc(size_t n, size_t size)
{
	void *ptr = pvPortMalloc(n * size);
	if(ptr) {
		memset(ptr, 0, n * size);
	}
	return ptr;
}
void *osal_realloc(void *ptr, size_t new_size)
{
	void *new_ptr = pvPortMalloc(new_size);
	if(new_size && ptr) {
		memcpy(new_ptr, ptr, new_size);
		vPortFree(ptr);
	}
	return new_ptr;
}

