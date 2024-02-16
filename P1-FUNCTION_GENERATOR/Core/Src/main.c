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
#include "math.h"


#define TABLE_SIZE 240
#define AMPLITUDE 1500 //3 Vpp
#define DC_OFFSET 1500 //1.5V DC offset
#define Vref 3300
#define pi 3.14159
#define SCLK 24000000 //24 Mhz


/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
void fill_sine_table();
void fill_saw_table();
void fill_triangle_table();
void DAC_write(uint16_t data);
uint16_t DAC_volt_conv(uint16_t milli_volt);
int PressDetector(int col);
void updateParam();

//global variables ##############################################################################
static uint16_t sine_table[TABLE_SIZE];
static uint16_t saw_table[TABLE_SIZE];
static uint16_t triangle_table[TABLE_SIZE];

int ccr_count;
int increment; // = 1000
int f_factor; // initially set to 1 for 100Hz
int duty_cycle;

int square_flag;
int saw_flag;
int sine_flag;
int triangle_flag;

int calc_ARR;
int calc_CCR1;

int main(void)
{
	HAL_Init();
	SystemClock_Config();

	//TABLES ##################################################################################################
	fill_sine_table();
	fill_saw_table();
	fill_triangle_table();

	//KEYPAD ###############################################################################################
	//turn on B and C
	RCC->AHB2ENR |= (RCC_AHB2ENR_GPIOBEN | RCC_AHB2ENR_GPIOCEN); //A will be turned on in SPI

	//bank B as GPIO
	//preset rows as inputs
	//pins B4,5,6,7
	GPIOB->MODER &= ~(GPIO_MODER_MODE4 | GPIO_MODER_MODE5 | GPIO_MODER_MODE6 | GPIO_MODER_MODE7);

	//set pins B4,5,6,7 to pull down
	GPIOB->PUPDR &= ~(GPIO_PUPDR_PUPD4 | GPIO_PUPDR_PUPD5 | GPIO_PUPDR_PUPD6 | GPIO_PUPDR_PUPD7);
	GPIOB->PUPDR |= (GPIO_PUPDR_PUPD4_1 | GPIO_PUPDR_PUPD5_1 | GPIO_PUPDR_PUPD6_1 | GPIO_PUPDR_PUPD7_1);

	//bank C as GPIO
	//preset the columns as outputs
	//pins C0,1,2,3
	GPIOC->MODER &= ~(GPIO_MODER_MODE0 | GPIO_MODER_MODE1 | GPIO_MODER_MODE2 | GPIO_MODER_MODE3);
	GPIOC->MODER |= (GPIO_MODER_MODE0_0 | GPIO_MODER_MODE1_0 | GPIO_MODER_MODE2_0 | GPIO_MODER_MODE3_0);

	//preset all output pins to 0
	GPIOC->BRR = (GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3);
	int col = 0;

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
	//TIM2->CCR1 = 999;

	//SQUARE WAVE
	//TIM2->CCR1 = 120000-1; //50% ->
	//TIM2->CCR1 = 80000;
	//TIM2->CCR1 = 60000;
	//TIM2->CCR1 = 48000;

	//TIM2->DIER |= (TIM_DIER_CC1IE);	// enable interrupts on channel 1
	//TIM2->SR &= ~(TIM_SR_CC1IF);		// go into status register and clear interrupt flag
	//TIM2->SR &= ~(TIM_SR_UIF);
	//TIM2->CR1 |= TIM_CR1_CEN;
	//TIM2->ARR = 239999;
	//TIM2->ARR = 119999;
	//TIM2->ARR = 79999;
	//TIM2->ARR = 59999;
	//TIM2->ARR = 47999;

	//initialization wave
	square_flag = 1; //square wave
	saw_flag = 0;
	sine_flag = 0;
	triangle_flag = 0;

	duty_cycle = 2; //50% duty cycle
	f_factor = 1; //100 Hz
	//increment = 120000;
	updateParam();

	TIM2->CR1 |= TIM_CR1_CEN;			// start timer

	while (1)
	{
		//code for reading from keypad
		//sets a square flag high or low (global var)
		//goes to function for calculating ARR and CCR1
		//set f_factor depending on freq
		//set duty_cycle for square waves -1 for else

		int res = -1;
		while (res == -1){
			res = PressDetector(col);
			col++;
			//HAL_Delay(5);
			if (col >= 4){
				col = 0;
			}
		}
		//TIM2->DIER &= ~(TIM_DIER_CC1IE);	// disable interrupts on channel 1
		//res has pressed value
		if (res == 6){
			sine_flag = 1; //sine wave
			square_flag = 0;
			saw_flag = 0;
			triangle_flag = 0;
			//updateParam();
		}
		else if (res == 7){
			triangle_flag = 1; //triangle wave
			square_flag = 0;
			saw_flag = 0;
			sine_flag = 0;
			//updateParam();
		}
		else if (res == 8){
			saw_flag = 1; //saw wave
			square_flag = 0;
			sine_flag = 0;
			triangle_flag = 0;
			//updateParam();
		}
		else if (res == 9){
			square_flag = 1; //square wave
			saw_flag = 0;
			sine_flag = 0;
			triangle_flag = 0;
			//updateParam();
		}
		else if (res == 1){
			f_factor = 1; //100 Hz
			//updateParam();
		}
		else if (res == 2){
			f_factor = 2; //200 Hz
			//updateParam();
		}
		else if (res == 3){
			f_factor = 3; //300 Hz
			//updateParam();
		}
		else if (res == 4){
			f_factor = 4; //400 Hz
			//updateParam();
		}
		else if (res == 5){
			f_factor = 5; //500 Hz
			//updateParam();
		}
		else if (res == 0){
			//might need an if to check if square wave
			duty_cycle = 2; //50% duty cycle
		}
		else if (res == 14){ // #
			if (duty_cycle < 9){
				duty_cycle += 1;
			}
		}
		else if (res == 15){ // *
			if (duty_cycle > 1){
				duty_cycle -= 1;
			}
		}
		//updateParam();
		HAL_Delay(2);
	}
}

void updateParam(){
	//recalculates the ARR and CCR1 values
	TIM2->DIER &= ~(TIM_DIER_CC1IE);	// disable interrupts on channel 1
	TIM2->DIER &= ~(TIM_DIER_UIE);	// enable interrupts on ARR

	calc_ARR = (SCLK / (f_factor * 100)) - 1; //calculate new ARR value
	TIM2->ARR = calc_ARR; //set ARR

	if (square_flag){
		//if square wave
		calc_CCR1 = (calc_ARR + 1) / duty_cycle;
		TIM2->CCR1 = calc_CCR1 - 1;
		increment = calc_CCR1; //increment equals CCR1 for square
	}
	else {
		calc_CCR1 = 1000;
		TIM2->CCR1 = calc_CCR1 - 1;
		increment = calc_CCR1;
		ccr_count = 0; //MAYBE NOT NEEDED
	}

	//TIM2->CR1 |= TIM_CR1_CEN; //start timer
	TIM2->DIER |= (TIM_DIER_CC1IE);	// enable interrupts on channel 1
	TIM2->DIER |= (TIM_DIER_UIE);	// enable interrupts on ARR
	TIM2->SR &= ~(TIM_SR_CC1IF);
	TIM2->SR &= ~(TIM_SR_UIF);
	TIM2->EGR |= (1 << 0); //reset CNT
	//HAL_Delay(5);
}

int PressDetector(int col){
	//set all columns high then monitor rows for detection
	//returns the now known row after button pressed
	GPIOC->BRR = (GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3);
	GPIOC->BSRR = (1 << col); //set given column high
	//check all the rows once
	if (GPIOB->IDR & GPIO_PIN_4){
		if (col == 3){
			return 10;
		} else {
			return col + (0 * 4) + 1;
		}
	} else if (GPIOB->IDR & GPIO_PIN_5){
		if (col == 3){
			return 11;
		} else {
			return col + (1 * 4);
		}
	} else if (GPIOB->IDR & GPIO_PIN_6){
		if (col == 3){
			return 12;
		} else {
			return col + (2 * 4) - 1;
		}
	} else if (GPIOB->IDR & GPIO_PIN_7){
		switch(col){
		case 0:
			return 15;
		case 1:
			return 0;
		case 2:
			return 14;
		case 3:
			return 13;
		}
	}
	return -1;
}

void fill_sine_table(void){
	uint16_t i, temp;
	for(i = 0; i < TABLE_SIZE; i++){
		temp = (uint16_t)((AMPLITUDE) * sin((2 * pi * i) / TABLE_SIZE) + DC_OFFSET);
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

	 if (ARR_int){
		 //reset CCR1 each time ARR reached
		 if (square_flag){
			 TIM2->CCR1 = increment - 1;
			 DAC_write(DAC_volt_conv(3000));
		 }
		 else {
			 if (sine_flag){
			 	 DAC_write(sine_table[((ccr_count + 1) * f_factor / 1000)]);
		 	 }
		 	 else if (saw_flag){
			 	 DAC_write(saw_table[((ccr_count + 1) * f_factor / 1000)]);
		 	 }
		 	 else if (triangle_flag){
			 	 DAC_write(triangle_table[((ccr_count + 1) * f_factor / 1000)]);
		 	 }
			 TIM2->CCR1 = increment -1;
			 ccr_count = 0;
		 }

		 //SIN/SAW/TRIANGLE WAVE
		 //DAC_write(triangle_table[((ccr_count + 1) * f_factor / 1000)]); //*5
		 //TIM2->CCR1 = increment-1;
		 //ccr_count = 0;

		 //SQUARE WAVE
		 //TIM2->CCR1 = 120000-1; //now increment
		 //TIM2->CCR1 = 80000;
		 //TIM2->CCR1 = 60000;
		 //TIM2->CCR1 = 48000;
		 //DAC_write(DAC_volt_conv(3000));
	 }
	 else if (C_int){
		 //write to DAC each time CCR1 reached

		 if (square_flag){
			 DAC_write(DAC_volt_conv(0));
			 TIM2->CCR1 += increment; //now increment hopefully
		 }
		 else {
			 if (sine_flag){
				 DAC_write(sine_table[((ccr_count + 1) * f_factor / 1000)]);
			 }
			 else if (saw_flag){
				 DAC_write(saw_table[((ccr_count + 1) * f_factor / 1000)]);
			 }
			 else if (triangle_flag){
				 DAC_write(triangle_table[((ccr_count + 1) * f_factor / 1000)]);
			 }
			 ccr_count += increment;
			 TIM2->CCR1 += increment;
		 }
		 //TIM2->CCR1 += increment;

		 //SIN/SAW/TRIANGLE WAVE
		 //DAC_write(triangle_table[((ccr_count + 1) * f_factor / 1000)]); // *5
		 //ccr_count += increment;
		 //TIM2->CCR1 += increment;

		 //SQUARE WAVE
		 //DAC_write(DAC_volt_conv(0));
		 //TIM2->CCR1 += 120000; //now increment hopefully
		 //TIM2->CCR1 += 80000;
		 //TIM2->CCR1 += 60000;
		 //TIM2->CCR1 += 48000;

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
  return (4096 / Vref) * milli_volt;
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
