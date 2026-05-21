#pragma once

#include "xil_printf.h"
#include "xil_types.h"

#include "FreeRTOS.h"
#include "task.h"

#include "bram.h"
#include "dds.h"

void FIR_Learn(void);
void FIR_Exec(void);
void FIR_Cancel(void);
