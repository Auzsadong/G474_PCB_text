#include "st7789.h"
#include "fonts.h"

/* ================= 新增：DMA 等待函数 ================= */
// 这是一个内联等待函数，防止 CS 提前拉高或局部数组被销毁
static inline void ST7789_WaitDMA(void)
{
    // 等待 SPI 发送状态恢复为空闲
    while (HAL_SPI_GetState(&hspi2) != HAL_SPI_STATE_READY)
    {
        // 裸机状态下在这里死等。
        // 如果以后你跑了 RTOS，这里可以换成 osDelay 等待信号量，把 CPU 让给其他任务！
    }
}
/* ====================================================== */

/* 发送命令 (短数据保持阻塞式发送，效率更高) */
static void ST7789_WriteCommand(uint8_t cmd)
{
    ST7789_CS_CLR();
    ST7789_DC_CMD();
    HAL_SPI_Transmit(&hspi2, &cmd, 1, HAL_MAX_DELAY);
    ST7789_CS_SET();
}

/* 发送8位数据 (短数据保持阻塞式发送) */
static void ST7789_WriteData(uint8_t data)
{
    ST7789_CS_CLR();
    ST7789_DC_DATA();
    HAL_SPI_Transmit(&hspi2, &data, 1, HAL_MAX_DELAY);
    ST7789_CS_SET();
}

/* 发送16位数据 (RGB565颜色) (短数据保持阻塞式发送) */
static void ST7789_WriteData16(uint16_t data)
{
    uint8_t buf[2];
    buf[0] = data >> 8;
    buf[1] = data & 0xFF;
    ST7789_CS_CLR();
    ST7789_DC_DATA();
    HAL_SPI_Transmit(&hspi2, buf, 2, HAL_MAX_DELAY);
    ST7789_CS_SET();
}

/* 硬件复位 */
static void ST7789_HardReset(void)
{
    ST7789_RST_CLR();
    HAL_Delay(100);
    ST7789_RST_SET();
    HAL_Delay(120);
}

/* 屏幕初始化序列 */
void ST7789_Init(void)
{
    // 1. 硬件复位
    ST7789_HardReset();

    // 2. 软件复位
    ST7789_WriteCommand(0x01);
    HAL_Delay(150);

    // 3. 退出睡眠模式
    ST7789_WriteCommand(0x11);
    HAL_Delay(120);

    // 4. 颜色模式设置 (RGB565, 16-bit/pixel)
    ST7789_WriteCommand(0x3A);
    ST7789_WriteData(0x05);

    // 5. 显示方向设置 (Memory Data Access Control)
    ST7789_WriteCommand(0x36);
#if USE_HORIZONTAL == 0
    ST7789_WriteData(0x00); // 竖屏
#elif USE_HORIZONTAL == 1
    // 横屏方向 1
    ST7789_WriteData(0x60);
#elif USE_HORIZONTAL == 2
    ST7789_WriteData(0xC0); // 竖屏翻转 180°
#elif USE_HORIZONTAL == 3
    // 横屏方向 2 (翻转 180°)
    ST7789_WriteData(0xA0);
#endif

    // 6. 开启颜色反转 (ST7789 面板特有，如果不加可能导致颜色显示相反)
    ST7789_WriteCommand(0x20);

    // 7. 开启显示
    ST7789_WriteCommand(0x29);
    HAL_Delay(10);

    // 8. 打开背光
    ST7789_BLK_ON();
}

/* 设置写入窗口的坐标 */
void ST7789_SetAddressWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    // 设置列地址 (X坐标)
    ST7789_WriteCommand(0x2A);
    ST7789_WriteData16(x0 + ST7789_XSTART);
    ST7789_WriteData16(x1 + ST7789_XSTART);

    // 设置行地址 (Y坐标)
    ST7789_WriteCommand(0x2B);
    ST7789_WriteData16(y0 + ST7789_YSTART);
    ST7789_WriteData16(y1 + ST7789_YSTART);

    // 发送内存写入命令
    ST7789_WriteCommand(0x2C);
}

/* 画一个像素点 */
void ST7789_DrawPixel(uint16_t x, uint16_t y, uint16_t color)
{
    if (x >= ST7789_WIDTH || y >= ST7789_HEIGHT) return;
    ST7789_SetAddressWindow(x, y, x, y);
    ST7789_WriteData16(color);
}

/* 填充指定区域 (纯色刷屏) - 已升级 DMA */
void ST7789_Fill(uint16_t xsta, uint16_t ysta, uint16_t xend, uint16_t yend, uint16_t color)
{
    uint32_t i, j;
    uint16_t width = xend - xsta + 1;
    uint16_t height = yend - ysta + 1;

    // 准备一行的数据缓冲区，提高 SPI 传输效率
    uint16_t line_buf[ST7789_WIDTH];
    for (i = 0; i < width; i++)
    {
        // 调整字节序，适应 SPI 传输 (大端模式)
        line_buf[i] = (color >> 8) | (color << 8);
    }

    ST7789_SetAddressWindow(xsta, ysta, xend, yend);

    ST7789_CS_CLR();
    ST7789_DC_DATA();

    // 逐行发送数据
    for (j = 0; j < height; j++)
    {
        // 采用 DMA 发送
        HAL_SPI_Transmit_DMA(&hspi2, (uint8_t*)line_buf, width * 2);

        // 必须等待本行发送完毕才能进行下一次循环或拉高 CS
        ST7789_WaitDMA();
    }

    ST7789_CS_SET();
}

/**
 * @brief  在屏幕上绘制一个 16x24 的字符 - 已升级 DMA
 * @param  x: 起始 X 坐标
 * @param  y: 起始 Y 坐标
 * @param  ch: 要显示的字符 (ASCII码)
 * @param  color: 字体颜色
 * @param  bg_color: 背景颜色
 */
void ST7789_DrawChar(uint16_t x, uint16_t y, char ch, uint16_t color, uint16_t bg_color)
{
    // 边界检查
    if (x > ST7789_WIDTH - 16 || y > ST7789_HEIGHT - 24) return;

    // 过滤掉不支持的非打印字符 (只支持 ' ' 到 '~')
    if (ch < ' ' || ch > '~') ch = ' ';

    // 计算该字符在字库数组中的起始索引
    uint32_t font_index = (ch - ' ') * 24;

    // 准备一个缓冲区，一次性存入 16x24 个像素的数据
    uint16_t color_buf[16 * 24];
    uint16_t line_data;

    for (int i = 0; i < 24; i++) {           // 遍历 24 行
        line_data = ASCII_Table[font_index + i]; // 获取当前行的数据 (16 bit)

        for (int j = 0; j < 16; j++) {       // 遍历这一行的 16 个像素

            // 从低位向高位读取，完美适配该字库
            if (line_data & (1 << j)) {
                color_buf[i * 16 + j] = (color >> 8) | (color << 8);    // 字体色
            } else {
                color_buf[i * 16 + j] = (bg_color >> 8) | (bg_color << 8); // 背景色
            }
        }
    }

    // 设置写入窗口为 16x24
    ST7789_SetAddressWindow(x, y, x + 15, y + 23);
    ST7789_CS_CLR();
    ST7789_DC_DATA();

    // 使用 DMA 异步发送整个字符块 (大小 = 16 * 24 * 2 Bytes)
    HAL_SPI_Transmit_DMA(&hspi2, (uint8_t*)color_buf, 16 * 24 * 2);

    // 👇 极其关键：等待 DMA 把局部的 color_buf 发送完毕！
    // 如果不等待直接退出函数，color_buf 就会被栈销毁变成乱码，且 CS 会被提前拉高
    ST7789_WaitDMA();

    ST7789_CS_SET();
}

/**
 * @brief  在屏幕上绘制连续的字符串
 */
void ST7789_DrawString(uint16_t x, uint16_t y, const char* str, uint16_t color, uint16_t bg_color)
{
    while (*str != '\0')
    {
        ST7789_DrawChar(x, y, *str, color, bg_color);
        x += 16; // 16x24 字体，下一个字的起始X坐标加16

        // 如果到了屏幕边缘，自动换行
        if (x > ST7789_WIDTH - 16)
        {
            x = 0;
            y += 24; // 换行，Y坐标加24
        }
        str++;
    }
}