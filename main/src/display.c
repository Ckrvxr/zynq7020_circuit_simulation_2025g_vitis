#include "display.h"

#include "xil_types.h"
#include "xiicps.h"

#include "FreeRTOS.h"
#include "task.h"

#include <stdio.h>
#include <math.h>

#include "u8g2.h"

#include "dds.h"
#include "fir.h"
/* -------------------------------------------------------- Driver -------------------------------------------------------- */
static XIicPs IicInstance;
static u8g2_t u8g2;

// 用于缓存 u8g2 传输的 I2C 数据流，128 字节对于单次页/数据传输通常已足够
static uint8_t i2c_buffer[128];
static uint8_t i2c_buf_idx = 0;

// u8g2 的 I2C 硬件通信回调
static uint8_t u8x8_byte_zynq_hw_i2c(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr) {
    uint8_t *data;
    switch(msg) {
        case U8X8_MSG_BYTE_SEND:
            data = (uint8_t *)arg_ptr;
            while(arg_int > 0) {
                if (i2c_buf_idx < sizeof(i2c_buffer)) {
                    i2c_buffer[i2c_buf_idx++] = *data;
                }
                data++;
                arg_int--;
            }
            break;
        case U8X8_MSG_BYTE_START_TRANSFER:
            i2c_buf_idx = 0;
            break;
        case U8X8_MSG_BYTE_END_TRANSFER:
            // u8g2 默认传进来的地址是 8-bit (例如 0x78)，而 Zynq XIicPs 默认使用 7-bit 地址，所以要右移 1 位 (变成 0x3C)
            XIicPs_MasterSendPolled(&IicInstance, i2c_buffer, i2c_buf_idx, u8x8_GetI2CAddress(u8x8) >> 1);
            while (XIicPs_BusIsBusy(&IicInstance));
            break;
        default:
            return 0;
    }
    return 1;
}

// u8g2 的延时和 GPIO 回调
static uint8_t u8x8_gpio_and_delay_zynq(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr) {
    switch(msg) {
        case U8X8_MSG_DELAY_MILLI:
            vTaskDelay(pdMS_TO_TICKS(arg_int)); // 使用 FreeRTOS 延时替代死等
            break;
        default:
            return 0;
    }
    return 1;
}

void Display_Init(void) {
    // 1. 初始化 Zynq I2C 硬件
    XIicPs_Config *Config = XIicPs_LookupConfig(XPAR_PS7_I2C_0_DEVICE_ID);
    XIicPs_CfgInitialize(&IicInstance, Config, Config->BaseAddress);
    XIicPs_SetSClk(&IicInstance, 400000);
    vTaskDelay(pdMS_TO_TICKS(100));
    // 2. 初始化 u8g2，如果是 SSD1306 屏幕可以直接把 ssd1315 换成 ssd1306
    u8g2_Setup_ssd1315_i2c_128x64_noname_f(&u8g2, U8G2_R0, u8x8_byte_zynq_hw_i2c, u8x8_gpio_and_delay_zynq);
    u8g2_InitDisplay(&u8g2);
    u8g2_SetPowerSave(&u8g2, 0); // 唤醒屏幕
}

/* -------------------------------------------------------- Scene -------------------------------------------------------- */
volatile DisplayState_t currentState = STATE_MAIN_MENU;
volatile uint8_t menu_index = 1;
volatile uint8_t slect_index = 0;
uint32_t frame_count = 0;

static void Format_With_Commas(uint32_t n, char *out_buf) {
    char temp[32];
    int len = snprintf(temp, sizeof(temp), "%u", (unsigned int)n);
    int commas = (len - 1) / 3;
    int out_len = len + commas;
    int i = len - 1, j = out_len - 1, count = 0;

    out_buf[out_len] = '\0';
    while (i >= 0) {
        if (count > 0 && count % 3 == 0) {
        out_buf[j--] = ',';
        }
        out_buf[j--] = temp[i--];
        count++;
    }
}

static void Display_Draw_LiveAnimation(int x, int y) {
    u8g2_SetDrawColor(&u8g2, 1);
    uint8_t phase = (frame_count / 2) % 4;
    switch (phase) {
        case 0: u8g2_DrawLine(&u8g2, x-3, y,   x+3, y);   break;
        case 1: u8g2_DrawLine(&u8g2, x-2, y-2, x+2, y+2); break;
        case 2: u8g2_DrawLine(&u8g2, x,   y-3, x,   y+3); break;
        case 3: u8g2_DrawLine(&u8g2, x+2, y-2, x-2, y+2); break;
    }
    if ((frame_count / 8) % 2) {
        u8g2_DrawPixel(&u8g2, x-5, y-5);
        u8g2_DrawPixel(&u8g2, x+5, y+5);
    }
}

static void Display_Draw_Cursor(int y, uint8_t is_selected, uint8_t is_editing) {
    if (!is_selected) return;
    int8_t offset = (frame_count / 4) % 4;
    if (offset > 2) offset = 4 - offset; 
    if (is_editing) {
        if ((frame_count / 4) % 2) u8g2_DrawStr(&u8g2, 2, y, "*");
    } else {
        u8g2_DrawStr(&u8g2, 5 + offset, y, ">");
    }
}

static void Display_Draw_WaveMini(int x, int y, int w, int h) {
    u8g2_DrawFrame(&u8g2, x, y, w, h);
    static const int8_t sin_table[16] = {0, 48, 90, 117, 127, 117, 90, 48, 0, -48, -90, -117, -127, -117, -90, -48};
    for (int i = 1; i < w - 1; i++) {
        uint8_t idx = ((i >> 2) + frame_count) % 16; 
        int py = y + (h / 2) + (sin_table[idx] * (h / 2 - 2) / 128);
        u8g2_DrawPixel(&u8g2, x + i, py);
    }
}

static void Display_Draw_MainMenu(void) {
    u8g2_SetFont(&u8g2, u8g2_font_ncenB08_tr);
    u8g2_DrawStr(&u8g2, 0, 10, "[Main Menu]");
    Display_Draw_LiveAnimation(120, 5);
    u8g2_DrawHLine(&u8g2, 0, 14, 128);

    Display_Draw_Cursor(30, (menu_index == 1), 0);
    u8g2_DrawStr(&u8g2, 18, 30, "DDS Mode");

    Display_Draw_Cursor(45, (menu_index == 2), 0);
    u8g2_DrawStr(&u8g2, 18, 45, "FIR Mode");
}

static void Display_Draw_DDSMode(void) {
    extern volatile uint32_t dds_vpp;
    extern volatile uint32_t dds_freq;
    char buf[64];
    char formatted_val[32];

    u8g2_SetFont(&u8g2, u8g2_font_ncenB08_tr);
    u8g2_DrawStr(&u8g2, 0, 10, "[DDS Mode]");
    Display_Draw_LiveAnimation(120, 5);
    u8g2_DrawHLine(&u8g2, 0, 14, 128);

    u8g2_DrawStr(&u8g2, 15, 28, "Wave:"); 
    Display_Draw_WaveMini(55, 18, 45, 12);

    Display_Draw_Cursor(43, (menu_index == 1), (slect_index == 1));
    u8g2_DrawStr(&u8g2, 15, 43, "Vpp:");
    Format_With_Commas(dds_vpp, formatted_val);
    snprintf(buf, sizeof(buf), "%s mV", formatted_val);
    u8g2_DrawStr(&u8g2, 55, 43, buf);

    Display_Draw_Cursor(58, (menu_index == 2), (slect_index == 2));
    u8g2_DrawStr(&u8g2, 15, 58, "Freq:");
    Format_With_Commas(dds_freq, formatted_val);
    snprintf(buf, sizeof(buf), "%s Hz", formatted_val);
    u8g2_DrawStr(&u8g2, 55, 58, buf);
}

static void Display_Draw_FIRMode(void) {
    u8g2_SetFont(&u8g2, u8g2_font_ncenB08_tr);
    u8g2_DrawStr(&u8g2, 0, 10, "[FIR Mode]");
    Display_Draw_LiveAnimation(120, 5);
    u8g2_DrawHLine(&u8g2, 0, 14, 128);

    u8g2_DrawStr(&u8g2, 15, 30, "Type: Band-Stop");

    Display_Draw_Cursor(45, (menu_index == 1), (slect_index == 1));
    u8g2_DrawStr(&u8g2, 15, 45, "Start Learning");
}

static void Display_Draw_FIRModeLearningProgress(void) {
    char buf[64];
    char fmt[32];

    u8g2_SetFont(&u8g2, u8g2_font_ncenB08_tr);
    u8g2_DrawStr(&u8g2, 0, 10, "[Learning...]");
    Display_Draw_LiveAnimation(120, 5);
    u8g2_DrawHLine(&u8g2, 0, 14, 128);

    u8g2_DrawFrame(&u8g2, 10, 20, 108, 8);
    if (fir_progress > 0) {
        uint8_t bar_w = (uint32_t)fir_progress * 104 / 1040;
        if (bar_w > 104) bar_w = 104;
        u8g2_DrawBox(&u8g2, 12, 22, bar_w, 4);
    }

    snprintf(buf, sizeof(buf), "Progress: %d/1040", fir_progress);
    u8g2_DrawStr(&u8g2, 10, 40, buf);

    Format_With_Commas(fir_curr_freq, fmt);
    snprintf(buf, sizeof(buf), "Freq: %s Hz", fmt);
    u8g2_DrawStr(&u8g2, 10, 52, buf);

    snprintf(buf, sizeof(buf), "I: %+5d  R: %+5d", fir_curr_i, fir_curr_r);
    u8g2_DrawStr(&u8g2, 10, 63, buf);
}

static void Display_Draw_FIRModeLearnComplete(void) {
    char buf[64];

    u8g2_SetFont(&u8g2, u8g2_font_ncenB08_tr);
    u8g2_DrawStr(&u8g2, 0, 10, "[LEARN DONE]");
    Display_Draw_LiveAnimation(120, 5);
    u8g2_DrawHLine(&u8g2, 0, 14, 128);

    Display_Draw_Cursor(30, (menu_index == 1), 0);
    snprintf(buf, sizeof(buf), "1.View Curve(%s)", fir_type_abbr[fir_filter_type]);
    u8g2_DrawStr(&u8g2, 18, 30, buf);

    Display_Draw_Cursor(45, (menu_index == 2), 0);
    u8g2_DrawStr(&u8g2, 18, 45, "2.Apply Filter");

    Display_Draw_Cursor(60, (menu_index == 3), 0);
    u8g2_DrawStr(&u8g2, 18, 60, "3.Cancel");
}

static void Display_Draw_FIRCurveView(void) {
    char buf[16];

    u8g2_SetFont(&u8g2, u8g2_font_ncenB08_tr);
    u8g2_DrawStr(&u8g2, 0, 10, "[Freq Response]");
    Display_Draw_LiveAnimation(120, 5);
    u8g2_DrawHLine(&u8g2, 0, 14, 128);

    // 预计算 1040 个频点的 dB 值
    float fir_dB[1040];
    float sum_dB = 0.0f;
    for (int i = 0; i < 1040; i++) {
        uint32_t mag2 = (uint32_t)(fir_i_data[i] * fir_i_data[i]) +
                        (uint32_t)(fir_r_data[i] * fir_r_data[i]);
        if (mag2 < 1) mag2 = 1;
        fir_dB[i] = 10.0f * log10f((float)mag2);
        sum_dB += fir_dB[i];
    }

    // 中心化：相对均值
    float mean_dB = sum_dB / 1040.0f;
    float min_dB = 100.0f, max_dB = -100.0f;
    for (int i = 0; i < 1040; i++) {
        fir_dB[i] -= mean_dB;
        if (fir_dB[i] < min_dB) min_dB = fir_dB[i];
        if (fir_dB[i] > max_dB) max_dB = fir_dB[i];
    }

    float range_dB = max_dB - min_dB;
    if (range_dB < 10.0f) {
        float half = range_dB / 2.0f;
        max_dB = half > 5.0f ? half : 5.0f;
        min_dB = -max_dB;
    }

    const int AX_LEFT  = 32;
    const int AX_RIGHT = 124;
    const int AX_TOP   = 18;
    const int AX_BOT   = 53;

    int y_mid = (AX_TOP + AX_BOT) / 2;

    // Y 轴标签（上 / 中 / 下）
    snprintf(buf, sizeof(buf), "%.0f", max_dB);
    u8g2_DrawStr(&u8g2, 0, AX_TOP + 4, buf);
    snprintf(buf, sizeof(buf), "%.0f", (max_dB + min_dB) / 2.0f);
    u8g2_DrawStr(&u8g2, 0, y_mid + 4, buf);
    snprintf(buf, sizeof(buf), "%.0f", min_dB);
    u8g2_DrawStr(&u8g2, 0, AX_BOT + 4, buf);

    // 右上角标注 "dB"
    u8g2_DrawStr(&u8g2, AX_RIGHT - 12, AX_TOP - 2, "dB");

    // 左右竖边框
    u8g2_DrawVLine(&u8g2, AX_LEFT, AX_TOP, AX_BOT - AX_TOP);
    u8g2_DrawVLine(&u8g2, AX_RIGHT, AX_TOP, AX_BOT - AX_TOP + 1);

    // 横网格线：上下虚线，中间实线
    for (int x = AX_LEFT + 1; x < AX_RIGHT; x += 4) {
        u8g2_DrawPixel(&u8g2, x, AX_TOP);
        u8g2_DrawPixel(&u8g2, x, AX_BOT);
    }
    u8g2_DrawHLine(&u8g2, AX_LEFT + 1, y_mid, AX_RIGHT - AX_LEFT - 1);

    // 绘制相对 dB 曲线
    for (int x = AX_LEFT + 1; x < AX_RIGHT; x++) {
        int idx = (x - AX_LEFT - 1) * 1040 / (AX_RIGHT - AX_LEFT - 1);
        float v = fir_dB[idx];
        int py = AX_BOT - (int)((v - min_dB) * (AX_BOT - AX_TOP) / (max_dB - min_dB));
        if (py >= AX_TOP && py <= AX_BOT) u8g2_DrawPixel(&u8g2, x, py);
    }

    // X 轴标签
    u8g2_DrawStr(&u8g2, AX_LEFT, 62, "50Hz");
    u8g2_DrawStr(&u8g2, AX_RIGHT - 30, 62, "60kHz");
}

void Display_Refresh(void) {
    u8g2_ClearBuffer(&u8g2);
    switch (currentState) {
        case STATE_MAIN_MENU:               Display_Draw_MainMenu(); break;
        case STATE_DDS_MODE_MENU:           Display_Draw_DDSMode();  break;
        case STATE_FIR_MODE_MENU:           Display_Draw_FIRMode();  break;
        case STATE_FIR_MODE_LEARNING:       Display_Draw_FIRModeLearningProgress(); break;
        case STATE_FIR_MODE_LEARN_COMPLETE: Display_Draw_FIRModeLearnComplete();    break;
        case STATE_FIR_CURVE_VIEW:          Display_Draw_FIRCurveView();            break;
    }
    u8g2_SendBuffer(&u8g2);
    frame_count++; 
}