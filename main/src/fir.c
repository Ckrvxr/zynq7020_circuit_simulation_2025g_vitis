#include "fir.h"

#include "xil_printf.h"
#include "xil_types.h"

#include "FreeRTOS.h"
#include "task.h"

#include "bram.h"
#include "dds.h"
#include "kiss_fft.h"

volatile uint32_t sweep_vpp  = 2000;
volatile uint32_t sweep_freq = 50000;

volatile int fir_progress  = 0;
volatile uint32_t fir_curr_freq = 0;
volatile int16_t fir_curr_i = 0;
volatile int16_t fir_curr_r = 0;

volatile int16_t fir_i_data[1040];
volatile int16_t fir_r_data[1040];

volatile uint32_t fir_ref_mag2[1040];
volatile uint8_t  fir_calibrated = 0;
volatile uint8_t  fir_is_calibrating = 0;

int16_t fir_coeffs[FIR_TAPS];
volatile uint8_t fir_coeffs_ready = 0;
volatile uint8_t fir_learned = 0;

volatile FIR_CircuitType_t fir_circuit_type = FIR_TYPE_OTHER;
const char *fir_type_str[] = {
    [FIR_TYPE_OTHER]   = "Other",
    [FIR_TYPE_LOW_PASS]  = "Low-Pass",
    [FIR_TYPE_HIGH_PASS] = "High-Pass",
    [FIR_TYPE_BAND_PASS] = "Band-Pass",
    [FIR_TYPE_BAND_STOP] = "Band-Stop",
    [FIR_TYPE_ALL_PASS]  = "All-Pass",
    [FIR_TYPE_ALL_STOP]  = "All-Stop",
};

const char *fir_type_abbr[] = {
    [FIR_TYPE_OTHER]   = "OT",
    [FIR_TYPE_LOW_PASS]  = "LP",
    [FIR_TYPE_HIGH_PASS] = "HP",
    [FIR_TYPE_BAND_PASS] = "BP",
    [FIR_TYPE_BAND_STOP] = "BS",
    [FIR_TYPE_ALL_PASS]  = "AP",
    [FIR_TYPE_ALL_STOP]  = "AS",
};

static uint8_t FIR_AnalyzeResponse(void) {
    uint32_t max_mag2 = 0;
    for (int i = 0; i < 1040; i++) {
        uint32_t m = (uint32_t)(fir_i_data[i] * fir_i_data[i]) +
                     (uint32_t)(fir_r_data[i] * fir_r_data[i]);
        if (m > max_mag2) max_mag2 = m;
    }
    if (max_mag2 == 0) return FIR_TYPE_OTHER;

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
    if (low_pct > 60 && mid_pct > 60 && high_pct > 60) return FIR_TYPE_ALL_PASS;
    if (low_pct < 40 && mid_pct < 40 && high_pct < 40) return FIR_TYPE_ALL_STOP;
    return FIR_TYPE_OTHER;
}

static void FIR_Sweep(void) {
    uint32_t cmd;
    uint32_t ir_result;

    // 设置 ADC 增益
    uint32_t adc_gain = 0x00000800;
    cmd = BRAM_Read(1);         // 读取当前增益寄存器值
    cmd &= ~0x00000FFF;         // 清空原有的 ADC_GAIN (位 11:0)
    cmd |= (adc_gain & 0x0FFF); // 写入新的 ADC_GAIN 到对应位域
    BRAM_Write(1, cmd);         // 回写增益寄存器

    // 设置 DDS 增益
    uint32_t dac_gain = DDS_Vpp_to_DACGain((uint32_t)sweep_vpp);
    cmd = BRAM_Read(1);                 // 读取当前增益寄存器值
    cmd &= ~(0x0FFF << 16);             // 清空原有的 DAC_GAIN (位 27:16)
    cmd |= ((dac_gain & 0x0FFF) << 16); // 写入新的 DAC_GAIN 到对应位域
    BRAM_Write(1, cmd);                 // 回写增益寄存器

    // 扫频：50Hz ~ 500kHz，1040 个线性间隔频点
    for (int i = 0; i < 1040; i++) {
        uint32_t freq = 50 + (uint32_t)((uint64_t)i * 499950 / 1039);
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
            vTaskDelay(pdMS_TO_TICKS(1));
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

        uint32_t mag2 = (uint32_t)(fir_curr_i * fir_curr_i) +
                        (uint32_t)(fir_curr_r * fir_curr_r);

        if (fir_is_calibrating) {
            fir_ref_mag2[i] = mag2;
        } else {
            fir_i_data[i] = fir_curr_i;
            fir_r_data[i] = fir_curr_r;
        }
    }

    fir_progress = 1040;
}

void FIR_CalcCoeffs(void) {
    static kiss_fft_cpx freq[FFT_N];
    static kiss_fft_cpx time[FFT_N];

    for (int k = 0; k < 1040; k++) {
        freq[k].r = (float)fir_i_data[k];
        freq[k].i = (float)fir_r_data[k];
    }
    for (int k = 1040; k < FFT_N - 1039; k++) {
        freq[k].r = 0; freq[k].i = 0;
    }
    for (int k = 1; k < 1040; k++) {
        freq[FFT_N - k].r =  freq[k].r;
        freq[FFT_N - k].i = -freq[k].i;
    }

    kiss_fft_cfg cfg = kiss_fft_alloc(FFT_N, 1, NULL, NULL);
    kiss_fft(cfg, freq, time);
    kiss_fft_free(cfg);

    float scale = 1.0f / FFT_N;
    for (int n = 0; n < FIR_TAPS; n++) {
        float win = 0.54f - 0.46f * cosf(2.0f * (float)M_PI * n / (FIR_TAPS - 1));
        float v = time[n].r * scale * win;
        if (v >  1.0f) v =  1.0f;
        if (v < -1.0f) v = -1.0f;
        fir_coeffs[n] = (int16_t)(v * 32767.0f);
    }

    fir_coeffs_ready = 1;
}

void FIR_Calibrate(void) {
    xil_printf("INFO[FIR]: Calibration started...\n\r");
    fir_learned = 0;
    fir_is_calibrating = 1;
    FIR_Sweep();
    fir_calibrated = 1;
    fir_is_calibrating = 0;
    xil_printf("INFO[FIR]: Calibration completed!\n\r");
}

void FIR_Learn(void) {
    xil_printf("INFO[FIR]: FIR Learning Started...\n\r");
    fir_is_calibrating = 0;
    FIR_Sweep();
    fir_circuit_type = FIR_AnalyzeResponse();
    fir_learned = 1;
    xil_printf("INFO[FIR]: Frequency sweep completed! Type: %s\n\r",
               fir_type_str[fir_circuit_type]);
}

void FIR_Run(void) {
    if (fir_learned) {
        xil_printf("INFO[FIR]: Calculating coefficients from learned data...\n\r");
        FIR_CalcCoeffs();

        for (int i = 0; i < 1040; i++) {
            uint32_t word = ((uint16_t)fir_coeffs[i * 2 + 1] << 16) |
                             (uint16_t)fir_coeffs[i * 2];
            BRAM_Write(4 + i, word);
        }
    } else {
        xil_printf("INFO[FIR]: No learned data, using PL default coefficients.\n\r");
    }

    uint32_t cmd = BRAM_Read(3);
    cmd |= (1 << 3);
    BRAM_Write(3, cmd);
}

void FIR_Cancel(void) {
    // xil_printf("INFO[FIR]: FIR operation cancelled. Returning to menu...\n\r");
    // // 停止当前 DDS 输出
    // DDS_Send_Command(0, 0);
    // // 切换通道回主输出通道
    // uint32_t cmd = BRAM_Read(3);
    // cmd &= ~(1 << 3); // 清除 Bit 3 切换回主输出通道
    // BRAM_Write(3, cmd);
}
