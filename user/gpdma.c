#include "gpdma.h"

void MX_GPDMA1_Init(void)
{

  /* USER CODE BEGIN GPDMA1_Init 0 */

  /* USER CODE END GPDMA1_Init 0 */

  /* Peripheral clock enable */
  __HAL_RCC_GPDMA1_CLK_ENABLE();

  /* GPDMA1 interrupt Init */
	HAL_NVIC_SetPriority(GPDMA1_Channel0_IRQn, 5, 0);
	HAL_NVIC_EnableIRQ(GPDMA1_Channel0_IRQn);
	HAL_NVIC_SetPriority(GPDMA1_Channel1_IRQn, 5, 0);
	HAL_NVIC_EnableIRQ(GPDMA1_Channel1_IRQn);
	HAL_NVIC_SetPriority(GPDMA1_Channel2_IRQn, 5, 0);
	HAL_NVIC_EnableIRQ(GPDMA1_Channel2_IRQn);
	HAL_NVIC_SetPriority(GPDMA1_Channel3_IRQn, 5, 0);
	HAL_NVIC_EnableIRQ(GPDMA1_Channel3_IRQn);


  /* USER CODE BEGIN GPDMA1_Init 1 */

  /* USER CODE END GPDMA1_Init 1 */
  /* USER CODE BEGIN GPDMA1_Init 2 */

  /* USER CODE END GPDMA1_Init 2 */

}



