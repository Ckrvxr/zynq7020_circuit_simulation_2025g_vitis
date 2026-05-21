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

extern volatile int16_t fir_i_data[1040];
extern volatile int16_t fir_r_data[1040];

extern volatile uint32_t fir_ref_mag2[1040];
extern volatile uint8_t  fir_calibrated;
extern volatile uint8_t  fir_is_calibrating;

#define FIR_TYPE_UNKNOWN    0
#define FIR_TYPE_LOW_PASS   1
#define FIR_TYPE_HIGH_PASS  2
#define FIR_TYPE_BAND_PASS  3
#define FIR_TYPE_BAND_STOP  4

extern volatile uint8_t fir_filter_type;
extern const char *fir_type_str[];
extern const char *fir_type_abbr[];

void FIR_Calibrate(void);
void FIR_Learn(void);
void FIR_Exec(void);
void FIR_Cancel(void);
