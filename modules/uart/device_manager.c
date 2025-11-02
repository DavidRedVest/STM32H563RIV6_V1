#include "device_manager.h"
//#include <stdio.h>
#include <string.h>


extern struct Dev_Mgmt g_uart2_dev;
extern struct Dev_Mgmt g_uart4_dev;
extern struct Dev_Mgmt g_usbserial_dev;

static struct Dev_Mgmt *g_uart_devices[] = {&g_uart2_dev, &g_uart4_dev,&g_usbserial_dev};

struct Dev_Mgmt *GetUARTDevice(char *name)
{
	unsigned int i = 0;
	for (i = 0; i < sizeof(g_uart_devices)/sizeof(g_uart_devices[0]); i++)
	{
		if (!strcmp(name, g_uart_devices[i]->name))
			return g_uart_devices[i];
	}
	
	return NULL;
}



