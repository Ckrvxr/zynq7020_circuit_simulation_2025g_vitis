#include "dds.h"

volatile int32_t dds_vpp  = 3300; // 0 ~ 5,000 -> 0 ~ 5 V
volatile int32_t dds_freq = 100;  // 0 ~ 1,000,000 -> 0 ~ 1 Mhz
static int32_t dds_vpp_bak;     // 0 ~ 5,000 -> 0 ~ 5 V
static int32_t dds_freq_bak;    // 0 ~ 1,000,000 -> 0 ~ 1 Mhz


void DDS_Vpp_Config(void) {
    dds_vpp_bak = dds_vpp;
}
void DDS_Vpp_Plus(void) {
    dds_vpp += 100;
    if (dds_vpp > 5000) dds_vpp = 5000;
}
void DDS_Vpp_Minus(void) {
    dds_vpp -= 100;
    if (dds_vpp < 0) dds_vpp = 0;
}
void DDS_Vpp_Exec(void) {
    //
}
void DDS_Vpp_Cancel(void) {
    dds_vpp = dds_vpp_bak;
}

void DDS_Freq_Config(void) {
    dds_freq_bak = dds_freq;
}
void DDS_Freq_Plus(void) {
    dds_freq += 100;
    if (dds_freq > 1000000) dds_freq = 1000000;
}
void DDS_Freq_Minus(void) {
    dds_freq -= 100;
    if (dds_freq < 0) dds_freq = 0;
}
void DDS_Freq_Exec(void){
    // 
}
void DDS_Freq_Cancel(void){
    dds_freq = dds_freq_bak;
}
