#include "FreeRTOS.h"
#include "task.h"
#include "usbx_cdc_io.h"
#include "ux_api.h"
#include <stdio.h>
#include "ux_device_class_cdc_acm.h"
#include "main.h"

extern UX_SLAVE_CLASS_CDC_ACM *cdc_acm_instance;

void USBX_CDC_Task(void *argument)
{
    UCHAR buf[128];
    ULONG n = 0;
	
	rt_kprintf("USBX CDC task 111.\r\n");

    /* 可选：等待 CDC 就绪（已被主机枚举并 activate） */
    while (cdc_acm_instance == UX_NULL) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    rt_kprintf("USBX CDC task started.\r\n");

    for (;;)
    {
    #if 1
        if (usb_cdc_read(buf, sizeof(buf), &n) == UX_SUCCESS && n > 0)
        {
            /* 回显 */
            usb_cdc_write(buf, n);

            /* 调试打印（注意串口冲突时请关闭或改为 RTT） */
            rt_kprintf("[USB RX %lu]: ", n);
            //for (ULONG i = 0; i < n; ++i) putchar(buf[i]);
            //putchar('\n');
        }

        vTaskDelay(pdMS_TO_TICKS(5));
#endif
	

    }
}

