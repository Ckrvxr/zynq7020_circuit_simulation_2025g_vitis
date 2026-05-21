#pragma once

#include "xiicps.h"
#include "xil_types.h"

#include "FreeRTOS.h"
#include "task.h"

#include <math.h>
#include <stdio.h>

#include "u8g2.h"

#include "dds.h"

typedef enum {
    STATE_MAIN_MENU,
    STATE_DDS_MODE_MENU,
    STATE_FIR_MODE_MENU,
    STATE_FIR_MODE_LEARNING,
    STATE_FIR_MODE_LEARN_COMPLETE,
    STATE_FIR_CURVE_VIEW
} DisplayState_t;

extern volatile DisplayState_t currentState;
extern volatile uint8_t menu_index;
extern volatile uint8_t slect_index;

void Display_Init(void);
void Display_Refresh(void);
