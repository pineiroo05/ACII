/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
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
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include <stdio.h>

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "TM1638.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
TM1638 TM;
#define TICK_MS 1000
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
/* USER CODE BEGIN PFP */
enum Estado{
	PARADO,
	TURNO_A,
	TURNO_B,
	PAUSA,
	FIN
};

void representacionTexto(char letra, int valor[3]){
	char caracteres[9];
	sprintf(caracteres, "%c %d.%d%d", letra, valor[2], valor[1], valor[0]);
	tm1638_DisplayTxt(&TM, caracteres);
}
void retrocesoCronometro(int valor[3]){
	valor[0]--;
	if(valor[0]<0){
		valor[0]=9;
		valor[1]--;
		if(valor[1]<0){
			valor[1]=5;
			valor[2]--;
			if(valor[2]<0){
				valor[0]=0;
				valor[1]=0;
				valor[2]=0;
			}
		}
	}
}
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
  /* USER CODE BEGIN 2 */
  TM.clk_port=CLK_GPIO_Port;
  TM.clk_pin=CLK_Pin;
  TM.stb_port=STB_GPIO_Port;
  TM.stb_pin=STB_Pin;
  TM.dio_port=DIO_GPIO_Port;
  TM.dio_pin=DIO_Pin;

  tm1638_TurnOn(&TM, 7);
  tm1638_DisplayClear(&TM);

  int valorA[3]={0,0,2};
  int valorB[3]={0,0,2};
  static uint32_t tCuenta=0;
  tCuenta=HAL_GetTick();

  enum Estado estadoActual=PARADO;
  enum Estado estadoAnterior;
  int teclaAnterior=0;
  int ganador=0;
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */
	  //Control de los botones
	  uint8_t teclas=tm1638_ScanButtons(&TM);
	  int teclaPulsada=tm1638_ReadKey(&TM, teclas);
	  if(teclaPulsada!=teclaAnterior && teclaPulsada!=0){
		  if(teclaPulsada==1){
			  if(estadoActual==PARADO || estadoActual==TURNO_B){
				  estadoActual=TURNO_A;
				  tCuenta=HAL_GetTick(); //Reinicia la base de tiempos
			  }
		  }else if(teclaPulsada==8){
			  if(estadoActual==PARADO || estadoActual==TURNO_A){
				  estadoActual=TURNO_B;
				  tCuenta=HAL_GetTick(); //Reinicia la base de tiempos
			  }
		  }else if(teclaPulsada==4){
			  if(estadoActual==TURNO_A || estadoActual==TURNO_B){
				  estadoAnterior=estadoActual;
				  estadoActual=PAUSA;
			  }else if(estadoActual==PAUSA){
				  estadoActual=estadoAnterior;
				  tCuenta=HAL_GetTick(); //Reinicia la base de tiempos
			  }else if(estadoActual==FIN){
				  //Reinicio de los relojes
				  valorA[0]=0; valorA[1]=0; valorA[2]=2;
				  valorB[0]=0; valorB[1]=0; valorB[2]=2;
				  estadoActual=PARADO;

			  }
		  }
	  }
	  teclaAnterior=teclaPulsada;

	  //Maquina de estados
	  switch(estadoActual){
	  	  case PARADO:
	  		  tm1638_DisplayTxt(&TM, "PArAdO");
	  		  break;
	  	  case TURNO_A:
	  		  //Encender los 4 primeros leds
	  		  for(int i=1; i<=8; i++){
	  			  tm1638_Led(&TM, i, i<=4?1:0);
	  		  }
	  		  if(HAL_GetTick()-tCuenta>=TICK_MS){
	  			  retrocesoCronometro(valorA);
	  			  representacionTexto('A', valorA);
	  			  if(valorA[0]==0 && valorA[1]==0 && valorA[2]==0){
	  				  ganador=1;
	  				  estadoActual=FIN;
	  			  }
	  			  tCuenta+=TICK_MS;
	  		  }
	  		  break;
	  	  case TURNO_B:
	  		  //Encender los 4 ultimos leds
	  		  for(int i=1; i<=8; i++){
	  			  tm1638_Led(&TM, i, i>4?1:0);
	  		  }
	  		  if(HAL_GetTick()-tCuenta>=TICK_MS){
	  			  retrocesoCronometro(valorB);
	  			  representacionTexto('b', valorB);
	  			  if(valorB[0]==0 && valorB[1]==0 && valorB[2]==0){
	  				  ganador=2;
	  				  estadoActual=FIN;
	  			  }
	  			  tCuenta+=TICK_MS;
	  		  }
	  		  break;
	  	  case PAUSA:
	  		  static uint32_t tAlternar=0;
	  		  static int alternar=0;
	  		  if(HAL_GetTick()-tAlternar>=500){
	  			  alternar=!alternar;
	  			  if(alternar){
	  				  tm1638_DisplayTxt(&TM, "PAUSA");
	  			  }else{
	  				  estadoAnterior==TURNO_A?representacionTexto('A', valorA):representacionTexto('b', valorB);
	  			  }
	  			  tAlternar=HAL_GetTick();
	  		  }
	  		  break;
	  	  case FIN:
	  		  ganador==1 ? tm1638_DisplayTxt(&TM, "Fin A"):tm1638_DisplayTxt(&TM, "Fin B");
	  		  static uint32_t tParpadeo=0;
	  		  static int estadoLeds=0;
	  		  if(HAL_GetTick()-tParpadeo>=250){
	  			  estadoLeds=!estadoLeds;
	  			  for(int i=1; i<=8; i++){
	  				  tm1638_Led(&TM, i, estadoLeds);
	  			  }
	  			  tParpadeo=HAL_GetTick();
	  		  }
	  		  break;
	  }
    /* USER CODE BEGIN 3 */
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
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE2);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 16;
  RCC_OscInitStruct.PLL.PLLN = 336;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV4;
  RCC_OscInitStruct.PLL.PLLQ = 7;
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
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, LD2_Pin|STB_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, CLK_Pin|DIO_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : B1_Pin */
  GPIO_InitStruct.Pin = B1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : LD2_Pin STB_Pin */
  GPIO_InitStruct.Pin = LD2_Pin|STB_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : CLK_Pin DIO_Pin */
  GPIO_InitStruct.Pin = CLK_Pin|DIO_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
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
#ifdef USE_FULL_ASSERT
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
