#pragma once

#include "xparameters.h"
#include "xgpiops.h"

#include "FreeRTOS.h"
#include "task.h"


#include "display.h"
#include "dds.h"


typedef enum {
    KEY_EVENT_NONE = 0,
    KEY_EVENT_SINGLE_CLICK,
    KEY_EVENT_LONG_PRESS_HOLD
} Key_Event_Type_t;

typedef enum {
    KEY_ID_UP = 0,
    KEY_ID_DOWN = 1,
    KEY_ID_CONFIRM = 2,
    KEY_ID_CANCEL = 3,
    KEY_COUNT
} Key_ID_t;

typedef struct {
    uint8_t id;
    uint8_t gpio_pin;
    XGpioPs *gpio_instance;
    
    Key_Event_Type_t last_event;
    TickType_t press_start_tick;
    uint8_t is_pressed;
    uint8_t long_press_triggered;
} Key_Handle_t;


#define KEY_DEBOUNCE_MS         50
#define KEY_LONG_PRESS_MS       1000
#define KEY_LONG_PRESS_REPEAT_MS 200

void Key_Init(void);
void Key_Task(void *pvParameters);
void Key_Clear_Event(uint8_t key_id);

void Key_Handler_Up(uint8_t key_id, Key_Event_Type_t event);
void Key_Handler_Down(uint8_t key_id, Key_Event_Type_t event);
void Key_Handler_Confirm(uint8_t key_id, Key_Event_Type_t event);
void Key_Handler_Cancel(uint8_t key_id, Key_Event_Type_t event);
