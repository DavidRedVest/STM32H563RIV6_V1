#include "bsp_led.h"

//PC12
void bsp_led_init()
{
	GPIO_InitTypeDef GPIO_InitStruct = {0};	
	__HAL_RCC_GPIOH_CLK_ENABLE();	
	__HAL_RCC_GPIOC_CLK_ENABLE();

	
	/*Configure GPIO pin : PC12 */
  GPIO_InitStruct.Pin = GPIO_PIN_12;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
	
	/*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_12, GPIO_PIN_RESET);	
}

void bsp_led_on()
{
	HAL_GPIO_WritePin(GPIOC, GPIO_PIN_12, GPIO_PIN_RESET);
}

void bsp_led_off()
{
	HAL_GPIO_WritePin(GPIOC, GPIO_PIN_12, GPIO_PIN_SET);
}

void bsp_led_toggle()
{
	HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_12);
}

void bsp_test_init()
{
	//PB0
	GPIO_InitTypeDef GPIO_InitStruct = {0};	
	__HAL_RCC_GPIOH_CLK_ENABLE();	
	__HAL_RCC_GPIOB_CLK_ENABLE();
	
	/*Configure GPIO pin : PB0 */
  GPIO_InitStruct.Pin = GPIO_PIN_0;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
	
	/*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_RESET);
}

void bsp_test_toggle()
{
	HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_0);
}


