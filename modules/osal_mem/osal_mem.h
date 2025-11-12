#ifndef __OSAL_MEM_H__
#define __OSAL_MEM_H__

#include <stddef.h>

#include "osal_common.h"

OSAL_EXTERN_C_BEGIN

/* 通用接口 */
void *osal_malloc(size_t size);
void osal_free(void *ptr);
void *osal_calloc(size_t n, size_t size);
void *osal_realloc(void *ptr, size_t new_size);

OSAL_EXTERN_C_END

#endif

