#include "fir.h"

volatile uint32_t sweep_vpp = 3300;
volatile uint32_t sweep_freq = 1000;

void FIR_Learn(void) {
    xil_printf("INFO[FIR]: FIR Learning Started...\n\r");

    uint32_t cmd;

    uint32_t dac_gain = Vpp_to_DACGain((uint32_t)sweep_vpp);
    cmd = BRAM_Read(1);
    cmd &= ~(0x0FFF << 16);             // 清空原有的 DAC_GAIN (位 27:16)
    cmd |= ((dac_gain & 0x0FFF) << 16); // 写入新的 DAC_GAIN 到对应位域
    BRAM_Write(1, cmd);                 // 写入 ADDR_GAIN (地址 1)

    uint32_t ftw = Freq_to_FTW((uint32_t)sweep_freq, 50000000);
    cmd = ftw;
    BRAM_Write(0, cmd);


}

void FIR_Exec(void) {

}

void FIR_Cancel(void) {
    // TODO: Cancel / interrupt FIR learning
}
