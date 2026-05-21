#pragma once

#include "xil_types.h"

#include "bram.h"

extern volatile uint32_t dds_vpp;
extern volatile uint32_t dds_freq;

void DDS_Vpp_Config(void);
void DDS_Vpp_PlusorMinus(int32_t delta);
void DDS_Vpp_Cancel(void);
void DDS_Vpp_Exec(void);

void DDS_Freq_Config(void);
void DDS_Freq_PlusorMinus(int32_t delta);
void DDS_Freq_Cancel(void);
void DDS_Freq_Exec(void);

uint32_t DDS_Freq_to_FTW(uint32_t f_out, uint32_t f_clk);
uint32_t DDS_Vpp_to_DACGain(uint32_t vpp);
void DDS_Send_Command(uint32_t vpp, uint32_t freq);