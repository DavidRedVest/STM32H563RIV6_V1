#ifndef __MODBUS_RTU_PRIVATE_H__
#define __MODBUS_RTU_PRIVATE_H__

#include <stdint.h>
#include "device_manager.h"

#define _MODBUS_RTU_HEADER_LENGTH     1
#define _MODBUS_RTU_PRESET_REQ_LENGTH 6
#define _MODBUS_RTU_PRESET_RSP_LENGTH 2

#define _MODBUS_RTU_CHECKSUM_LENGTH 2


typedef struct _modbus_rtu {
	/* for linux:/dev/ttyS0 or /dev/ttyUSB0*/
	char *device;
	/* baud */
	int baud;
	uint8_t data_bit;
	uint8_t stop_bit;
	/* Parity: 'N','O','E' */
	char parity;

	
	/* To handle many slaves on the same link */
	int confirmation_to_ignore;
	
	/* add uart device */
	struct Dev_Mgmt *dev;
}modbus_rtu_t;

#endif

