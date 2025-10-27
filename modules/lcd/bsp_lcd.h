#ifndef __BSP_LCD_H__
#define __BSP_LCD_H__
#include "main.h"

extern SPI_HandleTypeDef hspi2;

void MX_SPI2_Init(void);

void bsp_lcd_init(void);

void bsp_test_lcd(void);

#endif
