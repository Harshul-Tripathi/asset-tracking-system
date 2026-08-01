/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32u5xx_hal.h"
#include "stm32u5xx_nucleo.h"
#include <stdio.h>

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define UWB_CS_Pin GPIO_PIN_4
#define UWB_CS_GPIO_Port GPIOA
#define UWB_RST_Pin GPIO_PIN_2
#define UWB_RST_GPIO_Port GPIOB
#define UWB_WAKE_Pin GPIO_PIN_11
#define UWB_WAKE_GPIO_Port GPIOF
#define UWB_IRQ_Pin GPIO_PIN_12
#define UWB_IRQ_GPIO_Port GPIOF
#define UWB_IRQ_EXTI_IRQn EXTI12_IRQn
#define BOOT_Pin GPIO_PIN_9
#define BOOT_GPIO_Port GPIOE
#define CHIP_EN_Pin GPIO_PIN_11
#define CHIP_EN_GPIO_Port GPIOE
#define SPI_RDY_Pin GPIO_PIN_13
#define SPI_RDY_GPIO_Port GPIOE
#define SPI_RDY_EXTI_IRQn EXTI13_IRQn
#define SPI_CS_Pin GPIO_PIN_14
#define SPI_CS_GPIO_Port GPIOD

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
