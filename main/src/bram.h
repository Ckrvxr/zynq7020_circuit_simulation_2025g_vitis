#pragma once

#include "xil_printf.h"
#include "xil_types.h"
#include "xparameters.h"
#include "xbram.h"


#define BRAM_WORD_SIZE  4   // 32-bit = 4 bytes

void BRAM_Init(void);
int  BRAM_SelfTest(void);

void     BRAM_Write(uint32_t byte_offset, uint32_t data);
uint32_t BRAM_Read(uint32_t byte_offset);