#include "bsp_usb.h"

//#include "ux_port.h"
//#include "ux_device_descriptors.h"
//#include "ux_dcd_stm32.h"
//ux_dcd_stm32.h
//#include "stm32h5xx_hal_pcd_ex.h"


#include "ux_port.h"
#include "ux_device_descriptors.h"
#include "ux_dcd_stm32.h"


PCD_HandleTypeDef hpcd_USB_DRD_FS;


void MX_USB_PCD_Init(void)
{

  /* USER CODE BEGIN USB_Init 0 */
  UINT MX_USBX_Device_Init(void);
	MX_USBX_Device_Init();

  /* USER CODE END USB_Init 0 */

  /* USER CODE BEGIN USB_Init 1 */

  /* USER CODE END USB_Init 1 */
  hpcd_USB_DRD_FS.Instance = USB_DRD_FS;
  hpcd_USB_DRD_FS.Init.dev_endpoints = 8;
  hpcd_USB_DRD_FS.Init.speed = USBD_FS_SPEED;
  hpcd_USB_DRD_FS.Init.phy_itface = PCD_PHY_EMBEDDED;
  hpcd_USB_DRD_FS.Init.Sof_enable = DISABLE;
  hpcd_USB_DRD_FS.Init.low_power_enable = DISABLE;
  hpcd_USB_DRD_FS.Init.lpm_enable = DISABLE;
  hpcd_USB_DRD_FS.Init.battery_charging_enable = DISABLE;
  hpcd_USB_DRD_FS.Init.vbus_sensing_enable = DISABLE;
  hpcd_USB_DRD_FS.Init.bulk_doublebuffer_enable = DISABLE;
  hpcd_USB_DRD_FS.Init.iso_singlebuffer_enable = DISABLE;
  if (HAL_PCD_Init(&hpcd_USB_DRD_FS) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USB_Init 2 */

	HAL_PWREx_EnableVddUSB();
	HAL_PWREx_EnableUSBVoltageDetector();
	
  HAL_PCDEx_PMAConfig(&hpcd_USB_DRD_FS, 0x00, PCD_SNG_BUF, 0x14);
	HAL_PCDEx_PMAConfig(&hpcd_USB_DRD_FS, 0x80, PCD_SNG_BUF, 0x54);
	HAL_PCDEx_PMAConfig(&hpcd_USB_DRD_FS, USBD_CDCACM_EPINCMD_ADDR, PCD_SNG_BUF, 0x94);
	HAL_PCDEx_PMAConfig(&hpcd_USB_DRD_FS, USBD_CDCACM_EPOUT_ADDR, PCD_SNG_BUF, 0xD4);
	HAL_PCDEx_PMAConfig(&hpcd_USB_DRD_FS, USBD_CDCACM_EPIN_ADDR, PCD_SNG_BUF, 0x114);
	ux_dcd_stm32_initialize((ULONG)USB_DRD_FS, (ULONG)&hpcd_USB_DRD_FS);
	
	HAL_PCD_Start(&hpcd_USB_DRD_FS);


	
  /* USER CODE END USB_Init 2 */

}

void HAL_PCD_MspInit(PCD_HandleTypeDef* pcdHandle)
{

  GPIO_InitTypeDef GPIO_InitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};
  if(pcdHandle->Instance==USB_DRD_FS)
  {
  /* USER CODE BEGIN USB_DRD_FS_MspInit 0 */

  /* USER CODE END USB_DRD_FS_MspInit 0 */

  /** Initializes the peripherals clock
  */
    PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_USB;
    PeriphClkInitStruct.UsbClockSelection = RCC_USBCLKSOURCE_HSI48;
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
    {
      Error_Handler();
    }

    __HAL_RCC_GPIOA_CLK_ENABLE();
    /**USB GPIO Configuration
    PA11     ------> USB_DM
    PA12     ------> USB_DP
    */
    GPIO_InitStruct.Pin = GPIO_PIN_11|GPIO_PIN_12;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = GPIO_AF10_USB;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /* USB_DRD_FS clock enable */
    __HAL_RCC_USB_CLK_ENABLE();

    /* USB_DRD_FS interrupt Init */
    HAL_NVIC_SetPriority(USB_DRD_FS_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(USB_DRD_FS_IRQn);
  /* USER CODE BEGIN USB_DRD_FS_MspInit 1 */

  /* USER CODE END USB_DRD_FS_MspInit 1 */
  }
}

void HAL_PCD_MspDeInit(PCD_HandleTypeDef* pcdHandle)
{

  if(pcdHandle->Instance==USB_DRD_FS)
  {
  /* USER CODE BEGIN USB_DRD_FS_MspDeInit 0 */

  /* USER CODE END USB_DRD_FS_MspDeInit 0 */
    /* Peripheral clock disable */
    __HAL_RCC_USB_CLK_DISABLE();

    /**USB GPIO Configuration
    PA11     ------> USB_DM
    PA12     ------> USB_DP
    */
    HAL_GPIO_DeInit(GPIOA, GPIO_PIN_11|GPIO_PIN_12);

    /* USB_DRD_FS interrupt Deinit */
    HAL_NVIC_DisableIRQ(USB_DRD_FS_IRQn);
  /* USER CODE BEGIN USB_DRD_FS_MspDeInit 1 */

  /* USER CODE END USB_DRD_FS_MspDeInit 1 */
  }
}

/* USER CODE BEGIN 1 */

/* USER CODE END 1 */



