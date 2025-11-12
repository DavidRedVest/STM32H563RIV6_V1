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

#include "modbus.h"
#include "osal_mem.h"
#include "errno.h"

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

void SPILCDTask(void *argument)
{
	uint32_t cnt = 0;

	while(1)
	{
		vTaskDelay(500);
		rt_kprintf("SPILCDTask start:%d! \r\n",cnt++);

	}	
}
#endif


static void USBX_Core_Task(void *argument)
{

	while(1)
	{
		ux_system_tasks_run();			  // ★ Standalone 必须周期驱动
		vTaskDelay(pdMS_TO_TICKS(1));	  // 1~5ms 皆可

	}
}

static void LibmodbusServer(void *argument)
{
//	rt_kprintf("start 111 \r\n");

	uint8_t *query;
	modbus_t *ctx;
	int rc;
	modbus_mapping_t *mb_mapping;

	ctx = modbus_new_rtu("usb",115200,'N', 8, 1);
//	rt_kprintf("start 222 \r\n");
	modbus_set_slave(ctx, 1);
	query = osal_malloc(MODBUS_RTU_MAX_ADU_LENGTH);

	mb_mapping = modbus_mapping_new_start_address(0,10,
		0,10,
		0,10,
		0,10);

	memset(mb_mapping->tab_bits, 0, mb_mapping->nb_bits);
	memset(mb_mapping->tab_registers, 0x55, mb_mapping->nb_registers * 2);
	
	rc = modbus_connect(ctx);
	if(-1 == rc) {
		modbus_free(ctx);
		vTaskDelete(NULL);
	}
	//rt_kprintf("start 333 \r\n");

	for(;;)
	{
#if 1	
		do {
//			rt_kprintf("start ready \r\n");
			rc = modbus_receive(ctx, query);
		}while(0 == rc);

		if( -1 == rc && errno != EMBBADCRC ) {
			continue;
		}
		//rt_kprintf("start 444 \r\n");

		rc = modbus_reply(ctx, query, rc, mb_mapping);
		if(-1 == rc) {
			//break;
		}
		//rt_kprintf("start 555 \r\n");

		/* 测试功能，实现电灯 */
		if (mb_mapping->tab_bits[0]) {
			HAL_GPIO_WritePin(GPIOC, GPIO_PIN_12, GPIO_PIN_RESET);
		} else {
			HAL_GPIO_WritePin(GPIOC, GPIO_PIN_12, GPIO_PIN_SET);
		}
#endif		
		vTaskDelay(100);

	}

	modbus_mapping_free(mb_mapping);
	osal_free(query);
	/*for rtu*/
	modbus_close(ctx);
	modbus_free(ctx);
	vTaskDelete(NULL);
}

void MX_FREERTOS_Init(void) {

	BaseType_t ret;
#if 0
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
#endif
	
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
			configMAX_PRIORITIES - 1,
			NULL);

#if 1
	/* modbus从机实验 */
		ret = xTaskCreate(
			LibmodbusServer,
			"libmobusServer",
			200,
			NULL,
			configMAX_PRIORITIES - 1,
			NULL
		);
#endif
}

void StartDefaultTask(void *argument)
{
	while(1)
	{
		bsp_led_toggle();
		vTaskDelay(500);
	}
}




