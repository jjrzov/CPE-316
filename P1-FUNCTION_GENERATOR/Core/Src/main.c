/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2024 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include <math.h>


#define TABLE_SIZE 240
#define AMPLITUDE 1500 //3 Vpp
#define DC_OFFSET 1500 //1.5V DC offset
#define Vref 3300


/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
void fill_sine_table();
void fill_saw_table();
void fill_triangle_table();
void DAC_write(uint16_t data);
uint16_t DAC_volt_conv(uint16_t milli_volt);

//global variables ##############################################################################
static uint16_t sine_table[TABLE_SIZE];
static uint16_t saw_table[TABLE_SIZE];
static uint16_t triangle_table[TABLE_SIZE];
int ccr_count;

int main(void)
{
	HAL_Init();
	SystemClock_Config();

	//TABLES ##################################################################################################
	fill_sine_table();
	fill_saw_table();
	fill_triangle_table();

	//SPI ###################################################################################################
	RCC->AHB2ENR |= (RCC_AHB2ENR_GPIOAEN); //enable clock for port A (SCLK)

	//reset pins
	GPIOA->MODER &= ~(GPIO_MODER_MODE4 | GPIO_MODER_MODE5 | GPIO_MODER_MODE6 | GPIO_MODER_MODE7);
	//set GPIO mode to alternate function
	GPIOA->MODER |= (GPIO_MODER_MODE4_1 | GPIO_MODER_MODE5_1 | GPIO_MODER_MODE6_1 | GPIO_MODER_MODE7_1);

	//mask AF
	GPIOA->AFR[0] &= ~(GPIO_AFRL_AFSEL4 | GPIO_AFRL_AFSEL5 | GPIO_AFRL_AFSEL6 | GPIO_AFRL_AFSEL7);
	//set to SPI_1 -> AF5
	GPIOA->AFR[0] |= ((5 << GPIO_AFRL_AFSEL4_Pos) | (5 << GPIO_AFRL_AFSEL5_Pos) | (5 << GPIO_AFRL_AFSEL6_Pos) |
				  (5 << GPIO_AFRL_AFSEL7_Pos));

	//set speed to low
	GPIOA->OSPEEDR &= ~(GPIO_OSPEEDR_OSPEED4 | GPIO_OSPEEDR_OSPEED5 | GPIO_OSPEEDR_OSPEED6 | GPIO_OSPEEDR_OSPEED7);

	RCC->APB2ENR |= (RCC_APB2ENR_SPI1EN); //enable SPI clock
	SPI1->CR1 &= ~0xFFFF; //reset CR1
	SPI1->CR1 = (1 << 2) //master mode
		  | (0x0 << 3) //baud rate f/2
		  | (1 << 14); //transmit only

	SPI1->CR2 &= ~0xFFFF; //reset CR2
	SPI1->CR2 = (1 << 2) //SSOE enabled
		  | (1 << 3) //NSS pulse generation
		  | (0xF << 8); //set data length to 16 bits (12 data and 4 operation)

	SPI1->CR1 |= (1 << 6); //set SPI ON

	//ISR #######################################################################################################
	__enable_irq();

	NVIC->ISER[0] = (1 << (TIM2_IRQn & 0x1F));

	RCC->APB1ENR1 |= (RCC_APB1ENR1_TIM2EN);	// turn on TIM2
	TIM2->CCR1 = 0x000003E8;			// set CCR1 to 1000 for 100Hz
	TIM2->DIER |= (TIM_DIER_CC1IE);	// enable interrupts on channel 1
	//TIM2->CCMR1 &= ~(0 << 3);
	TIM2->SR &= ~(TIM_SR_CC1IF);		// go into status register and clear interrupt flag
	TIM2->ARR = 0x0003A980;			// set ARR to 240000
	TIM2->CR1 |= TIM_CR1_CEN;			// start timer

	while (1)
	{
		//code for reading from keypad
	}

}

void fill_sine_table(void){
	uint16_t i, temp;
	for(i = 0; i < TABLE_SIZE; i++){
		temp = (uint16_t)((AMPLITUDE) * sin((2 * M_PI * 100 * i) / TABLE_SIZE) + DC_OFFSET);
		sine_table[i] = DAC_volt_conv(temp);
	}
}

void fill_saw_table(void){
	uint16_t i, temp;
	for(i = 0; i < TABLE_SIZE; i++){
		temp = (uint16_t)((AMPLITUDE * i)/TABLE_SIZE);
		saw_table[i] = DAC_volt_conv(temp);
	}
}

void fill_triangle_table(void){
	uint16_t i, temp;
	for(i = 0; i < TABLE_SIZE/2; i++){
		temp = (uint16_t)((AMPLITUDE * i)/TABLE_SIZE);
		triangle_table[i] = DAC_volt_conv(temp);
		triangle_table[(TABLE_SIZE - 1) - i] = triangle_table[i];
	}
}

void TIM2_IRQHandler(void){
	 uint32_t C_int = TIM2->SR & TIM_SR_CC1IF;
	 uint32_t ARR_int = TIM2->SR & TIM_SR_UIF;

	 TIM2->SR &= ~(TIM_SR_CC1IF);
	 TIM2->SR &= ~(TIM_SR_UIF);
	 //TIM2->CCR1 += (0x000003E8); //increment CCR1 by 1000
	 if (ARR_int){
		 //reset CCR1 each time ARR reached
		 TIM2->CCR1 = 0x000003E8;
		 ccr_count = 1000;
	 }
	 else if (C_int){
		 //write to DAC each time CCR1 reached
		 DAC_write(saw_table[(ccr_count)/1000]);
		 ccr_count += 1000;
		 TIM2->CCR1 += (0x000003E8); //increment CCR1 by 1000
	 }
}

void DAC_write(uint16_t data){
  while (!(SPI1->SR & SPI_SR_TXE)){
	  //wait until transmit fifo is empty
  }

  SPI1->DR = (0b0011 << 12) | data;

  while (!(SPI1->SR & SPI_SR_TXE)){
	  //wait until tramsit fifo empty
  }
}

uint16_t DAC_volt_conv(uint16_t milli_volt){
  return (4096 *1000/ Vref) * milli_volt;
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
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_MSI;
  //RCC_OscInitStruct.MSIState = RCC_MSI_ON;  //datasheet says NOT to turn on the MSI then change the frequency.
  RCC_OscInitStruct.MSICalibrationValue = 0;
  RCC_OscInitStruct.MSIClockRange = RCC_MSIRANGE_9;
	/* from stm32l4xx_hal_rcc.h==
	#define RCC_MSIRANGE_0                 MSI = 100 KHz
	#define RCC_MSIRANGE_1                 MSI = 200 KHz
	#define RCC_MSIRANGE_2                 MSI = 400 KHz
	#define RCC_MSIRANGE_3                 MSI = 800 KHz
	#define RCC_MSIRANGE_4                 MSI = 1 MHz
	#define RCC_MSIRANGE_5                 MSI = 2 MHz
	#define RCC_MSIRANGE_6                 MSI = 4 MHz
	#define RCC_MSIRANGE_7                 MSI = 8 MHz
	#define RCC_MSIRANGE_8                 MSI = 16 MHz
	#define RCC_MSIRANGE_9                 MSI = 24 MHz
	#define RCC_MSIRANGE_10                MSI = 32 MHz
	#define RCC_MSIRANGE_11                MSI = 48 MHz   dont use this one*/
  RCC_OscInitStruct.MSIState = RCC_MSI_ON;  //datasheet says NOT to turn on the MSI then change the frequency.
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
	Error_Handler();
  }
  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
							  |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_MSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
	Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

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
