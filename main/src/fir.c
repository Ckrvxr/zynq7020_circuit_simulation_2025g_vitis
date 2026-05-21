#include "fir.h"

#include "xil_printf.h"
#include "xil_types.h"

#include "FreeRTOS.h"
#include "task.h"

#include "bram.h"
#include "dds.h"

volatile uint32_t sweep_vpp  = 3300;
volatile uint32_t sweep_freq = 50000;

void FIR_Learn(void) {
    xil_printf("INFO[FIR]: FIR Learning Started...\n\r");

    uint32_t cmd;
    uint32_t ir_result;

    // 设置 ADC 增益
    uint32_t adc_gain = 0x00000800;
    cmd = BRAM_Read(1);
    cmd &= ~0x00000FFF;         // 清空原有的 ADC_GAIN (位 11:0)
    cmd |= (adc_gain & 0x0FFF); // 写入新的 ADC_GAIN 到对应位域
    BRAM_Write(1, cmd);

    // 设置延时参数
    uint32_t delay_cycles = 58;
    cmd = BRAM_Read(3);
    cmd &= ~(0xFFFF << 7);                 // 清空原有的 DELAY_VAL (位 22:7)
    cmd |= ((delay_cycles & 0xFFFF) << 7); // 写入新的延时值到位域 [22:7]
    cmd |= (1 << 5);                       // 拉高 DELAY_TRIG 触发延时参数配置
    BRAM_Write(3, cmd);
    do {
        vTaskDelay(pdMS_TO_TICKS(1));
        cmd = BRAM_Read(3);
    } while ((cmd & (1 << 5)) != 0);
    vTaskDelay(pdMS_TO_TICKS(100));

    // 设置 DDS 增益
    uint32_t dac_gain = DDS_Vpp_to_DACGain((uint32_t)sweep_vpp);
    cmd = BRAM_Read(1);
    cmd &= ~(0x0FFF << 16);             // 清空原有的 DAC_GAIN (位 27:16)
    cmd |= ((dac_gain & 0x0FFF) << 16); // 写入新的 DAC_GAIN 到对应位域
    BRAM_Write(1, cmd);                 // 写入 ADDR_GAIN (地址 1)

    xil_printf("INFO[FIR]: Frequency sweep 100Hz to 50000Hz with 100Hz step...\n\r");
    
    // 扫频：从 100Hz 扫到 50000Hz，步进 100Hz
    for (uint32_t freq = 100; freq <= 50000; freq += 100) {
        uint32_t ftw = DDS_Freq_to_FTW(freq, 50000000);
        cmd = ftw;
        BRAM_Write(0, cmd);

        // 触发装载 DDS 参数到 DDS 模块
        cmd = BRAM_Read(3);
        cmd |= (1 << 0) | (1 << 1);
        BRAM_Write(3, cmd);
        do {
            vTaskDelay(pdMS_TO_TICKS(1)); // 释放 CPU，每次只等 1ms
            cmd = BRAM_Read(3);
        } while ((cmd & ((1 << 0) | (1 << 1))) != 0);
        vTaskDelay(pdMS_TO_TICKS(10));

        // 触发扫频测量
        cmd = BRAM_Read(3);
        cmd |= (1 << 4);
        BRAM_Write(3, cmd);

        // 读取扫频结果
        do {
            vTaskDelay(pdMS_TO_TICKS(1));
            cmd = BRAM_Read(3);
        } while ((cmd & (1 << 4)) != 0);
        vTaskDelay(pdMS_TO_TICKS(20));
        ir_result = BRAM_Read(2);
        xil_printf("INFO[FIR]: Freq= %u Hz, I= %u R= %u\n\r",
                   freq,
                   (ir_result >> 16) & 0xFFFF,
                   ir_result & 0xFFFF);
    }
    
    xil_printf("INFO[FIR]: Frequency sweep completed!\n\r");
}

void FIR_Exec(void) {
}

void FIR_Cancel(void) {
    // TODO: Cancel / interrupt FIR learning
}
