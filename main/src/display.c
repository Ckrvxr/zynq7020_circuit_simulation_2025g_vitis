#include "FreeRTOS.h"
#include "task.h"
#include "display.h"
#include "xiicps.h"

u8g2_t u8g2;
static XIicPs IicInstance;

uint8_t u8x8_byte_zynq_i2c(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr) {
    static uint8_t buffer[1024 + 128];
    static uint8_t buf_idx;

    switch(msg) {
        case U8X8_MSG_BYTE_SEND:
            memcpy(&buffer[buf_idx], arg_ptr, arg_int);
            buf_idx += arg_int;
            break;
        case U8X8_MSG_BYTE_START_TRANSFER:
            buf_idx = 0;
            break;
        case U8X8_MSG_BYTE_END_TRANSFER:
            // 调用 Xilinx 库函数发送数据
            // 0x3C 是常见的 OLED 地址，需根据实际确认
            XIicPs_MasterSendPolled(&IicInstance, buffer, buf_idx, 0x3C);
            break;  
        case U8X8_MSG_BYTE_INIT:
            // 这里可以放 I2C 控制器的低级初始化代码（或者放在 Display_Init）
            break;
    }
    return 1;
}

uint8_t u8x8_gpio_and_delay_zynq(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr) {
    switch(msg) {
        case U8X8_MSG_DELAY_MILLI:
            vTaskDelay(pdMS_TO_TICKS(arg_int));
            break;
        case U8X8_MSG_GPIO_RESET:
            // 如果你有 Reset 引脚连接到 EMIO/MIO，在这里操作电平
            // XGpioPs_WritePin(...);
            break;
    }
    return 1;
}

void Display_Init(void) {
    // 1. 初始化 Zynq I2C 硬件
    XIicPs_Config *Config = XIicPs_LookupConfig(XPAR_PS7_I2C_0_DEVICE_ID);
    XIicPs_CfgInitialize(&IicInstance, Config, Config->BaseAddress);
    XIicPs_SetSClk(&IicInstance, 400000);

    // 2. 绑定 U8g2 驱动
    u8g2_Setup_ssd1315_i2c_128x64_noname_f(&u8g2, U8G2_R0, u8x8_byte_zynq_i2c, u8x8_gpio_and_delay_zynq);

    // 3. 唤醒屏幕
    u8g2_InitDisplay(&u8g2);
    u8g2_SetPowerSave(&u8g2, 0);
    u8g2_ClearBuffer(&u8g2);
}
