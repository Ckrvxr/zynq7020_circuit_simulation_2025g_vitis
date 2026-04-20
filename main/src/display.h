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

void Display_Init(void);
void Display_Refresh(void);

void Display_Draw_MainMenu(void) ;
void Display_Draw_DDSMode(void);
void Display_Draw_FIRMode(void);
void Display_Draw_FIRModeLearningProgress(void);
void Display_Draw_FIRModeLearnComplete(void);