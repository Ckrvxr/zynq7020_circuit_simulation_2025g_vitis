#pragma once

#include "xparameters.h"
#include "xgpiops.h"

#include "FreeRTOS.h"
#include "task.h"

#include "multi_button.h"

void Key_Init(void);
void Key_Ticks(void);
uint8_t Key_Read_Callback(uint8_t button_id);

void Key_Event_Up_SingleClcick_Handler(Button* btn, void* user_data);
void Key_Event_Down_SingleClcick_Handler(Button* btn, void* user_data);
void Key_Event_Confirm_SingleClcick_Handler(Button* btn, void* user_data);
void Key_Event_Cancel_SingleClcick_Handler(Button* btn, void* user_data);