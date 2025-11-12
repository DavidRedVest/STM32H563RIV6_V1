#ifndef __OSAL_COMMON_H__
#define __OSAL_COMMON_H__


#ifdef _WIN32
  #define OSAL_API __declspec(dllexport)
#else
  #define OSAL_API __attribute__((visibility("default")))
#endif


#ifdef __cplusplus
  #define OSAL_EXTERN_C_BEGIN  extern "C" {
  #define OSAL_EXTERN_C_END    }
#else
  #define OSAL_EXTERN_C_BEGIN
  #define OSAL_EXTERN_C_END
#endif

#endif

