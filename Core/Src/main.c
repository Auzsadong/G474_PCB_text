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
#include "rtc.h"
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
#define BUTTON_PORT      GPIOE
#define BUTTON_PIN       GPIO_PIN_6
#define BUTTON_DEBOUNCE_MS 50U
#define BUTTON_LONG_MS   5000U

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


/* 增加 printf 到 UART1 的重定向支持 */
#ifdef __GNUC__
#define PUTCHAR_PROTOTYPE int __io_putchar(int ch)
#else
#define PUTCHAR_PROTOTYPE int fputc(int ch, FILE *f)
#endif
PUTCHAR_PROTOTYPE
{
  /* 将 printf 输出重定向到 USART1 */
  HAL_UART_Transmit(&huart1, (uint8_t *)&ch, 1, HAL_MAX_DELAY);
  return ch;
}

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
static uint8_t Button_IsPressed(void);
static void Dashboard_DrawStatic(void);
static void Dashboard_ToggleDac1Wave(void);
static void ScreenRefreshTest_Run(void);

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
void delay_us(uint32_t us) {
  uint32_t count = us * (170 / 4);
  while(count--) {
    __NOP();
  }
}

static uint8_t Button_IsPressed(void)
{
  return HAL_GPIO_ReadPin(BUTTON_PORT, BUTTON_PIN) == GPIO_PIN_RESET;
}

static void Dashboard_DrawStatic(void)
{
  ST7789_Fill(0, 0, ST7789_WIDTH - 1, ST7789_HEIGHT - 1, UI_COLOR_BG);

  ST7789_Fill(0, 0, ST7789_WIDTH - 1, 32, 0x01CF);
  ST7789_DrawString(12, 8, "STM32G474 DASHBOARD", WHITE, 0x01CF);

  ST7789_DrawString(15, 55,  "ADC1 :", UI_COLOR_TEXT, UI_COLOR_BG);
  ST7789_DrawString(15, 95,  "ADC2 :", UI_COLOR_TEXT, UI_COLOR_BG);
  ST7789_DrawString(15, 135, "DAC1 :", UI_COLOR_TEXT, UI_COLOR_BG);
  ST7789_DrawString(15, 175, "DAC2 :", UI_COLOR_TEXT, UI_COLOR_BG);
  ST7789_DrawString(15, 215, "RTC  :", UI_COLOR_TEXT, UI_COLOR_BG);

  ST7789_DrawString(80, 135,
                    dac1_current_mode == WAVE_MODE_SINE ? "SINE Wave" : "TRI  Wave",
                    dac1_current_mode == WAVE_MODE_SINE ? UI_COLOR_SINE : UI_COLOR_TRI,
                    UI_COLOR_BG);
  ST7789_DrawString(80, 175,
                    dac2_current_mode == WAVE_MODE_SINE ? "SINE Wave" : "TRI  Wave",
                    dac2_current_mode == WAVE_MODE_SINE ? UI_COLOR_SINE : UI_COLOR_TRI,
                    UI_COLOR_BG);
}

static void Dashboard_ToggleDac1Wave(void)
{
  if (dac1_current_mode == WAVE_MODE_SINE)
  {
    HAL_DAC_Stop_DMA(&hdac1, DAC_CHANNEL_1);
    HAL_DAC_Start_DMA(&hdac1, DAC_CHANNEL_1, (uint32_t*)wave_data_triangle, 64, DAC_ALIGN_12B_R);
    dac1_current_mode = WAVE_MODE_TRIANGLE;
    ST7789_DrawString(80, 135, "TRI  Wave", UI_COLOR_TRI, UI_COLOR_BG);
  }
  else
  {
    HAL_DAC_Stop_DMA(&hdac1, DAC_CHANNEL_1);
    HAL_DAC_Start_DMA(&hdac1, DAC_CHANNEL_1, (uint32_t*)wave_data_64, 64, DAC_ALIGN_12B_R);
    dac1_current_mode = WAVE_MODE_SINE;
    ST7789_DrawString(80, 135, "SINE Wave", UI_COLOR_SINE, UI_COLOR_BG);
  }
}

static void ScreenRefreshTest_Run(void)
{
  static const uint16_t colors[] = {
    BLACK, WHITE, RED, GREEN, BLUE, CYAN, MAGENTA, YELLOW
  };
  uint32_t last_step = 0;
  uint8_t step = 0;
  uint8_t exit_press_seen = 0;

  printf("[INFO] Enter screen refresh test. Click PE6 to exit.\r\n");

  while (Button_IsPressed())
  {
    HAL_Delay(10);
  }

  ST7789_Fill(0, 0, ST7789_WIDTH - 1, ST7789_HEIGHT - 1, BLACK);
  ST7789_DrawString(16, 40, "LCD REFRESH TEST", WHITE, BLACK);
  ST7789_DrawString(16, 80, "CLICK PE6 TO EXIT", YELLOW, BLACK);
  HAL_Delay(500);

  while (1)
  {
    uint32_t now = HAL_GetTick();

    if ((now - last_step) >= 250U)
    {
      last_step = now;

      if ((step % 3U) == 0U)
      {
        uint16_t color = colors[(step / 3U) % (sizeof(colors) / sizeof(colors[0]))];
        ST7789_Fill(0, 0, ST7789_WIDTH - 1, ST7789_HEIGHT - 1, color);
        ST7789_DrawString(16, 16, "FULL FILL", color == BLACK ? WHITE : BLACK, color);
      }
      else if ((step % 3U) == 1U)
      {
        uint16_t band_w = ST7789_WIDTH / 8U;
        for (uint8_t i = 0; i < 8U; i++)
        {
          uint16_t x0 = i * band_w;
          uint16_t x1 = (i == 7U) ? (ST7789_WIDTH - 1U) : (x0 + band_w - 1U);
          ST7789_Fill(x0, 0, x1, ST7789_HEIGHT - 1U, colors[i]);
        }
      }
      else
      {
        uint16_t color_a = colors[(step / 3U) % (sizeof(colors) / sizeof(colors[0]))];
        uint16_t color_b = colors[((step / 3U) + 4U) % (sizeof(colors) / sizeof(colors[0]))];
        for (uint16_t y = 0; y < ST7789_HEIGHT; y += 24U)
        {
          for (uint16_t x = 0; x < ST7789_WIDTH; x += 24U)
          {
            uint16_t x1 = (x + 23U >= ST7789_WIDTH) ? (ST7789_WIDTH - 1U) : (x + 23U);
            uint16_t y1 = (y + 23U >= ST7789_HEIGHT) ? (ST7789_HEIGHT - 1U) : (y + 23U);
            ST7789_Fill(x, y, x1, y1, (((x + y) / 24U) & 1U) ? color_a : color_b);
          }
        }
      }

      step++;
    }

    if (Button_IsPressed())
    {
      exit_press_seen = 1;
    }
    else if (exit_press_seen)
    {
      HAL_Delay(BUTTON_DEBOUNCE_MS);
      if (!Button_IsPressed())
      {
        printf("[INFO] Exit screen refresh test.\r\n");
        return;
      }
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
  MX_DMA_Init();
  MX_DAC1_Init();
  MX_DAC2_Init();
  MX_ADC1_Init();
  MX_ADC2_Init();
  MX_USART2_UART_Init();
  MX_SPI2_Init();
  MX_TIM7_Init();
  MX_TIM16_Init();
  MX_USART1_UART_Init();
  MX_RTC_Init();
  /* USER CODE BEGIN 2 */
  // ================= 1. 系统启动与日志 =================
  printf("\r\n========================================\r\n");
  printf("      STM32G474 Dashboard Booting...      \r\n");
  printf("========================================\r\n");
  printf("[OK] Hardware Peripherals Initialized\r\n");

  // ================= 2. LCD 初始化与 UI 静态绘制 =================
  ST7789_Init();
  printf("[OK] ST7789 LCD Driver Started\r\n");

  Dashboard_DrawStatic();

  // ================= 3. TIM16 配置与 GPIO 相位初始化 =================
  // 强制接管 TIM16 配置：假设主频 170MHz，设为 500us 中断周期，从而产生 1KHz 方波
  TIM16->PSC = 170 - 1;       // 预分频器：1MHz 计数频率 (1us步进)
  TIM16->ARR = 500 - 1;       // 自动重装载：500us
  TIM16->EGR = TIM_EGR_UG;    // 触发更新事件使配置生效

  // 统一利用 BSRR (Bit Set/Reset Register) 高 16 位清零对应引脚
  // 确保所有用作 1KHz 方波输出的 IO 口起始电平全部一致为低电平
  GPIOA->BSRR = (uint32_t)0x99A0 << 16;
  GPIOB->BSRR = (uint32_t)0x0FFF << 16;
  GPIOC->BSRR = (uint32_t)0x3FFF << 16;
  GPIOD->BSRR = (uint32_t)0xECFF << 16;
  GPIOE->BSRR = (uint32_t)0xFFDF << 16;
  GPIOF->BSRR = (uint32_t)0x0604 << 16;
  GPIOG->BSRR = (uint32_t)0x0400 << 16;

  // ================= 4. 启动 DMA、转换器与核心定时器 =================
  printf("[INFO] Starting DMA, ADC, DAC & Timers...\r\n");

  HAL_DAC_Start_DMA(&hdac1, DAC_CHANNEL_1, (uint32_t*)wave_data_64, 64, DAC_ALIGN_12B_R);
  HAL_DAC_Start_DMA(&hdac2, DAC_CHANNEL_1, (uint32_t*)wave_data_64, 64, DAC_ALIGN_12B_R);
  HAL_TIM_Base_Start(&htim7);
  HAL_ADC_Start_DMA(&hadc1, (uint32_t*)adc_buffer, 1000);
  HAL_ADC_Start_DMA(&hadc2, (uint32_t*)adc2_buffer, 1000);

  HAL_TIM_Base_Start_IT(&htim16); // 启动已重写为 500us 的中断节拍

  printf("[INFO] System Boot Complete. Entering Main Loop.\r\n");
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  char lcd_buf[32];
  float voltage1 = 0.0f;
  float voltage2 = 0.0f;

  uint32_t lcd_tick = 0;
  uint32_t button_down_tick = 0;
  uint8_t button_was_down = 0;
  uint8_t button_long_handled = 0;

  while (1)
  {
    // ----------------- [1. 异步事件检测 (按键防抖)] -----------------
    uint32_t now = HAL_GetTick();
    uint8_t button_down = Button_IsPressed();

    if (button_down && !button_was_down)
    {
      button_down_tick = now;
      button_long_handled = 0;
    }
    else if (button_down && !button_long_handled && ((now - button_down_tick) >= BUTTON_LONG_MS))
    {
      button_long_handled = 1;
      ScreenRefreshTest_Run();
      Dashboard_DrawStatic();
      lcd_tick = 0;
    }
    else if (!button_down && button_was_down)
    {
      uint32_t press_time = now - button_down_tick;
      if (!button_long_handled && press_time >= BUTTON_DEBOUNCE_MS)
      {
        Dashboard_ToggleDac1Wave();
      }
    }
    button_was_down = button_down;

    // ----------------- [2. 传感器数据处理] -----------------
    voltage1 = (float)adc_buffer[0] * 3.3f / 4095.0f;
    voltage2 = (float)adc2_buffer[0] * 3.3f / 4095.0f;

    // ----------------- [3. LCD 界面 10Hz 动态刷新] -----------------
    if (HAL_GetTick() - lcd_tick >= 100)
    {
      lcd_tick = HAL_GetTick();

      sprintf(lcd_buf, "%.3f V  ", voltage1);
      ST7789_DrawString(80, 55, lcd_buf, UI_COLOR_VAL, UI_COLOR_BG);

      sprintf(lcd_buf, "%.3f V  ", voltage2);
      ST7789_DrawString(80, 95, lcd_buf, UI_COLOR_VAL, UI_COLOR_BG);

      RTC_TimeTypeDef rtc_time = {0};
      RTC_DateTypeDef rtc_date = {0};
      HAL_RTC_GetTime(&hrtc, &rtc_time, RTC_FORMAT_BIN);
      HAL_RTC_GetDate(&hrtc, &rtc_date, RTC_FORMAT_BIN);
      sprintf(lcd_buf, "%02d:%02d:%02d  ", rtc_time.Hours, rtc_time.Minutes, rtc_time.Seconds);
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

  /** Configure LSE Drive Capability
  */
  HAL_PWR_EnableBkUpAccess();
  __HAL_RCC_LSEDRIVE_CONFIG(RCC_LSEDRIVE_LOW);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI|RCC_OSCILLATORTYPE_LSE;
  RCC_OscInitStruct.LSEState = RCC_LSE_ON;
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
  // 判断是 TIM16 触发的 500us 中断
  if (htim->Instance == TIM16)
  {
    // 1. 每 500us 翻转一次所有指定的 GPIO_Output 引脚，产生精确的 1KHz 方波
    GPIOA->ODR ^= 0x99A0;
    GPIOB->ODR ^= 0x0FFF;
    GPIOC->ODR ^= 0x3FFF;
    GPIOD->ODR ^= 0xECFF;
    GPIOE->ODR ^= 0xFFDF;
    GPIOF->ODR ^= 0x0604;
    GPIOG->ODR ^= 0x0400;

    // 2. 软件分频：500us * 2 = 1ms，维持原有的屏幕仪表盘计时器逻辑
    static uint8_t ms_divider = 0;
    ms_divider++;
    if (ms_divider >= 2)
    {
      ms_divider = 0;
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
