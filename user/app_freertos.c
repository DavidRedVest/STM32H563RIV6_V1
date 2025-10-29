#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

#include "main.h"
#include "bsp_led.h"
#include "bsp_lcd.h"
#include "device_manager.h"
#include "draw.h"
#include <stdio.h>

#include "ux_api.h"

static void StartDefaultTask(void *argument);
//static void SPILCDTask(void *argument);

extern void USBX_CDC_Task(void *argument);

//栈溢出检测钩子函数
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    /* 用户自定义处理，比如打印、断言或重启 */
  
    /* 简单处理：进入死循环 */
   (void)xTask;
    (void)pcTaskName;
    while(1);
}

#if 0
extern UX_SLAVE_CLASS_CDC_ACM *cdc_acm_instance;

void USBX_CDC_Task(void *argument)
{
    UCHAR buffer[128];
    ULONG actual_length;

  //  printf("USBX CDC Task started.\r\n");

    while (1)
    {
        if (usb_cdc_read(buffer, sizeof(buffer), &actual_length) == UX_SUCCESS)
        {
            if (actual_length > 0)
            {
                /* 回显 */
                usb_cdc_write(buffer, actual_length);

                /* 控制台输出调试信息 */
                buffer[actual_length] = '\0';
                printf("[USB RX] %s\r\n", buffer);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
#endif

#if 0
static void CH1_UART2_TxTaskFunction( void *pvParameters )	
{	
		uint8_t c = 0;
		struct Dev_Mgmt *pdev = GetUARTDevice("uart2");
		
		pdev->Init(pdev, 115200, 'N', 8, 1);
		
		while (1)
		{
			/* send data */
			pdev->Send(pdev, &c, 1, 100);
			vTaskDelay(500);
			c++;
		}
}

static void CH2_UART4_RxTaskFunction( void *pvParameters )	
{
		uint8_t c = 0;
		int cnt = 0;
		char buf[50];
		int err;

		struct Dev_Mgmt *pdev = GetUARTDevice("uart4");

		pdev->Init(pdev, 115200, 'N', 8, 1);
				
		while (1)
		{
		
			/* 接收数据 */
			err = pdev->RecvByte(pdev, &c, 200);
			
			if (err == 0)		
			{
				sprintf(buf, "Recv : 0x%02x, Cnt : %d", c, cnt++);
				Draw_String(0, 0, buf, 0x0000ff00, 0);
			}
			else
			{
				//HAL_UART_DMAStop(&huart4);
			}
		}
}
#endif


void USBX_Core_Task(void *argument)
{

	while(1)
	{
		ux_system_tasks_run();			  // ★ Standalone 必须周期驱动
		//vTaskDelay(pdMS_TO_TICKS(1));	  // 1~5ms 皆可

	}
}


void MX_FREERTOS_Init(void) {

	BaseType_t ret;

	ret = xTaskCreate(
	  StartDefaultTask,
	  "StartDefaultTask",
	  200,
	  NULL,
	  configMAX_PRIORITIES - 1,
	  NULL);
	if (ret != pdPASS)
	{
		rt_kprintf("StartDefaultTask failed! \r\n");
	    Draw_String(0, 0, "StartDefaultTask failed!", 0x0000ff00, 0);
		Error_Handler();
	}
#if 0
	ret = xTaskCreate(
	  SPILCDTask,
	  "SPILCDTask",
	  200,
	  NULL,
	  configMAX_PRIORITIES - 1,
	  NULL);	
	if (ret != pdPASS)
	{
		rt_kprintf("SPILCDTask failed! \r\n");
		Draw_String(0, 0, "SPILCDTask failed!", 0x0000ff00, 0);
		Error_Handler();
	}

	ret = xTaskCreate(
		CH1_UART2_TxTaskFunction,
		"ch1_uart2_tx_task",
		200,
		NULL,
		configMAX_PRIORITIES - 1,
		NULL);
	if (ret != pdPASS)
	{
		rt_kprintf("CH1_UART2_TxTaskFunction failed! \r\n");
		Draw_String(0, 0, "CH1_UART2_TxTaskFunction failed!", 0x0000ff00, 0);
		Error_Handler();
	}


	ret = xTaskCreate(
		CH2_UART4_RxTaskFunction,
		"ch2_uart4_rx_task",
		200,
		NULL,
		configMAX_PRIORITIES - 1,
		NULL);	
	if (ret != pdPASS)
	{
		rt_kprintf("CH2_UART4_RxTaskFunction failed! \r\n");
		Draw_String(0, 0, "CH2_UART4_RxTaskFunction failed!", 0x0000ff00, 0);
		Error_Handler();
	}
#endif
#if 0
	ret = xTaskCreate(
			USBX_CDC_Task,
			"USBX_CDC_Task",
			1024,
			NULL,
			configMAX_PRIORITIES - 1,
			NULL);	
		if (ret != pdPASS)
		{
			rt_kprintf("USBX_CDC_Task failed! \r\n");
			Draw_String(0, 0, "USBX_CDC_Task failed!", 0x0000ff00, 0);
			Error_Handler();
		} 
#endif
		ret = xTaskCreate(
			USBX_Core_Task,
			"USBX_Core_Task",
			512,
			NULL,
			configMAX_PRIORITIES - 2,
			NULL);

}

void StartDefaultTask(void *argument)
{
	while(1)
	{
		bsp_led_toggle();
		vTaskDelay(500);
	}
}

#if 0
void SPILCDTask(void *argument)
{

	while(1)
	{
		vTaskDelay(500);
	}	
}
#endif


