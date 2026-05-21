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

typedef enum {
    FIR_TYPE_OTHER     = 0,
    FIR_TYPE_LOW_PASS  = 1,
    FIR_TYPE_HIGH_PASS = 2,
    FIR_TYPE_BAND_PASS = 3,
    FIR_TYPE_BAND_STOP = 4,
    FIR_TYPE_ALL_PASS  = 5,
    FIR_TYPE_ALL_STOP  = 6,
} FIR_CircuitType_t;

extern volatile FIR_CircuitType_t fir_circuit_type;
extern const char *fir_type_str[];
extern const char *fir_type_abbr[];

enum {
    FIR_TAPS = 2080,
    FFT_N    = 2048
};

extern int16_t fir_coeffs[FIR_TAPS];
extern volatile uint8_t fir_coeffs_ready;
extern volatile uint8_t fir_learned;

void FIR_Calibrate(void);
void FIR_Learn(void);
void FIR_Run(void);

