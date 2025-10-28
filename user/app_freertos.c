#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "bsp_led.h"
#include "bsp_lcd.h"

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


void MX_FREERTOS_Init(void) {

	xTaskCreate(
	  StartDefaultTask,
	  "StartDefaultTask",
	  400,
	  NULL,
	  configMAX_PRIORITIES - 1,
	  NULL);
	
	xTaskCreate(
	  SPILCDTask,
	  "SPILCDTask",
	  400,
	  NULL,
	  configMAX_PRIORITIES - 1,
	  NULL);	

}

void StartDefaultTask(void *argument)
{
	unsigned int cnt = 0;
	while(1)
	{
		rt_kprintf("StartDefaultTask run:%d \r\n",cnt++);
		bsp_led_toggle();
		//vTaskDelay(500);
		HAL_Delay(500);

	}
}

void SPILCDTask(void *argument)
{
	unsigned int cnt = 0;

	bsp_test_lcd();

	while(1)
	{
		rt_kprintf("SPILCDTask run:%d \r\n",cnt++);
		vTaskDelay(500);
	}
}



