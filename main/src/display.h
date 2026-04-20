#pragma once

#include "FreeRTOS.h"
#include "task.h"

#include "xiicps.h"
#include "xil_types.h"

#include <stdio.h>

#include "u8g2.h"

typedef enum {
    STATE_MAIN_MENU,
    STATE_DDS_MODE_MENU,
    STATE_FIR_MODE_MENU,
    STATE_FIR_MODE_LEARNING,
    STATE_FIR_MODE_LEARN_COMPLETE
} DisplayState_t;

extern volatile DisplayState_t currentState;
extern volatile uint8_t menu_index;

void Display_Init(void);
void Display_Refresh(void);
