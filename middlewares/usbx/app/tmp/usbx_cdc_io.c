#include "ux_api.h"
#include "ux_device_class_cdc_acm.h"
#include "usbx_cdc_io.h"

/* 在 usbx_device_init.c 中的 activate 回调里赋值 */
extern UX_SLAVE_CLASS_CDC_ACM *cdc_acm_instance;

UINT usb_cdc_write(const UCHAR *data, ULONG length)
{
    if (cdc_acm_instance == UX_NULL) return UX_ERROR;

    ULONG actual = 0;
    return ux_device_class_cdc_acm_write_run(cdc_acm_instance,
                                         (UCHAR *)data,
                                         length,
                                         &actual);
}

UINT usb_cdc_read(UCHAR *buffer, ULONG buffer_size, ULONG *actual_length)
{
    if (cdc_acm_instance == UX_NULL) return UX_ERROR;

    return ux_device_class_cdc_acm_read_run(cdc_acm_instance,
                                        buffer,
                                        buffer_size,
                                        actual_length);
}

