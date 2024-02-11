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

#define TABLE_SIZE 240
#define AMPLITUDE 1500 //3 Vpp
#define DC_OFFSET 1500 //1.5V DC offset
#define Vref 3300


/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
void fill_sin_table(int *sin_table);
void fill_saw_table(int *saw_table);
void fill_triangle_table(int *triangle_table);
void fill_square_table(int *square_table);
void DAC_write(uint16_t data);
uint16_t DAC_volt_conv(uint16_t volt);

int main(void)
{
	HAL_Init();
	SystemClock_Config();



	//TABLES ##################################################################################################
	int sine_table[TABLE_SIZE];
	int saw_table[TABLE_SIZE];
	int triangle_table[TABLE_SIZE];
	int square_table[TABLE_SIZE];

	fill_sin_table(sin_table);
	fill_saw_table(saw_table);
	fill_triangle_table(triangle_table);
	fill_square_table(square_table);

	//SPI ###################################################################################################
	RCC->AHB2ENR |= (RCC_AHB2ENR_GPIOAEN); //enable clock for port A (SCLK)
	RCC->CFGR |= (0x7 << 11); //MAYBE NOT NEEDED!?

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
		  | (0x7 << 3) //baud rate f/256
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
	TIM2->CCR1 = 0x000000C8;			// set channel 1 compare value to 200 for 25% Duty Cycle
	TIM2->DIER |= (TIM_DIER_CC1IE);	// enable interrupts on channel 1
	TIM2->SR &= ~(TIM_SR_CC1IF);		// go into status register and clear interrupt flag
	TIM2->ARR = 0x00000320;			// set count reload value 800
	//TIM2->CR1 |= TIM_CR1_CEN;			// start timer

	while (1)
	{
		//code for reading from keypad
	}

}

void fill_sin_table(int *sin_table){
	int i;
	for(i = 0; i < TABLE_SIZE; i++){
		sin_table[i] = (int)((AMPLITUDE) * sin((2 * M_PI * i) / TABLE_SIZE) + DC_OFFSET);
	}
}

void fill_saw_table(int *saw_table){
	int i;
	for(i = 0; i < TABLE_SIZE; i++){
		saw_table[i] = (int)((AMPLITUDE * i)/TABLE_SIZE);
	}
}

void fill_triangle_table(int *triangle_table){
	int i;
	for(i = 0; i < TABLE_SIZE/2; i++){
		triangle_table[i] = (int)((AMPLITUDE * i)/TABLE_SIZE);
		triangle_table[(TABLE_SIZE - 1) - i] = triangle_table[i];
	}
}

void fill_square_table(int *square_table){

}

void TIM2_IRQHandler(void){
	 uint32_t C_int = TIM2->SR & TIM_SR_CC1IF;
	 uint32_t ARR_int = TIM2->SR & TIM_SR_UIF;

	 TIM2->SR &= ~(TIM_SR_CC1IF);
	 TIM2->SR &= ~(TIM_SR_UIF);
	 TIM2->CCR1 += (0x000000C7);
	 if (ARR_int){
		 //write to the DAC each time ARR reached
		 TIM2->CCR1 = 0x000000C7;
	 }
	 else if (C_int){
		 //CC1F only used for square wave duty cycle
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

uint16_t DAC_volt_conv(uint16_t volt){
  return ((4096 * 1000) / Vref) * volt;
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
  RCC_OscInitStruct.MSIState = RCC_MSI_ON;
  RCC_OscInitStruct.MSICalibrationValue = 0;
  RCC_OscInitStruct.MSIClockRange = RCC_MSIRANGE_6;
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
