#ifndef __DEVICE_MANAGER_H__
#define __DEVICE_MANAGER_H__
#include "main.h"

typedef struct Dev_Mgmt{
	char *name;
	int (*Init)(struct Dev_Mgmt *pDev, int baud, char parity, int data_bit, int stop_bit);
	int (*Send)(struct Dev_Mgmt *pDev, uint8_t *datas, uint32_t len, int timeout);
	int (*RecvByte)(struct Dev_Mgmt *pDev, uint8_t *data, int timeout);
	int (*Flush)(struct Dev_Mgmt *pDev);
	void *priv_data;
}Dev_Mgmt, *PDev_Mgmt;

struct Dev_Mgmt *GetUARTDevice(char *name);

#endif

