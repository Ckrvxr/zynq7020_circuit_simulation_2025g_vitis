#pragma once

#include "xil_types.h"

extern volatile uint32_t dds_vpp;
extern volatile uint32_t dds_freq;

void DDS_Vpp_Config(void);
void DDS_Vpp_Plus(void);
void DDS_Vpp_Minus(void);
void DDS_Vpp_Exec(void);
void DDS_Vpp_Cancel(void);


void DDS_Freq_Config(void);
void DDS_Freq_Plus(void);
void DDS_Freq_Minus(void);
void DDS_Freq_Exec(void);
void DDS_Freq_Cancel(void);