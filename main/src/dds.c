#include "dds.h"

#include "xil_types.h"

#include "bram.h"

volatile uint32_t dds_vpp  = 1000;  // 0 ~ 8,000 -> 0 ~ 8 V
volatile uint32_t dds_freq = 50000;  // 0 ~ 1,000,000 -> 0 ~ 1 Mhz
static uint32_t dds_vpp_bak;
static uint32_t dds_freq_bak;

void DDS_Vpp_Config(void) {
    dds_vpp_bak = dds_vpp;
}
void DDS_Vpp_PlusorMinus(int32_t delta) {
    if (delta > 0) {
        dds_vpp += (uint32_t)delta;
        if (dds_vpp > 8000) dds_vpp = 8000;
    } else if (delta < 0) {
        uint32_t abs_delta = (uint32_t)(-delta);
        if (dds_vpp < abs_delta) dds_vpp = 0;
        else dds_vpp -= abs_delta;
    }
}
void DDS_Vpp_Cancel(void) { 
    dds_vpp = dds_vpp_bak; 
}
void DDS_Vpp_Exec(void) {
    DDS_Send_Command();
}

void DDS_Freq_Config(void) {
    dds_freq_bak = dds_freq;
}
void DDS_Freq_PlusorMinus(int32_t delta) {
    if (delta > 0) {
        dds_freq += (uint32_t)delta;
        if (dds_freq > 1000000) dds_freq = 1000000;
    } else if (delta < 0) {
        uint32_t abs_delta = (uint32_t)(-delta);
        if (dds_freq < abs_delta) dds_freq = 0;
        else dds_freq -= abs_delta;
    }
}
void DDS_Freq_Cancel(void) {
     dds_freq = dds_freq_bak; 
}
void DDS_Freq_Exec(void) {
    DDS_Send_Command(); 
}

/**
 * @brief 计算 32 位 DDS 的相位递增字 (FTW)
 *
 * @param f_out 目标输出频率 (Hz)
 * @param f_clk 系统主时钟频率 (Hz)
 * @return uint32_t 返回 32 位的相位递增字
 */
uint32_t DDS_Freq_to_FTW(uint32_t f_out, uint32_t f_clk) {
  uint64_t pow_2_32 = 1ULL << 32;
  // 防溢出处理：先将 f_out 强转为 64 位，再做乘法
  uint64_t temp = ((uint64_t)f_out * pow_2_32) + (f_clk / 2);
  uint32_t ftw = (uint32_t)(temp / f_clk);
  return ftw;
}

uint32_t DDS_Vpp_to_DACGain(uint32_t vpp) {
    // 将 0 ~ 8,000 mV 映射到 0 ~ 255 (12-bit DAC,实际只有 8-bit 有效)
    uint32_t dac_value = (vpp * 255 + 4000) / 8000;
    // if (dac_value > 4) dac_value -= 1;
    if (dac_value > 255) dac_value = 255;
    return dac_value;
}

void DDS_Send_Command() {
    uint32_t cmd = 0;

    // Set DDS Frequency
    uint32_t ftw = DDS_Freq_to_FTW((uint32_t)dds_freq, 50000000);
    cmd = ftw;
    BRAM_Write(0, cmd);
    // Set DDS Vpp
    uint32_t dac_gain = DDS_Vpp_to_DACGain((uint32_t)dds_vpp);
    cmd = BRAM_Read(1) ;
    cmd &= ~(0x0FFF << 16);             // 清空原有的 DAC_GAIN (位 27:16)
    cmd |= ((dac_gain & 0x0FFF) << 16); // 写入新的 DAC_GAIN 到对应位域
    BRAM_Write(1, cmd);                 // 写入 ADDR_GAIN (地址 1)
    // DDS Update Trigger
    cmd = BRAM_Read(3);                 // 读取当前状态寄存器 (地址 3)
    cmd |= (1 << 0) | (1 << 1);         // 同时将 Bit 0 和 Bit 1 置 1 触发更新
    BRAM_Write(3, cmd);
}