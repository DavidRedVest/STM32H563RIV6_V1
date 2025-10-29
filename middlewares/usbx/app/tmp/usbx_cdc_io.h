/* usbx_cdc_io.h */
#ifndef __USBX_CDC_IO_H__
#define __USBX_CDC_IO_H__

#include "ux_api.h"

UINT usb_cdc_write(const UCHAR *data, ULONG length);
UINT usb_cdc_read(UCHAR *buffer, ULONG buffer_size, ULONG *actual_length);

#endif

