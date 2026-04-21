#include "display.h"

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
    vTaskDelay(pdMS_TO_TICKS(1000));
    // 2. 初始化 u8g2，如果是 SSD1306 屏幕可以直接把 ssd1315 换成 ssd1306
    u8g2_Setup_ssd1315_i2c_128x64_noname_f(&u8g2, U8G2_R0, u8x8_byte_zynq_hw_i2c, u8x8_gpio_and_delay_zynq);
    u8g2_InitDisplay(&u8g2);
    u8g2_SetPowerSave(&u8g2, 0); // 唤醒屏幕
}

/* -------------------------------------------------------- Scene -------------------------------------------------------- */
volatile DisplayState_t currentState = STATE_MAIN_MENU;
volatile uint8_t menu_index = 1;
volatile uint8_t slect_index = 0;

static void Display_Draw_LiveAnimation(int x, int y) {
    static uint8_t frame_idx = 0;
    u8g2_SetDrawColor(&u8g2, 1);
    switch ( frame_idx % 4 ) {
        case 0: u8g2_DrawLine(&u8g2, x-3, y, x+3, y);     break;
        case 1: u8g2_DrawLine(&u8g2, x-2, y-2, x+2, y+2); break;
        case 2: u8g2_DrawLine(&u8g2, x, y-3, x, y+3);     break;
        case 3: u8g2_DrawLine(&u8g2, x+2, y-2, x-2, y+2); break;
    }
    frame_idx++;
}
static void Display_Draw_MainMenu(void) {
    u8g2_SetFont(&u8g2, u8g2_font_ncenB08_tr);

    u8g2_DrawStr(&u8g2, 0, 10, "[Main Menu]");
    Display_Draw_LiveAnimation(120, 5);
    u8g2_DrawHLine(&u8g2, 0, 14, 128);

    // 简单的选中逻辑：在选中的行前面画个 ">"
    if (menu_index == 1) u8g2_DrawStr(&u8g2, 5, 30, ">");
    u8g2_DrawStr(&u8g2, 15, 30, "DDS Mode");
    if (menu_index == 2) u8g2_DrawStr(&u8g2, 5, 45, ">");
    u8g2_DrawStr(&u8g2, 15, 45, "FIR Mode");
}
static void Display_Draw_DDSMode(void) {
    extern volatile int32_t dds_vpp;
    extern volatile int32_t dds_freq;

    static const char* wave_type = "Sine";

    char buf[32];

    u8g2_SetFont(&u8g2, u8g2_font_ncenB08_tr);

    u8g2_DrawStr(&u8g2, 0, 10, "[DDS Mode]");
    Display_Draw_LiveAnimation(120, 5);
    u8g2_DrawHLine(&u8g2, 0, 14, 128);

    u8g2_DrawStr(&u8g2, 5, 28, "Wave:");
    u8g2_DrawStr(&u8g2, 60, 28, wave_type);

    u8g2_DrawStr(&u8g2, 5, 43, "Vpp:");
    // 格式化浮点数，Zynq 的标准库通常支持 %f
    // 如果不支持，可以转成整数部分和小数部分显示
    snprintf(buf, sizeof(buf), "%lu mV", dds_vpp);
    u8g2_DrawStr(&u8g2, 60, 43, buf);

    u8g2_DrawStr(&u8g2, 5, 58, "Freq:");
    snprintf(buf, sizeof(buf), "%lu Hz", dds_freq);
    u8g2_DrawStr(&u8g2, 60, 58, buf);
}
static void Display_Draw_FIRMode(void) {
    // 模拟当前选择的参数索引和状态
    static const char* filter_type = "Band-Stop";

    u8g2_SetFont(&u8g2, u8g2_font_ncenB08_tr);

    u8g2_DrawStr(&u8g2, 0, 10, "[FIR Mode]");
    Display_Draw_LiveAnimation(120, 5);
    u8g2_DrawHLine(&u8g2, 0, 14, 128);


    u8g2_DrawStr(&u8g2, 5, 32, "Active:");
    u8g2_DrawStr(&u8g2, 50, 32, filter_type);

    u8g2_DrawStr(&u8g2, 5, 56, "> Start Learning");
}
static void Display_Draw_FIRModeLearningProgress(void) {

}
static void Display_Draw_FIRModeLearnComplete(void) {

}

void Display_Refresh(void) {
    // Test Display
    // static uint32_t count = 0;
    // static char buf[32];
    // u8g2_ClearBuffer(&u8g2);
    // u8g2_SetFont(&u8g2, u8g2_font_ncenB08_tr);
    // u8g2_DrawStr(&u8g2, 0, 15, "u8g2 on Zynq!");
    // snprintf(buf, sizeof(buf), "Count: ", count++);
    // u8g2_DrawStr(&u8g2, 0, 35, buf);
    // u8g2_SendBuffer(&u8g2);

    u8g2_ClearBuffer(&u8g2);
    switch (currentState) {
        case STATE_MAIN_MENU:
            Display_Draw_MainMenu();
            break;
        case STATE_DDS_MODE_MENU:
            Display_Draw_DDSMode();
            break;
        case STATE_FIR_MODE_MENU:
            Display_Draw_FIRMode();
            break;
        case STATE_FIR_MODE_LEARNING:
            Display_Draw_FIRModeLearningProgress();
            break;
        case STATE_FIR_MODE_LEARN_COMPLETE:
            Display_Draw_FIRModeLearnComplete();
            break;
    }
    u8g2_SendBuffer(&u8g2);
}
