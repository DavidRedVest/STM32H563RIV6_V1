#ifndef __MODBUS_RTU_H__
#define __MODBUS_RTU_H__

#include "modbus.h"

#define MODBUS_RTU_MAX_ADU_LENGTH (256)

MODBUS_API modbus_t *
modbus_new_rtu(const char *device, int baud, char parity, int data_bit, int stop_bit);



#endif

