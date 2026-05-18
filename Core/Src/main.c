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
uint32_t wave_data_64[64] = {
  2048, 2248, 2447, 2642, 2831, 3013, 3185, 3346,
  3495, 3630, 3750, 3853, 3939, 4006, 4056, 4085,
  4095, 4085, 4056, 4006, 3939, 3853, 3750, 3630,
  3495, 3346, 3185, 3013, 2831, 2642, 2447, 2248,
  2048, 1847, 1648, 1453, 1264, 1082,  910,  749,
   600,  465,  345,  242,  156,   89,   39,   10,
     0,   10,   39,   89,  156,  242,  345,  465,
   600,  749,  910, 1082, 1264, 1453, 1648, 1847
};
// 新增：64点全量程三角波数组（0 ~ 4095）
 uint32_t wave_data_triangle[64] = {
  0,  128,  256,  384,  512,  640,  768,  896, 1024, 1152, 1280, 1408, 1536, 1664, 1792, 1920,
2048, 2176, 2304, 2432, 2560, 2688, 2816, 2944, 3072, 3200, 3328, 3456, 3584, 3712, 3840, 3968,
4095, 3968, 3840, 3712, 3584, 3456, 3328, 3200, 3072, 2944, 2816, 2688, 2560, 2432, 2304, 2176,
2048, 1920, 1792, 1664, 1536, 1408, 1280, 1152, 1024,  896,  768,  640,  512,  384,  256,  128
};

#define UI_COLOR_BG      BLACK          // 屏幕背景
#define UI_COLOR_TEXT    0xAAAA         // 静态标签颜色（浅灰色）
#define UI_COLOR_VAL     0x07FF         // ADC数值颜色（亮青色）
#define UI_COLOR_SINE    GREEN          // 正弦波标签背景（绿色）
#define UI_COLOR_TRI     0xFDE0         // 三角波标签背景（橙黄色）

typedef enum {
  WAVE_MODE_SINE = 0,
  WAVE_MODE_TRIANGLE
} WaveMode_t;

WaveMode_t dac1_current_mode = WAVE_MODE_SINE;
WaveMode_t dac2_current_mode = WAVE_MODE_SINE; // DAC2 默认为正弦

uint16_t adc_buffer[1000];
uint16_t adc2_buffer[1000];    // 新增：ADC2 缓存
// -------- 新增：运行时长统计变量与颜色 --------
volatile uint16_t run_time_ms = 0;
uint8_t run_time_s = 0;
uint8_t run_time_m = 0;
uint16_t run_time_h = 0;
#define UI_COLOR_TIME    0xFFE0         // 计时器颜色（黄色）
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
  MX_ADC1_Init();
  MX_ADC2_Init();
  MX_USART2_UART_Init();
  MX_SPI2_Init();
  MX_TIM7_Init();
  MX_TIM16_Init();
  /* USER CODE BEGIN 2 */
  // ================= 外设启动代码 =================
  ST7789_Init();
  ST7789_Fill(0, 0, ST7789_WIDTH - 1, ST7789_HEIGHT - 1, UI_COLOR_BG); // 刷黑背景

  // 1. 绘制顶部 Header 条
  ST7789_Fill(0, 0, ST7789_WIDTH - 1, 32, 0x01CF); // 深青色顶栏
  ST7789_DrawString(12, 8, "STM32G474 DASHBOARD", WHITE, 0x01CF);

  // 2. 绘制静态标签 (整齐对齐)
  ST7789_DrawString(15, 55,  "ADC1 :", UI_COLOR_TEXT, UI_COLOR_BG);
  ST7789_DrawString(15, 95,  "ADC2 :", UI_COLOR_TEXT, UI_COLOR_BG);
  ST7789_DrawString(15, 135, "DAC1 :", UI_COLOR_TEXT, UI_COLOR_BG);
  ST7789_DrawString(15, 175, "DAC2 :", UI_COLOR_TEXT, UI_COLOR_BG);

  // 3. 绘制 DAC 初始状态 (默认正弦波)
  ST7789_DrawString(80, 135, "SINE Wave", UI_COLOR_SINE, UI_COLOR_BG);
  ST7789_DrawString(80, 175, "SINE Wave", UI_COLOR_SINE, UI_COLOR_BG);

  // ================= 外设常规启动 =================
  HAL_DAC_Start_DMA(&hdac1, DAC_CHANNEL_1, (uint32_t*)wave_data_64, 64, DAC_ALIGN_12B_R);
  HAL_DAC_Start_DMA(&hdac2, DAC_CHANNEL_1, (uint32_t*)wave_data_64, 64, DAC_ALIGN_12B_R);
  HAL_TIM_Base_Start(&htim7);
  HAL_ADC_Start_DMA(&hadc1, (uint32_t*)adc_buffer, 1000);
  HAL_ADC_Start_DMA(&hadc2, (uint32_t*)adc2_buffer, 1000); // 新增：启动 ADC2 的 DMA

  HAL_TIM_Base_Start_IT(&htim16); // 启动 TIM16 中断，用于计时器更新
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  char lcd_buf[32];
  float voltage1 = 0.0f;
  float voltage2 = 0.0f;       // 预留 ADC2 电压变量

  uint32_t lcd_tick = 0;
  uint32_t btn_tick = 0;       // 用于按键防抖的时间记录

  // 用于边沿检测的静态变量 (假设按键默认高电平，按下为低电平)
  static GPIO_PinState btn_last = GPIO_PIN_SET;
  while (1)
  {
    // ================= 0. 按键状态触发检测与波形切换 =================
    GPIO_PinState btn_state = HAL_GPIO_ReadPin(GPIOE, GPIO_PIN_6);

    // 检测下降沿 (当前为低电平，上次为高电平) 并且加入 50ms 软件防抖
    if (btn_state == GPIO_PIN_RESET && btn_last == GPIO_PIN_SET && (HAL_GetTick() - btn_tick > 50))
    {
      btn_tick = HAL_GetTick(); // 更新按键触发时间

      // 切换 DAC1 状态并局部刷新 UI
      if (dac1_current_mode == WAVE_MODE_SINE)
      {
        // 切换为三角波
        HAL_DAC_Stop_DMA(&hdac1, DAC_CHANNEL_1);
        HAL_DAC_Start_DMA(&hdac1, DAC_CHANNEL_1, (uint32_t*)wave_data_triangle, 64, DAC_ALIGN_12B_R);
        dac1_current_mode = WAVE_MODE_TRIANGLE;

        // 刷新 DAC1 UI 状态 (多加几个空格覆盖之前的旧字符)
        ST7789_DrawString(80, 135, "TRI  Wave", UI_COLOR_TRI, UI_COLOR_BG);
      }
      else
      {
        // 切换回正弦波
        HAL_DAC_Stop_DMA(&hdac1, DAC_CHANNEL_1);
        HAL_DAC_Start_DMA(&hdac1, DAC_CHANNEL_1, (uint32_t*)wave_data_64, 64, DAC_ALIGN_12B_R);
        dac1_current_mode = WAVE_MODE_SINE;

        // 刷新 DAC1 UI 状态
        ST7789_DrawString(80, 135, "SINE Wave", UI_COLOR_SINE, UI_COLOR_BG);
      }
    }
    // 记录本次按键状态，用于下一次循环判断边缘
    btn_last = btn_state;

    // ================= 1. 获取最新电压 =================
    voltage1 = (float)adc_buffer[0] * 3.3f / 4095.0f;
    voltage2 = (float)adc2_buffer[0] * 3.3f / 4095.0f; // 新增：计算 ADC2 的电压

    // ================= 2. LCD 数值动态刷新 (10Hz 刷新率) =================
    if (HAL_GetTick() - lcd_tick >= 100)
    {
      lcd_tick = HAL_GetTick();

      // 动态刷新 ADC1 数据
      sprintf(lcd_buf, "%.3f V  ", voltage1); // 后面的空格用于清除残留字符
      ST7789_DrawString(80, 55, lcd_buf, UI_COLOR_VAL, UI_COLOR_BG);

      // 动态刷新 ADC2 数据 (占位显示)
      sprintf(lcd_buf, "%.3f V  ", voltage2);
      ST7789_DrawString(80, 95, lcd_buf, UI_COLOR_VAL, UI_COLOR_BG);

      // -------- 新增：动态刷新运行时长 (HH:MM:SS) --------
      sprintf(lcd_buf, "%02d:%02d:%02d  ", run_time_h, run_time_m, run_time_s);
      ST7789_DrawString(80, 215, lcd_buf, UI_COLOR_TIME, UI_COLOR_BG);
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
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  // 判断是 TIM16 触发的 1ms 中断
  if (htim->Instance == TIM16)
  {
    run_time_ms++;
    if (run_time_ms >= 1000)
    {
      run_time_ms = 0;
      run_time_s++;

      if (run_time_s >= 60)
      {
        run_time_s = 0;
        run_time_m++;

        if (run_time_m >= 60)
        {
          run_time_m = 0;
          run_time_h++; // 小时数无上限累加，适合长测
        }
      }
    }
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
