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


static void StartDefaultTask(void *argument);
static void SPILCDTask(void *argument);


//栈溢出检测钩子函数
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    /* 用户自定义处理，比如打印、断言或重启 */
  
    /* 简单处理：进入死循环 */
   (void)xTask;
    (void)pcTaskName;
    while(1);
}


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

}

void StartDefaultTask(void *argument)
{
	while(1)
	{
		bsp_led_toggle();
		vTaskDelay(500);
	}
}

void SPILCDTask(void *argument)
{

	while(1)
	{
		vTaskDelay(500);
	}	
}



