/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "adc.h"
#include "dac.h"
#include "dma.h"
#include "spi.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <string.h>
#include "st7789.h"   // 包含 ST7789 LCD 驱动库头文件
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
const uint32_t wave_data_64[64] = {
  2048, 2248, 2447, 2642, 2831, 3013, 3185, 3346,
  3495, 3630, 3750, 3853, 3939, 4006, 4056, 4085,
  4095, 4085, 4056, 4006, 3939, 3853, 3750, 3630,
  3495, 3346, 3185, 3013, 2831, 2642, 2447, 2248,
  2048, 1847, 1648, 1453, 1264, 1082,  910,  749,
   600,  465,  345,  242,  156,   89,   39,   10,
     0,   10,   39,   89,  156,  242,  345,  465,
   600,  749,  910, 1082, 1264, 1453, 1648, 1847
};
uint16_t adc_buffer[1000];
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
void delay_us(uint32_t us) {
  uint32_t count = us * (170 / 4);
  while(count--) {
    __NOP();
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
  MX_DMA_Init();
  MX_DAC1_Init();
  MX_DAC2_Init();
  MX_TIM6_Init();
  MX_ADC1_Init();
  MX_ADC2_Init();
  MX_USART2_UART_Init();
  MX_SPI2_Init();
  /* USER CODE BEGIN 2 */
  // ================= 外设启动代码 =================
  // 1. 启动 DAC DMA 连续输出波形
  HAL_DAC_Start_DMA(&hdac1, DAC_CHANNEL_1, (uint32_t*)wave_data_64, 64, DAC_ALIGN_12B_R);
  HAL_DAC_Start_DMA(&hdac2, DAC_CHANNEL_1, (uint32_t*)wave_data_64, 64, DAC_ALIGN_12B_R);

  // 2. 启动 TIM6 定时器触发 DAC
  HAL_TIM_Base_Start(&htim6);

  // 3. 启动 ADC1 的 DMA 连续搬运
  HAL_ADC_Start_DMA(&hadc1, (uint32_t*)adc_buffer, 1000);

  // ================= LCD 屏幕开机测试 =================
  ST7789_Init(); // 初始化 LCD

  // 刷全屏红色测试 (等待 500ms)
  ST7789_Fill(0, 0, ST7789_WIDTH - 1, ST7789_HEIGHT - 1, RED);
  HAL_Delay(500);

  // 刷全屏绿色测试 (等待 500ms)
  ST7789_Fill(0, 0, ST7789_WIDTH - 1, ST7789_HEIGHT - 1, GREEN);
  HAL_Delay(500);

  // 刷全屏蓝色测试 (等待 500ms)
  ST7789_Fill(0, 0, ST7789_WIDTH - 1, ST7789_HEIGHT - 1, BLUE);
  HAL_Delay(500);

  // 测试完成，清屏为黑色，准备进入主循环
  ST7789_Fill(0, 0, ST7789_WIDTH - 1, ST7789_HEIGHT - 1, BLACK);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  char vofa_buf[64];
  char lcd_buf[32];            // 新增：用于格式化屏幕显示的字符串
  float voltage1 = 0.0f;
  uint32_t lcd_tick = 0;       // 新增：用于记录上一次刷屏的时间戳

  while (1)
  {
      // ================= 1. 获取最新电压 =================
      // 从 DMA 缓冲区获取最新的 ADC 值并转换为电压
      voltage1 = (float)adc_buffer[0] * 3.3f / 4095.0f;

      // ================= 2. 串口发给 Vofa+ =================
      // 防止 DMA 被过快覆盖，判断状态再发
      if (huart2.gState == HAL_UART_STATE_READY)
      {
          sprintf(vofa_buf, "%.3f\n", voltage1);
          HAL_UART_Transmit_DMA(&huart2, (uint8_t*)vofa_buf, strlen(vofa_buf));
      }

      // ================= 3. LCD 屏幕显示 (10Hz 刷新率) =================
      if (HAL_GetTick() - lcd_tick >= 100)
      {
          lcd_tick = HAL_GetTick(); // 更新时间戳

          sprintf(lcd_buf, "ADC: %.3f V", voltage1);

          // 在 (10, 10) 坐标处显示电压，白字黑底
          ST7789_DrawString(10, 10, lcd_buf, WHITE, BLACK);
          ST7789_DrawString(10, 40, lcd_buf, WHITE, BLACK);
      }

    /* USER CODE END WHILE */

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
  HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1_BOOST);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = RCC_PLLM_DIV4;
  RCC_OscInitStruct.PLL.PLLN = 85;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
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
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc)
{
  if (hadc->Instance == ADC1)
  {
      // 保持为空即可
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
