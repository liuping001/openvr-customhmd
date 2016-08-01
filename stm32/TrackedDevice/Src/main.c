/**
  ******************************************************************************
  * File Name          : main.c
  * Description        : Main program body
  ******************************************************************************
  *
  * COPYRIGHT(c) 2016 STMicroelectronics
  *
  * Redistribution and use in source and binary forms, with or without modification,
  * are permitted provided that the following conditions are met:
  *   1. Redistributions of source code must retain the above copyright notice,
  *      this list of conditions and the following disclaimer.
  *   2. Redistributions in binary form must reproduce the above copyright notice,
  *      this list of conditions and the following disclaimer in the documentation
  *      and/or other materials provided with the distribution.
  *   3. Neither the name of STMicroelectronics nor the names of its contributors
  *      may be used to endorse or promote products derived from this software
  *      without specific prior written permission.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
  * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
  * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
  * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
  * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
  * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
  * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
  *
  ******************************************************************************
  */
/* Includes ------------------------------------------------------------------*/
#include "stm32f1xx_hal.h"

/* USER CODE BEGIN Includes */
#include <stdlib.h>
#include "..\\..\\Common\\led.h"
#include "..\\..\\Common\\SensorFusion.h"
#include "..\\..\\Common\\gy8x.h"
#include "..\\..\\Common\\nrf24l01.h"
#include "..\\..\\Common\\usb.h"
/* USER CODE END Includes */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;
DMA_HandleTypeDef hdma_adc1;

I2C_HandleTypeDef hi2c2;

SPI_HandleTypeDef hspi2;

/* USER CODE BEGIN PV */
/* Private variables ---------------------------------------------------------*/

int8_t ctlSource = LEFTCTL_SOURCE;
SensorData tSensorData = {0};
int32_t blinkDelay = 1000;
USBPacket USBDataPacket = {0};
extern uint8_t USB_RX_Buffer[CUSTOM_HID_EPIN_SIZE];
USBPacket *pUSBCommandPacket = (USBPacket *) USB_RX_Buffer;
//volatile uint8_t rfReady = 1;
uint8_t rfStatus = 0;
bool sensorStatus = false;

#if CUSTOM_HID_EPOUT_SIZE != 32
#error HID out packet size not 32
#end
#elif CUSTOM_HID_EPIN_SIZE != 32
#error HID packet in size not 32
#end
#elif USB_CUSTOM_HID_DESC_SIZ != 34
#error HID packet size not 34
#end
#endif

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
void Error_Handler(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_I2C2_Init(void);
static void MX_SPI2_Init(void);
static void MX_ADC1_Init(void);

/* USER CODE BEGIN PFP */
/* Private function prototypes -----------------------------------------------*/
unsigned char TrackedDevice_NRF24L01_Init (void)
{
	RF_InitTypeDef RF_InitStruct = {0};
	RF_InitStruct.RF_Power_State=RF_Power_On;
	RF_InitStruct.RF_Config=RF_Config_IRQ_RX_Off|RF_Config_IRQ_TX_Off|RF_Confing_IRQ_Max_Rt_Off;
	RF_InitStruct.RF_CRC_Mode=RF_CRC8_On;
	RF_InitStruct.RF_Mode=RF_Mode_TX;
	RF_InitStruct.RF_Pipe_Auto_Ack=0;	
	RF_InitStruct.RF_Enable_Pipe=1;	
	RF_InitStruct.RF_Setup=RF_Setup_5_Byte_Adress;
	RF_InitStruct.RF_TX_Power=RF_TX_Power_High;
	RF_InitStruct.RF_Data_Rate=RF_Data_Rate_2Mbs;
	RF_InitStruct.RF_Channel=250;
	RF_InitStruct.RF_TX_Adress[0]=0x53;
	RF_InitStruct.RF_TX_Adress[1]=0x45;
	RF_InitStruct.RF_TX_Adress[2]=0x4E;
	RF_InitStruct.RF_TX_Adress[3]=0x43;
	RF_InitStruct.RF_TX_Adress[4]=0xC0 + ctlSource;
	RF_InitStruct.RF_RX_Adress_Pipe0[0]=0x53;
	RF_InitStruct.RF_RX_Adress_Pipe0[1]=0x45;
	RF_InitStruct.RF_RX_Adress_Pipe0[2]=0x4E;
	RF_InitStruct.RF_RX_Adress_Pipe0[3]=0x43;
	RF_InitStruct.RF_RX_Adress_Pipe0[4]=0xC0 + ctlSource;
	RF_InitStruct.RF_Payload_Size_Pipe0=32;
	RF_InitStruct.RF_Auto_Retransmit_Count=1;
	RF_InitStruct.RF_Auto_Retransmit_Delay=0;
	return RF_Init(&hspi2, &RF_InitStruct);
}	
/* USER CODE END PFP */

/* USER CODE BEGIN 0 */

uint64_t HAL_GetMicros()
{
	uint64_t ticks = HAL_GetTick();
	uint64_t systick = (uint64_t)SysTick->VAL;
	return (ticks * 1000ull) + 1000ull - (systick / 72ul);
}

void HAL_MicroDelay(uint64_t delay)
{
	uint64_t tickstart = 0;	
	tickstart = HAL_GetMicros();
	while((HAL_GetMicros() - tickstart) < delay)
	{
	}
}

volatile uint32_t ADC_Values[9] = {0};
volatile uint32_t ADC_Averages[9] = {0};
uint64_t lastUpdate = 0; 
uint64_t now = 0;        
uint32_t nextSend = 0; 


/* USER CODE END 0 */

int main(void)
{

  /* USER CODE BEGIN 1 */
  /* USER CODE END 1 */

  /* MCU Configuration----------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* Configure the system clock */
  SystemClock_Config();

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_I2C2_Init();
  MX_SPI2_Init();
  MX_ADC1_Init();

  /* USER CODE BEGIN 2 */
	
	LedOff();	 
	HAL_GPIO_WritePin(CTL_VIBRATE_GPIO_Port, CTL_VIBRATE_Pin, GPIO_PIN_SET); 
	BlinkRease(30);
	HAL_GPIO_WritePin(CTL_VIBRATE_GPIO_Port, CTL_VIBRATE_Pin, GPIO_PIN_RESET);
	
	sensorStatus = initSensors();		
	if (!sensorStatus)
		Error_Handler();
	LedOff();		
	
	rfStatus = TrackedDevice_NRF24L01_Init();	
	if (rfStatus == 0x00 || rfStatus == 0xff)
		Error_Handler();
	LedOff();	
	
	if( HAL_ADC_Start(&hadc1) != HAL_OK)  
		Error_Handler();
	if (HAL_ADC_Start_DMA(&hadc1, (uint32_t*)&ADC_Values[1], 7) != HAL_OK)  
		Error_Handler();
	//HAL_Delay(1000);

	ctlSource = 0;
	ctlSource = (GPIO_PIN_SET == HAL_GPIO_ReadPin(CTL_MODE1_GPIO_Port, CTL_MODE1_Pin)?1:0); 
	ctlSource += (GPIO_PIN_SET == HAL_GPIO_ReadPin(CTL_MODE2_GPIO_Port, CTL_MODE2_Pin)?2:0); 	
	srand(ctlSource);
	
	int extraDelay = ctlSource>1? 25:5;
	
	bool hasNewData = false;

	CSensorFusion fuse(aRes, gRes, mRes);
	
	if (ctlSource == RIGHTCTL_SOURCE || ctlSource == LEFTCTL_SOURCE)
	{
		
	}
	
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
	bool readyToSend = true;
	Quaternion quat;
	RF_ReceiveMode(&hspi2, NULL); //default to receive mode			
	uint32_t lastCommandSequence = 0;
	uint32_t vibrationStopTime = 0;
	while (true)
	{
		
		for (int i=1; i<8; i++)
		{
			ADC_Averages[i] = ((float)ADC_Averages[i] * 0.9) + ((float)ADC_Values[i] * 0.1);
		}
		
		if (ADC_Values[4] > 0x100)
		{
			ADC_Values[4] = ADC_Values[4];
		}
		
		
		Blink(blinkDelay);
		blinkDelay = 1000;

		if (sensorStatus)
		{
			hasNewData = readAccelData(tSensorData.Accel);  // Read the x/y/z adc values			
			hasNewData |= readGyroData(tSensorData.Gyro); 
			hasNewData |= readMagData(tSensorData.Mag);
		}
		
		now = HAL_GetMicros();				
		tSensorData.TimeElapsed = ((now - lastUpdate) / 1000000.0f); // set integration time by time elapsed since last filter update				
		if (hasNewData) // && tSensorData.TimeElapsed >= 0.001f)
		{
			//blinkDelay = 500;
			hasNewData = false;			
			lastUpdate = now;	
			quat = fuse.Fuse(&tSensorData);
		}

		if (!readyToSend)
		{
			rfStatus = RF_FifoStatus(&hspi2);			
			readyToSend = (rfStatus & RF_TX_FIFO_FULL_Bit) == 0x00; //if not full
		}
		
		if (readyToSend && HAL_GetTick() >= nextSend)
		{
			readyToSend = false;
			//send update
			RF_TransmitMode(&hspi2, NULL); //switch to tx mode
			blinkDelay = 100;
			//send continiout pos and rot data
			USBDataPacket.Header.Type = ctlSource | ROTATION_DATA;
			USBDataPacket.Header.Sequence++;
			USBDataPacket.Header.Crc8 = 0;
			USBDataPacket.Rotation.w = quat.w;
			USBDataPacket.Rotation.x = quat.x;
			USBDataPacket.Rotation.y = quat.y;
			USBDataPacket.Rotation.z = quat.z;			
			uint8_t* data = (uint8_t*)&USBDataPacket;
			uint8_t crc = 0;
			for (int i=0; i<sizeof(USBDataPacket); i++)
				crc ^= data[i];
			USBDataPacket.Header.Crc8 = crc;
			RF_SendPayload(&hspi2, data, sizeof(USBDataPacket));
			do { rfStatus = RF_FifoStatus(&hspi2); } while ((rfStatus & RF_TX_FIFO_EMPTY_Bit) != RF_TX_FIFO_EMPTY_Bit); //wait for send complete
			nextSend = HAL_GetTick() + (extraDelay + (rand() % 10));
			RF_ReceiveMode(&hspi2, NULL); //back to rx mode
		}
		else
		{
			//receive command
			rfStatus = RF_FifoStatus(&hspi2);
			if ((rfStatus & RF_RX_FIFO_EMPTY_Bit) == 0x00) //if not empty
			{
				//has fifo, receive
				RF_ReceivePayload(&hspi2, (uint8_t*)pUSBCommandPacket, sizeof(USBPacket));
									
				if ((pUSBCommandPacket->Header.Type & ctlSource) == ctlSource)
				{
					if (lastCommandSequence != pUSBCommandPacket->Header.Sequence) //discard duplicates
					{
						lastCommandSequence = pUSBCommandPacket->Header.Sequence; 
						if (pUSBCommandPacket->Command.Command == CMD_VIBRATE)
						{
							vibrationStopTime = HAL_GetTick() + pUSBCommandPacket->Command.Data.Vibration.Duration;
							HAL_GPIO_WritePin(CTL_VIBRATE_GPIO_Port, CTL_VIBRATE_Pin, GPIO_PIN_SET);							
						}
					}
				}
				pUSBCommandPacket->Header.Type = 0xff;
			}			
		}
		if (vibrationStopTime && HAL_GetTick() > vibrationStopTime)			
		{
			HAL_GPIO_WritePin(CTL_VIBRATE_GPIO_Port, CTL_VIBRATE_Pin, GPIO_PIN_RESET);
			vibrationStopTime = 0;
		}
		
	}
  /* USER CODE END WHILE */

  /* USER CODE BEGIN 3 */
  /* USER CODE END 3 */

}

/** System Clock Configuration
*/
void SystemClock_Config(void)
{

  RCC_OscInitTypeDef RCC_OscInitStruct;
  RCC_ClkInitTypeDef RCC_ClkInitStruct;
  RCC_PeriphCLKInitTypeDef PeriphClkInit;

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

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

  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_ADC;
  PeriphClkInit.AdcClockSelection = RCC_ADCPCLK2_DIV6;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }

  HAL_SYSTICK_Config(HAL_RCC_GetHCLKFreq()/1000);

  HAL_SYSTICK_CLKSourceConfig(SYSTICK_CLKSOURCE_HCLK);

  /* SysTick_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(SysTick_IRQn, 0, 0);
}

/* ADC1 init function */
static void MX_ADC1_Init(void)
{

  ADC_ChannelConfTypeDef sConfig;

    /**Common config 
    */
  hadc1.Instance = ADC1;
  hadc1.Init.ScanConvMode = ADC_SCAN_ENABLE;
  hadc1.Init.ContinuousConvMode = ENABLE;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.NbrOfConversion = 7;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

    /**Configure Regular Channel 
    */
  sConfig.Channel = ADC_CHANNEL_1;
  sConfig.Rank = 1;
  sConfig.SamplingTime = ADC_SAMPLETIME_239CYCLES_5;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }

    /**Configure Regular Channel 
    */
  sConfig.Channel = ADC_CHANNEL_2;
  sConfig.Rank = 2;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }

    /**Configure Regular Channel 
    */
  sConfig.Channel = ADC_CHANNEL_3;
  sConfig.Rank = 3;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }

    /**Configure Regular Channel 
    */
  sConfig.Channel = ADC_CHANNEL_4;
  sConfig.Rank = 4;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }

    /**Configure Regular Channel 
    */
  sConfig.Channel = ADC_CHANNEL_5;
  sConfig.Rank = 5;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }

    /**Configure Regular Channel 
    */
  sConfig.Channel = ADC_CHANNEL_6;
  sConfig.Rank = 6;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }

    /**Configure Regular Channel 
    */
  sConfig.Channel = ADC_CHANNEL_7;
  sConfig.Rank = 7;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }

}

/* I2C2 init function */
static void MX_I2C2_Init(void)
{

  hi2c2.Instance = I2C2;
  hi2c2.Init.ClockSpeed = 400000;
  hi2c2.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c2.Init.OwnAddress1 = 0;
  hi2c2.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c2.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c2.Init.OwnAddress2 = 0;
  hi2c2.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c2.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c2) != HAL_OK)
  {
    Error_Handler();
  }

}

/* SPI2 init function */
static void MX_SPI2_Init(void)
{

  hspi2.Instance = SPI2;
  hspi2.Init.Mode = SPI_MODE_MASTER;
  hspi2.Init.Direction = SPI_DIRECTION_2LINES;
  hspi2.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi2.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi2.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi2.Init.NSS = SPI_NSS_SOFT;
  hspi2.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_4;
  hspi2.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi2.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi2.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi2.Init.CRCPolynomial = 10;
  if (HAL_SPI_Init(&hspi2) != HAL_OK)
  {
    Error_Handler();
  }

}

/** 
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void) 
{
  /* DMA controller clock enable */
  __HAL_RCC_DMA1_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA1_Channel1_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel1_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel1_IRQn);

}

/** Configure pins as 
        * Analog 
        * Input 
        * Output
        * EVENT_OUT
        * EXTI
*/
static void MX_GPIO_Init(void)
{

  GPIO_InitTypeDef GPIO_InitStruct;

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, CTL_VIBRATE_Pin|SPI_RF_NSS_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(SPI_RF_CE_GPIO_Port, SPI_RF_CE_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : LED_Pin */
  GPIO_InitStruct.Pin = LED_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(LED_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : CTL_BTN1_Pin CTL_BTN2_Pin CTL_MODE2_Pin CTL_MODE1_Pin */
  GPIO_InitStruct.Pin = CTL_BTN1_Pin|CTL_BTN2_Pin|CTL_MODE2_Pin|CTL_MODE1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLDOWN;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pins : CTL_VIBRATE_Pin SPI_RF_NSS_Pin SPI_RF_CE_Pin */
  GPIO_InitStruct.Pin = CTL_VIBRATE_Pin|SPI_RF_NSS_Pin|SPI_RF_CE_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @param  None
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler */
  /* User can add his own implementation to report the HAL error return state */
	//while(1)
	//{
		BlinkRease(80, false);
		//BlinkRease(80, true);
	//}
  LedOff();
//  __HAL_RCC_SPI1_FORCE_RESET();
//  __HAL_RCC_SPI1_CLK_ENABLE();
//  __HAL_RCC_I2C2_FORCE_RESET();
//  __HAL_RCC_I2C2_CLK_ENABLE();
  NVIC_SystemReset();
  /* USER CODE END Error_Handler */ 
}

#ifdef USE_FULL_ASSERT

/**
   * @brief Reports the name of the source file and the source line number
   * where the assert_param error has occurred.
   * @param file: pointer to the source file name
   * @param line: assert_param error line source number
   * @retval None
   */
void assert_failed(uint8_t* file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
	/* User can add his own implementation to report the file name and line number,
	ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */

}

#endif

/**
  * @}
  */ 

/**
  * @}
*/ 

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/