/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2021 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under BSD 3-Clause license,
  * the "License"; You may not use this file except in compliance with the
  * License. You may obtain a copy of the License at:
  *                        opensource.org/licenses/BSD-3-Clause
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "spi.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

#include <stdio.h>
#include "spsgrf.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

//#define APPLICATION_TRANSMITTER // comment out if programming the receiver

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

volatile SpiritFlagStatus xTxDoneFlag;
volatile SpiritFlagStatus xRxDoneFlag;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* USER CODE BEGIN 1 */

  char payload[20] = "Hello World!\r\n";
  uint8_t rxLen;

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_USART2_UART_Init();
  MX_SPI1_Init();
  /* USER CODE BEGIN 2 */

  SPSGRF_Init();
  SpiritPktBasicSetDestinationAddress(0x44);

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */

#ifdef APPLICATION_TRANSMITTER
    // Send the payload
    xTxDoneFlag = S_RESET;
    HAL_UART_Transmit(&huart2, "before while\r\n", 14, HAL_MAX_DELAY);
    SPSGRF_StartTx(payload, strlen(payload));
    HAL_UART_Transmit(&huart2, "Entering while\r\n", 14, HAL_MAX_DELAY);

    while(!xTxDoneFlag);
    HAL_UART_Transmit(&huart2, "Payload Sent\r\n", 14, HAL_MAX_DELAY); // Never getting here

    HAL_Delay(2000); // Block for 2000 ms
#else
    xRxDoneFlag = S_RESET;
    SPSGRF_StartRx();
    HAL_UART_Transmit(&huart2, "Waiting to receive\r\n", 14, HAL_MAX_DELAY);
    while (!xRxDoneFlag);
    HAL_UART_Transmit(&huart2, "After loop\r\n", 14, HAL_MAX_DELAY);
    rxLen = SPSGRF_GetRxData(payload);
    HAL_UART_Transmit(&huart2, "Received: ", 10, HAL_MAX_DELAY); // Never getting here
    HAL_UART_Transmit(&huart2, payload, rxLen, HAL_MAX_DELAY);
#endif // APPLICATION_TRANSMITTER
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 1;
  RCC_OscInitStruct.PLL.PLLN = 10;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV7;
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

void EXTI0_IRQHandler(void){
	SpiritIrqs xIrqStatus;
	HAL_UART_Transmit(&huart2, "Interrupt called\r\n", 14, HAL_MAX_DELAY);
	SpiritIrqGetStatus(&xIrqStatus);
	  if (xIrqStatus.IRQ_TX_DATA_SENT)
	  {
	    xTxDoneFlag = S_SET;
	  }
	  if (xIrqStatus.IRQ_RX_DATA_READY)
	  {
	    xRxDoneFlag = S_SET;
	  }
    if (EXTI->PR1 & 0x1){
        EXTI->PR1 |= 0x1; //clear flag by writing to it
        GPIOA->BSRR = 0x1 << 5; //turn on LED
    }
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_SET);

  SpiritIrqs xIrqStatus;
  HAL_UART_Transmit(&huart2, "Interrupt called\r\n", 14, HAL_MAX_DELAY);
  if (GPIO_Pin != SPIRIT1_GPIO3_Pin)
  {
    return;
  }

  SpiritIrqGetStatus(&xIrqStatus);
  if (xIrqStatus.IRQ_TX_DATA_SENT)
  {
    xTxDoneFlag = S_SET;
  }
  if (xIrqStatus.IRQ_RX_DATA_READY)
  {
    xRxDoneFlag = S_SET;
  }
  if (xIrqStatus.IRQ_RX_DATA_DISC || xIrqStatus.IRQ_RX_TIMEOUT)
  {
    SpiritCmdStrobeSabort();
  }
}

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
