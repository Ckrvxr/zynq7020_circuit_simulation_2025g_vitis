#pragma once

#include "FreeRTOS.h"
#include "task.h"

#include "xparameters.h"
#include "xgpiops.h"

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
    TickType_t last_trigger_tick;
    uint8_t is_pressed;
    uint8_t long_press_triggered;
} Key_Handle_t;

#define KEY_DEBOUNCE_MS          50
#define KEY_LONG_PRESS_MS        800
#define KEY_LONG_PRESS_REPEAT_MS 200 

// 函数声明保持不变...
void Key_Init(void);
void Key_Task(void *pvParameters);