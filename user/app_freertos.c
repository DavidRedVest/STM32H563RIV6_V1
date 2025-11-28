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
//#include <errno.h>

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
static void StartDefaultTask(void *argument)
{
	unsigned int cnt = 0;
	while(1)
	{
		bsp_led_toggle();
		//rt_kprintf("StartDefaultTask test:%d \r\n",cnt++);
		vTaskDelay(500);
	}
}
#endif

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

#if 0
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
#endif
#if 0
static void LibmodbusClient( void *pvParameters )	
{
	modbus_t *ctx;
	int rc;
	uint16_t val = 0;
	int nb = 1;

	ctx = modbus_new_rtu("usb", 115200, 'N', 8, 1);
	modbus_set_slave(ctx, 1);

	rc = modbus_connect(ctx);
	if(-1 == rc) {
		modbus_free(ctx);
		vTaskDelete(NULL);
	}


	for(;;)
	{
		/* read hoding register 1 */
		rc = modbus_read_registers(ctx, 1, nb, &val);

		if (rc != nb)
			continue;

		/* display on lcd */
		Draw_Number(0, 0, val, 0xff0000);

		/* val ++ */
		val++;

		/* write val to hoding register 2 */
		rc = modbus_write_registers(ctx, 2, nb, &val);

	}
	/* For RTU */
	modbus_close(ctx);
	modbus_free(ctx);

	vTaskDelete(NULL);
}
#endif

#if 0
static void USBtoRS485( void *pvParameters )	
{
	struct Dev_Mgmt *pUSBUART = GetUARTDevice("usb");
	struct Dev_Mgmt *pRS485UART = GetUARTDevice("uart4");
	uint8_t data;

	while (1)
	{
		if (0 == pUSBUART->RecvByte(pUSBUART, &data, portMAX_DELAY))
		{
			pRS485UART->Send(pRS485UART, &data, 1, 100);
			
		}
	}

}


static void RS485toUSB( void *pvParameters )	
{
	struct Dev_Mgmt *pUSBUART = GetUARTDevice("usb");
    struct Dev_Mgmt *pRS485UART = GetUARTDevice("uart4");
    uint8_t data;

    while (1)
    {
        if (0 == pRS485UART->RecvByte(pRS485UART, &data, portMAX_DELAY))
        {
            pUSBUART->Send(pUSBUART, &data, 1, 100);
        }
    }
}
#endif

#if 1
/* 测试modbus_write_file_record函数功能 */
static void CH1_UART2_ClientTask( void *pvParameters )	
{
	modbus_t *ctx;
	int rc;
	char buf[100] = {0};
	int cnt = 0;
    int err_cnt = 0;
	//ctx = modbus_new_rtu("uart2", 115200, 'N', 8, 1);
	ctx = modbus_new_st_rtu("uart2", 115200, 'N', 8, 1);
	modbus_set_slave(ctx, 1);

// 设置超时时间，避免永久阻塞，设为 1秒
  //  modbus_set_response_timeout(ctx, 1, 0);

	rc = modbus_connect(ctx);
	if(-1 == rc) {
		modbus_free(ctx);
		vTaskDelete(NULL);
	}
/* 等待 Server 任务初始化完成 */
    vTaskDelay(1000);
	for(;;)
	{
		// 1. 手动清零，验证底层是否修改了它
	   // errno = 0;

		memset(buf, 0x5A, sizeof(buf)); // 填充测试数据
		rc = modbus_write_file_record(ctx, 1, 1, (uint8_t *)buf, 100);
		//rc = modbus_write_file_record(ctx, 1, 1, file_data, 100);
		cnt++;

		if(rc < 0) {
			err_cnt++;
			// 打印 errno 的十六进制，方便看是不是内存被踩了
            //sprintf(buf, "ERR c:%d rc:%d err:%d(0x%X)", cnt, rc, errno, errno);
			sprintf(buf, "ERR c:%d rc:%d ", cnt, rc);
			/* 发生错误后，稍微延时久一点，让链路恢复 */
        //vTaskDelay(2000);
		} else {
			sprintf(buf, "OK cnt:%d rc:%d", cnt, rc);
		}
		//sprintf(buf, "rc:%d errno:%d client send file record cnt = %d, err_cnt = %d", rc, errno, cnt, err_cnt);
        Draw_String(0, 0, buf, 0xff0000, 0);
            
		/* delay 2s */
		vTaskDelay(2000);
	}
	/* For RTU */
	modbus_close(ctx);
	modbus_free(ctx);

	vTaskDelete(NULL);
}

static void CH2_UART4_ServerTask( void *pvParameters )	
{
	uint8_t *query;
	modbus_t *ctx;
	int rc;
	modbus_mapping_t *mb_mapping;
	char buf[100];
	int cnt = 0;
	
	//ctx = modbus_new_rtu("uart4", 115200, 'N', 8, 1);
	ctx = modbus_new_st_rtu("uart4", 115200, 'N', 8, 1);
	modbus_set_slave(ctx, 1);
	query = osal_malloc(MODBUS_RTU_MAX_ADU_LENGTH);

	mb_mapping = modbus_mapping_new_start_address(0,
												  10,
												  0,
												  10,
												  0,
												  10,
												  0,
												  10);
	memset(mb_mapping->tab_bits, 0, mb_mapping->nb_bits);
	memset(mb_mapping->tab_registers, 0x55, mb_mapping->nb_registers*2);
									
	rc = modbus_connect(ctx);
	if (rc == -1) {
		//fprintf(stderr, "Unable to connect %s\n", modbus_strerror(errno));
		modbus_free(ctx);
		vTaskDelete(NULL);;
	}

	for (;;) {
		do {
			rc = modbus_receive(ctx, query);
			/* Filtered queries return 0 */
		} while (rc == 0);

		/* The connection is not closed on errors which require on reply such as
		   bad CRC in RTU. */
		if (rc < 0 ) {
			/* Quit */
			continue;
		}

		rc = modbus_reply(ctx, query, rc, mb_mapping);
		if (rc == -1) {
			//break;
		}

		cnt++;
		sprintf(buf, "rc:%d server recv file record cnt = %d",rc, cnt);
        Draw_String(0, 32, buf, 0xff0000, 0);

		if (mb_mapping->tab_bits[0])
			HAL_GPIO_WritePin(GPIOC, GPIO_PIN_12, GPIO_PIN_RESET);
		else
			HAL_GPIO_WritePin(GPIOC, GPIO_PIN_12, GPIO_PIN_SET);

		//vTaskDelay(1000);
		mb_mapping->tab_registers[1]++;
	}
	modbus_mapping_free(mb_mapping);
	vPortFree(query);
	/* For RTU */
	modbus_close(ctx);
	modbus_free(ctx);

	vTaskDelete(NULL);
}
#endif

#if 0
/* * 优化 2: 纯数值操作的大小端转换
 * 比指针操作更快，且无需强制类型转换。
 * 如果使用 GCC/Clang，可以直接用 __builtin_bswap32(val)
 */
static inline uint32_t swap_uint32(uint32_t val)
{
    return ((val & 0x000000FF) << 24) |
           ((val & 0x0000FF00) << 8)  |
           ((val & 0x00FF0000) >> 8)  |
           ((val & 0xFF000000) >> 24);
}
/* 测试modbus_write_file功能 */
static void modbus_parse_file_record(uint8_t *msg, uint16_t msg_len)
{
    uint16_t record_no;
    FileInfo tFileInfo;
    char buf[100];
    static int recv_len = 0;
    
    if (msg[1] == MODBUS_FC_WRITE_FILE_RECORD)
    {
        record_no = ((uint16_t)msg[6]<<8) | msg[7];
        if (record_no == 0)
        {
            tFileInfo = *((PFileInfo)&msg[10]);
            tFileInfo.file_len = swap_uint32(tFileInfo.file_len);
            sprintf(buf, "Get File Record for Head, file len = %ld", tFileInfo.file_len);
            Draw_String(0, 32, buf, 0xff0000, 0);

            recv_len = 0;
        }
        else
        {
            recv_len += msg[2] - 7;
            
            sprintf(buf, "Get File Record %d for Data, record len = %d, recv_len = %d   ", record_no, msg[2] - 7, recv_len);
            Draw_String(0, 64, buf, 0xff0000, 0);            
        }
    }
}


static void CH2_UART4_ServerTask( void *pvParameters )	
{
	uint8_t *query;
	modbus_t *ctx;
	int rc;
	modbus_mapping_t *mb_mapping;
	//char buf[100];
	int cnt = 0;
	
	ctx = modbus_new_rtu("uart4", 115200, 'N', 8, 1);
	modbus_set_slave(ctx, 1);
	query = pvPortMalloc(MODBUS_RTU_MAX_ADU_LENGTH);

	mb_mapping = modbus_mapping_new_start_address(0,
												  10,
												  0,
												  10,
												  0,
												  10,
												  0,
												  10);
	
	memset(mb_mapping->tab_bits, 0, mb_mapping->nb_bits);
	memset(mb_mapping->tab_registers, 0x55, mb_mapping->nb_registers*2);
    
	rc = modbus_connect(ctx);
	if (rc == -1) {
		//fprintf(stderr, "Unable to connect %s\n", modbus_strerror(errno));
		modbus_free(ctx);
		vTaskDelete(NULL);;
	}

	for (;;) {
		do {
			rc = modbus_receive(ctx, query);
			/* Filtered queries return 0 */
		} while (rc == 0);
 
		/* The connection is not closed on errors which require on reply such as
		   bad CRC in RTU. */
		if (rc < 0 ) {
			/* Quit */
			continue;
		}

		rc = modbus_reply(ctx, query, rc, mb_mapping);
		if (rc == -1) {
			//break;
		}

        cnt++;
        
        modbus_parse_file_record(query, rc);

		if (mb_mapping->tab_bits[0])
			HAL_GPIO_WritePin(GPIOC, GPIO_PIN_12, GPIO_PIN_RESET);
		else
			HAL_GPIO_WritePin(GPIOC, GPIO_PIN_12, GPIO_PIN_SET);

		//vTaskDelay(1000);
		mb_mapping->tab_registers[1]++;
	}

	modbus_mapping_free(mb_mapping);
	vPortFree(query);
	/* For RTU */
	modbus_close(ctx);
	modbus_free(ctx);

	vTaskDelete(NULL);
}


static void CH1_UART2_ClientTask( void *pvParameters )	
{
	modbus_t *ctx;
	int rc;
	//uint16_t val;
	//int nb = 1;
	//int level = 1;
	char *buf;
	int cnt = 0;
    int err_cnt = 0;
	
	ctx = modbus_new_rtu("uart2", 115200, 'N', 8, 1);
	modbus_set_slave(ctx, 1);

    buf = osal_malloc(500);
	
	rc = modbus_connect(ctx);
	if (rc == -1) {
		//fprintf(stderr, "Unable to connect %s\n", modbus_strerror(errno));
		modbus_free(ctx);
		vTaskDelete(NULL);;
	}

	for (;;) {
        memset(buf, 0x5A, 500);
        rc = modbus_write_file(ctx, 1, "text.txt", (const unsigned char *)buf, 500);
        cnt++;

        if (rc < 0)
            err_cnt++;

        sprintf(buf, "rc:%d client send file cnt = %d, err_cnt = %d", rc,cnt, err_cnt);
        Draw_String(0, 0, buf, 0xff0000, 0);
            
		/* delay 2s */
		vTaskDelay(2000);
	}

	/* For RTU */
	modbus_close(ctx);
	modbus_free(ctx);

	vTaskDelete(NULL);
}

#endif

void MX_FREERTOS_Init(void) {

	BaseType_t ret;

//	struct Dev_Mgmt *pUSBUART = GetUARTDevice("usb");
//	struct Dev_Mgmt *pRS485UART = GetUARTDevice("uart4");
	
//	pUSBUART->Init(pUSBUART, 115200, 'N', 8, 1);
	//pRS485UART->Init(pRS485UART, 115200, 'N', 8, 1);

	
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

#if 1
		ret = xTaskCreate(
			USBX_Core_Task,
			"USBX_Core_Task",
			512,
			NULL,
			configMAX_PRIORITIES - 1,
			NULL);
		if (ret != pdPASS)
		{
			rt_kprintf("USBX_CDC_Task failed! \r\n");
			Error_Handler();
		}
#endif

#if 0
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

#if 0
	/* modbus主机实验 */
		ret = xTaskCreate(
			LibmodbusClient,
			"LibmodbusClient",
			300,
			NULL,
			configMAX_PRIORITIES - 1,
			NULL
		);
		if (ret != pdPASS)
		{
			rt_kprintf("LibmodbusClient failed! \r\n");
			Error_Handler();
		}

#endif

#if 0
/* H5实现USB转RS485接口，测试从机 */

	xTaskCreate(
		USBtoRS485,
		"USBtoRS485",
		300,
		NULL,
		configMAX_PRIORITIES - 1,
		NULL
	);
	xTaskCreate(
		RS485toUSB,
		"RS485toUSB",
		500,
		NULL,
		configMAX_PRIORITIES - 1,
		NULL
	);
#endif

#if 1
/* 测试modbus_write_file_record的功能
 * 通过CH1_UART2发送数据，CH2_UART4接收数据测试
 */
	xTaskCreate(
		CH1_UART2_ClientTask,
		"CH1_UART2_ClientTask",
		2048,
		NULL,
		configMAX_PRIORITIES - 1,
		NULL
	);
	xTaskCreate(
		CH2_UART4_ServerTask,
		"CH2_UART4_ServerTask",
		2048,
		NULL,
		configMAX_PRIORITIES - 1,
		NULL
	);
#endif

}






