#pragma once

#include "xbram.h"
#include "xil_printf.h"
#include "xil_types.h"
#include "xparameters.h"

void BRAM_Init(void);

void BRAM_Write(uint32_t byte_offset, uint32_t data);
uint32_t BRAM_Read(uint32_t byte_offset);