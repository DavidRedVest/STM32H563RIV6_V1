#include "osal_mem.h"
#include <stdlib.h>
#include <string.h>

void *osal_malloc(size_t size)
{
	return malloc(size);
}
void osal_free(void *ptr)
{
	free(ptr);
}

void *osal_calloc(size_t n, size_t size)
{
	return calloc(n, size);
}
void *osal_realloc(void *ptr, size_t new_size)
{
	return realloc(ptr, new_size);
}

