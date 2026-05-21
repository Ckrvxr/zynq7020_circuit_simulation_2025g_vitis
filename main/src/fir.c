#include "fir.h"

#include "xil_printf.h"
#include "xil_types.h"

#include "FreeRTOS.h"
#include "task.h"

#include "bram.h"
#include "dds.h"

volatile uint32_t sweep_vpp  = 3300;
volatile uint32_t sweep_freq = 50000;

volatile int fir_progress  = 0;
volatile uint32_t fir_curr_freq = 0;
volatile int16_t fir_curr_i = 0;
volatile int16_t fir_curr_r = 0;

volatile int16_t fir_i_data[1040];
volatile int16_t fir_r_data[1040];

volatile uint8_t fir_filter_type = FIR_TYPE_UNKNOWN;
const char *fir_type_str[] = {
    [FIR_TYPE_UNKNOWN]   = "Unknown",
    [FIR_TYPE_LOW_PASS]  = "Low-Pass",
    [FIR_TYPE_HIGH_PASS] = "High-Pass",
    [FIR_TYPE_BAND_PASS] = "Band-Pass",
    [FIR_TYPE_BAND_STOP] = "Band-Stop",
};

const char *fir_type_abbr[] = {
    [FIR_TYPE_UNKNOWN]   = "UK",
    [FIR_TYPE_LOW_PASS]  = "LP",
    [FIR_TYPE_HIGH_PASS] = "HP",
    [FIR_TYPE_BAND_PASS] = "BP",
    [FIR_TYPE_BAND_STOP] = "BS",
};

static uint8_t FIR_AnalyzeResponse(void) {
    uint32_t max_mag2 = 0;
    for (int i = 0; i < 1040; i++) {
        uint32_t m = (uint32_t)(fir_i_data[i] * fir_i_data[i]) +
                     (uint32_t)(fir_r_data[i] * fir_r_data[i]);
        if (m > max_mag2) max_mag2 = m;
    }
    if (max_mag2 == 0) return FIR_TYPE_UNKNOWN;

    uint32_t threshold = max_mag2 * 45 / 100;
    int low_cnt = 0, mid_cnt = 0, high_cnt = 0;

    for (int i = 0; i < 1040; i++) {
        uint32_t m = (uint32_t)(fir_i_data[i] * fir_i_data[i]) +
                     (uint32_t)(fir_r_data[i] * fir_r_data[i]);
        uint8_t above = (m >= threshold) ? 1 : 0;
        if      (i < 347) low_cnt  += above;
        else if (i < 694) mid_cnt  += above;
        else              high_cnt += above;
    }

    int low_pct  = low_cnt  * 100 / 347;
    int mid_pct  = mid_cnt  * 100 / 347;
    int high_pct = high_cnt * 100 / 346;

    if (low_pct > 60 && high_pct < 40) return FIR_TYPE_LOW_PASS;
    if (low_pct < 40 && high_pct > 60) return FIR_TYPE_HIGH_PASS;
    if (low_pct > 60 && high_pct > 60 && mid_pct < 40) return FIR_TYPE_BAND_STOP;
    if (low_pct < 40 && high_pct < 40 && mid_pct > 60) return FIR_TYPE_BAND_PASS;
    return FIR_TYPE_UNKNOWN;
}

void FIR_Learn(void) {
    xil_printf("INFO[FIR]: FIR Learning Started...\n\r");

    uint32_t cmd;
    uint32_t ir_result;

    // 设置 ADC 增益
    uint32_t adc_gain = 0x00000800;
    cmd = BRAM_Read(1);         // 读取当前增益寄存器值
    cmd &= ~0x00000FFF;         // 清空原有的 ADC_GAIN (位 11:0)
    cmd |= (adc_gain & 0x0FFF); // 写入新的 ADC_GAIN 到对应位域
    BRAM_Write(1, cmd);         // 回写增益寄存器

    // 设置延时参数
    // uint32_t delay_cycles = 58;
    // cmd = BRAM_Read(3);
    // cmd &= ~(0xFFFF << 7);                 // 清空原有的 DELAY_VAL (位 22:7)
    // cmd |= ((delay_cycles & 0xFFFF) << 7); // 写入新的延时值到位域 [22:7]
    // cmd |= (1 << 5);                       // 拉高 DELAY_TRIG 触发延时参数配置
    // BRAM_Write(3, cmd);
    // do {
    //     vTaskDelay(pdMS_TO_TICKS(1));
    //     cmd = BRAM_Read(3);
    // } while ((cmd & (1 << 5)) != 0);
    // vTaskDelay(pdMS_TO_TICKS(100));

    // 设置 DDS 增益
    uint32_t dac_gain = DDS_Vpp_to_DACGain((uint32_t)sweep_vpp);
    cmd = BRAM_Read(1);                 // 读取当前增益寄存器值
    cmd &= ~(0x0FFF << 16);             // 清空原有的 DAC_GAIN (位 27:16)
    cmd |= ((dac_gain & 0x0FFF) << 16); // 写入新的 DAC_GAIN 到对应位域
    BRAM_Write(1, cmd);                 // 回写增益寄存器

    xil_printf("INFO[FIR]: Frequency sweep started...\n\r");
    
    // 扫频：50Hz ~ 60kHz，1040 个线性间隔频点
    for (int i = 0; i < 1040; i++) {
        uint32_t freq = 50 + (uint32_t)((uint64_t)i * 59950 / 1039);
        fir_progress = i;
        fir_curr_freq = freq;

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
        vTaskDelay(pdMS_TO_TICKS(10));
        ir_result = BRAM_Read(2);
        fir_curr_i = (int16_t)((ir_result >> 16) & 0xFFFF);
        fir_curr_r = (int16_t)(ir_result & 0xFFFF);
        fir_i_data[i] = fir_curr_i;
        fir_r_data[i] = fir_curr_r;
        // xil_printf("INFO[FIR]: Freq= %u Hz, I= %d R= %d\n\r",
        //            freq, fir_curr_i, fir_curr_r);
    }

    fir_progress = 1040;
    fir_filter_type = FIR_AnalyzeResponse();

    xil_printf("INFO[FIR]: Frequency sweep completed! Type: %s\n\r",
               fir_type_str[fir_filter_type]);
}

void FIR_Exec(void) {
}

void FIR_Cancel(void) {
    // TODO: Cancel / interrupt FIR learning
}
