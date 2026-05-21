#pragma once

#include "xil_printf.h"
#include "xil_types.h"

#include "FreeRTOS.h"
#include "task.h"

#include "bram.h"
#include "dds.h"

extern volatile int fir_progress;
extern volatile uint32_t fir_curr_freq;
extern volatile int16_t fir_curr_i;
extern volatile int16_t fir_curr_r;

void FIR_Learn(void);
void FIR_Exec(void);
void FIR_Cancel(void);
