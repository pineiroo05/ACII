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
#include <stdio.h>
#include "main.h"
#include "TM1638.h"
/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
TM1638 TM;
#define RECORRIDO_CM 300
#define PASO_CM 3
#define TICK_MOV_MS 100
#define ANTIRREBOTE_MS 50
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

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

enum Estado{
	CERRADO,
	ABRIENDO,
	ABIERTO,
	CERRANDO,
	PARADO_ABRIENDO,
	PARADO_CERRANDO,
	PARADO_FOTOCELULA
};

bool detectarFlanco(){ //Codigo antirrebotes dado
	static bool estable=false;
	static bool ultimo=false;
	static uint32_t tCambio=0;
	bool crudo=(HAL_GPIO_ReadPin(B1_GPIO_Port, B1_Pin)==GPIO_PIN_RESET);
	//Si no coincide es que el boton se pulso
	if(crudo!=ultimo){
		ultimo=crudo;
		tCambio=HAL_GetTick();
	}
	if((HAL_GetTick()-tCambio)>=ANTIRREBOTE_MS && crudo!=estable){
		estable=crudo;
		return estable;
	}
	return false;
}

void controlPanel(enum Estado estado, int posicion){
	//Control de la pantalla. Los sprint son para convertir a string, como las funciones de hae
	char texto[9];
	switch(estado){
		case CERRADO:
			tm1638_DisplayTxt(&TM, "cErrAdA");
			break;
		case ABRIENDO:
			sprintf(texto, "%2d", posicion);
			tm1638_DisplayTxt(&TM,texto);
			break;
		case ABIERTO:
			tm1638_DisplayTxt(&TM, "AbiErtA");
			break;
		case CERRANDO:
			sprintf(texto, "%2d", posicion);
			tm1638_DisplayTxt(&TM,texto);
			break;
		case PARADO_ABRIENDO:
		case PARADO_CERRANDO:
		case PARADO_FOTOCELULA:
			tm1638_DisplayTxt(&TM, "PArAdA");
			break;
	}
	//Control de los leds
	int ledsEncendidos=(posicion*8)/RECORRIDO_CM;
	for(int i=1; i<=8; i++){
		if(i<=ledsEncendidos){
			tm1638_Led(&TM,i,1);
		}
		else{
			tm1638_Led(&TM,i,0);
		}
	}
}

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

  /* USER CODE END 2 */
  //Como en hae, inicializacion de puertos y del display
  TM.clk_port = CLK_GPIO_Port;
  TM.clk_pin = CLK_Pin;
  TM.stb_port = STB_GPIO_Port;
  TM.stb_pin = STB_Pin;
  TM.dio_port = DIO_GPIO_Port;
  TM.dio_pin = DIO_Pin;

  tm1638_TurnOn(&TM, 7);
  tm1638_DisplayClear(&TM);

  enum Estado estado=CERRADO;
  int posicion=0;
  //static uint32_t tMov=0;

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */
	  bool pulsacionBoton=detectarFlanco();
	  uint8_t teclas=tm1638_ScanButtons(&TM);
	  bool fotocelula=tm1638_KeyState(&TM, teclas, 1);

	  //Temporizacion de tiempos
	  static uint32_t tMov=0;
	  bool tick=(HAL_GetTick()-tMov)>=TICK_MOV_MS;
	  	  if(tick){
	  		  tMov+=TICK_MOV_MS;
	  }

	  switch(estado){
	  	  case CERRADO:
	  		  if(pulsacionBoton){
	  			  estado=ABRIENDO;
	  		  }
	  		  break;
	  	  case ABRIENDO:
	  		  if(pulsacionBoton){
	  			  estado=PARADO_ABRIENDO;
	  		  }
	  		  if(tick){
	  			  posicion+=PASO_CM;
	  			  if(posicion>=RECORRIDO_CM){
	  				  posicion=RECORRIDO_CM;
	  				  estado=ABIERTO;
	  			  }
	  		  }
	  		  break;
	  	  case ABIERTO:
	  		  if(pulsacionBoton){
	  			  estado=CERRANDO;
	  		  }
	  		  break;
	  	  case CERRANDO:
	  		  if(fotocelula){
	  			  estado=PARADO_FOTOCELULA;
	  		  }
	  		  else if(pulsacionBoton){
	  			  estado=PARADO_CERRANDO;
	  		  }
	  		  else if(tick){
	  			  posicion-=PASO_CM;
	  			  if(posicion<=0){
	  				  posicion=0;
	  				  estado=CERRADO;
	  			  }
	  		  }
	  		  break;
	  	  case PARADO_ABRIENDO:
	  		  if(pulsacionBoton){
	  			  estado=CERRANDO;
	  		  }
	  		  break;
	  	  case PARADO_CERRANDO:
	  		  if(pulsacionBoton){
	  			  estado=ABRIENDO;
	  		  }
	  		  break;
	  	  case PARADO_FOTOCELULA:
	  		  if(!fotocelula){
	  			  estado=CERRANDO;
	  		  }
	  		  break;
	  }
	  controlPanel(estado, posicion);
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
