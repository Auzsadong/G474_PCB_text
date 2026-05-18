#ifndef __ST7789_H
#define __ST7789_H

#include "main.h"
#include "spi.h"

/* ---------------- 屏幕显示方向设置 ---------------- */
// 0: 竖屏 (Portrait)
// 1: 横屏 (Landscape)
// 2: 竖屏翻转180° (Portrait 180)
// 3: 横屏翻转180° (Landscape 180)
#define USE_HORIZONTAL 1  // <--- 只需要修改这个数字即可一键切换方向！

/* ---------------- 屏幕分辨率自动配置 ---------------- */
// 基础面板分辨率 (对于2.8寸通常是 240x320)
#define ST7789_BASE_WIDTH  240
#define ST7789_BASE_HEIGHT 320

// 根据方向自动计算当前宽度和高度
#if USE_HORIZONTAL == 0 || USE_HORIZONTAL == 2
    #define ST7789_WIDTH   ST7789_BASE_WIDTH
    #define ST7789_HEIGHT  ST7789_BASE_HEIGHT
#else
    #define ST7789_WIDTH   ST7789_BASE_HEIGHT
    #define ST7789_HEIGHT  ST7789_BASE_WIDTH
#endif

// 屏幕偏移量 (通常不需要)
#define ST7789_XSTART 0
#define ST7789_YSTART 0

/* ---------------- 硬件引脚定义 ---------------- */
extern SPI_HandleTypeDef hspi2;

#define ST7789_CS_PORT    GPIOB
#define ST7789_CS_PIN     GPIO_PIN_12
#define ST7789_DC_PORT    GPIOD
#define ST7789_DC_PIN     GPIO_PIN_8
#define ST7789_RST_PORT   GPIOD
#define ST7789_RST_PIN    GPIO_PIN_9
#define ST7789_BLK_PORT   GPIOD
#define ST7789_BLK_PIN    GPIO_PIN_12

/* ---------------- 常用引脚操作宏 ---------------- */
#define ST7789_CS_CLR()   HAL_GPIO_WritePin(ST7789_CS_PORT, ST7789_CS_PIN, GPIO_PIN_RESET)
#define ST7789_CS_SET()   HAL_GPIO_WritePin(ST7789_CS_PORT, ST7789_CS_PIN, GPIO_PIN_SET)

#define ST7789_DC_CMD()   HAL_GPIO_WritePin(ST7789_DC_PORT, ST7789_DC_PIN, GPIO_PIN_RESET)
#define ST7789_DC_DATA()  HAL_GPIO_WritePin(ST7789_DC_PORT, ST7789_DC_PIN, GPIO_PIN_SET)

#define ST7789_RST_CLR()  HAL_GPIO_WritePin(ST7789_RST_PORT, ST7789_RST_PIN, GPIO_PIN_RESET)
#define ST7789_RST_SET()  HAL_GPIO_WritePin(ST7789_RST_PORT, ST7789_RST_PIN, GPIO_PIN_SET)

#define ST7789_BLK_OFF()  HAL_GPIO_WritePin(ST7789_BLK_PORT, ST7789_BLK_PIN, GPIO_PIN_RESET)
#define ST7789_BLK_ON()   HAL_GPIO_WritePin(ST7789_BLK_PORT, ST7789_BLK_PIN, GPIO_PIN_SET)

/* ---------------- 常用颜色定义 (RGB565) ---------------- */
#define WHITE       0xFFFF
#define BLACK       0x0000
#define BLUE        0x001F
#define BRED        0XF81F
#define GRED        0XFFE0
#define GBLUE       0X07FF
#define RED         0xF800
#define MAGENTA     0xF81F
#define GREEN       0x07E0
#define CYAN        0x7FFF
#define YELLOW      0xFFE0

/* ---------------- 驱动函数声明 ---------------- */
void ST7789_Init(void);
void ST7789_SetAddressWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);
void ST7789_DrawPixel(uint16_t x, uint16_t y, uint16_t color);
void ST7789_Fill(uint16_t xsta, uint16_t ysta, uint16_t xend, uint16_t yend, uint16_t color);
void ST7789_DrawChar(uint16_t x, uint16_t y, char ch, uint16_t color, uint16_t bg_color);
void ST7789_DrawString(uint16_t x, uint16_t y, const char *str, uint16_t color, uint16_t bg_color);
#endif