#include "key.h"

#include "xgpiops.h"
#include "xparameters.h"

#include "FreeRTOS.h"
#include "task.h"

#include "dds.h"
#include "display.h"
#include "fir.h"

static Key_Handle_t keys[KEY_COUNT];

static XGpioPs Gpio;

extern volatile DisplayState_t currentState;
extern volatile uint8_t menu_index;
extern volatile uint8_t slect_index;

#define ACCEL_LEVEL_1_MS 1400
#define ACCEL_LEVEL_2_MS 2400
#define ACCEL_LEVEL_3_MS 3400
#define ACCEL_LEVEL_4_MS 4400

static void Key_Handler_Up(uint8_t key_id, Key_Event_Type_t event) {
    uint32_t step_vpp = 1, step_freq = 1;
    if (event == KEY_EVENT_LONG_PRESS_HOLD) {
        TickType_t duration = xTaskGetTickCount() - keys[key_id].press_start_tick;
        uint32_t duration_ms = (uint32_t)(duration * 1000 / configTICK_RATE_HZ);
        if      (duration_ms >= ACCEL_LEVEL_4_MS) { step_freq = 10000; step_vpp = 1000; }
        else if (duration_ms >= ACCEL_LEVEL_3_MS) { step_vpp = 1000;  step_freq = 1000; }
        else if (duration_ms >= ACCEL_LEVEL_2_MS) { step_vpp = 100;   step_freq = 100;  }
        else if (duration_ms >= ACCEL_LEVEL_1_MS) { step_vpp = 10;    step_freq = 10;   }
    }
    if(event == KEY_EVENT_SINGLE_CLICK) {
        if(currentState == STATE_MAIN_MENU) {
            if(menu_index > 1) { menu_index--; }
        }
        else if(currentState == STATE_DDS_MODE_MENU) {
            if(slect_index == 0) {
                if(menu_index > 1) { menu_index--; }
            }
            else if(slect_index == 1) {
                DDS_Vpp_PlusorMinus((int32_t)step_vpp);
            }
            else if(slect_index == 2) {
                DDS_Freq_PlusorMinus((int32_t)step_freq);
            }
        }
    }
    else if(event == KEY_EVENT_LONG_PRESS_HOLD) {
        if(currentState == STATE_DDS_MODE_MENU) {
            if(slect_index == 1) { DDS_Vpp_PlusorMinus((int32_t)step_vpp); }
            else if(slect_index == 2) { DDS_Freq_PlusorMinus((int32_t)step_freq); }
        }
    }
}

static void Key_Handler_Down(uint8_t key_id, Key_Event_Type_t event) {
    uint32_t step_vpp = 1, step_freq = 1;
    if (event == KEY_EVENT_LONG_PRESS_HOLD) {
        TickType_t duration = xTaskGetTickCount() - keys[key_id].press_start_tick;
        uint32_t duration_ms = (uint32_t)(duration * 1000 / configTICK_RATE_HZ);
        if      (duration_ms >= ACCEL_LEVEL_4_MS) { step_freq = 10000; step_vpp = 1000; }
        else if (duration_ms >= ACCEL_LEVEL_3_MS) { step_vpp = 1000;  step_freq = 1000; }
        else if (duration_ms >= ACCEL_LEVEL_2_MS) { step_vpp = 100;   step_freq = 100;  }
        else if (duration_ms >= ACCEL_LEVEL_1_MS) { step_vpp = 10;    step_freq = 10;   }
    }
    if(event == KEY_EVENT_SINGLE_CLICK) {
        if(currentState == STATE_MAIN_MENU) {
            if(menu_index < 2) { menu_index++; }
        }
        else if(currentState == STATE_DDS_MODE_MENU) {
            if(slect_index == 0) {
                if(menu_index < 2) { menu_index++; }
            }
            else if(slect_index == 1) {
                DDS_Vpp_PlusorMinus(-(int32_t)step_vpp);
            }
            else if(slect_index == 2) {
                DDS_Freq_PlusorMinus(-(int32_t)step_freq);
            }
        }
    }
    else if(event == KEY_EVENT_LONG_PRESS_HOLD) {
        if(currentState == STATE_DDS_MODE_MENU) {
            if(slect_index == 1) { DDS_Vpp_PlusorMinus(-(int32_t)step_vpp); }
            else if(slect_index == 2) { DDS_Freq_PlusorMinus(-(int32_t)step_freq); }
        }
    }
}

static void Key_Handler_Confirm(uint8_t key_id, Key_Event_Type_t event) {
    if(event == KEY_EVENT_SINGLE_CLICK) {
        if     (currentState == STATE_MAIN_MENU) {
            if     (menu_index == 1) { currentState = STATE_DDS_MODE_MENU; menu_index = 1; slect_index = 0; }
            else if(menu_index == 2) { currentState = STATE_FIR_MODE_MENU; menu_index = 1; slect_index = 0; }
        }
        else if(currentState == STATE_DDS_MODE_MENU) {
            if     (slect_index == 0) {
                slect_index = menu_index;
                if     (slect_index == 1) { DDS_Vpp_Config(); }
                else if(slect_index == 2) { DDS_Freq_Config(); }
            }
            else if(slect_index == 1) {
                DDS_Vpp_Exec();
                slect_index = 0;
            }
            else if(slect_index == 2) {
                DDS_Freq_Exec();
                slect_index = 0;
            }
        }
        else if(currentState == STATE_FIR_MODE_MENU) {
            if(slect_index == 0) {
                slect_index = menu_index;
            }
            else if(slect_index == 1) {
                FIR_Learn();
                currentState = STATE_FIR_MODE_LEARNING;
                slect_index = 0;
            }
        }
        else if(currentState == STATE_FIR_MODE_LEARNING) {
            return;
        }
        else if(currentState == STATE_FIR_MODE_LEARN_COMPLETE) {
            FIR_Exec();
            currentState = STATE_FIR_MODE_MENU;
            slect_index = 0;
        }
    }
}

static void Key_Handler_Cancel(uint8_t key_id, Key_Event_Type_t event) {
    if(event == KEY_EVENT_SINGLE_CLICK) {
        if     (currentState == STATE_MAIN_MENU) {
            return;
        }
        else if(currentState == STATE_DDS_MODE_MENU) {
            if(slect_index == 0) {
                currentState = STATE_MAIN_MENU; menu_index = 1; slect_index = 0;
            }
            else if(slect_index == 1) {
                DDS_Vpp_Cancel();
                slect_index = 0;
            }
            else if(slect_index == 2) {
                DDS_Freq_Cancel();
                slect_index = 0;
            }
        }
        else if(currentState == STATE_FIR_MODE_MENU) {
            if(slect_index == 0) {
                currentState = STATE_MAIN_MENU; menu_index = 1; slect_index = 0;
            }
            else if(slect_index == 1) {
                slect_index = 0;
            }
        }
        else if(currentState == STATE_FIR_MODE_LEARNING) {
            FIR_Cancel();
            currentState = STATE_FIR_MODE_MENU;
            slect_index = 0;
        }
        else if(currentState == STATE_FIR_MODE_LEARN_COMPLETE) {
            FIR_Exec();
            currentState = STATE_FIR_MODE_MENU;
            slect_index = 0;
        }
    }
}


static void Key_Dispatch_Event(uint8_t key_id, Key_Event_Type_t event) {
    switch(key_id) {
        case KEY_ID_UP:      Key_Handler_Up(key_id, event);      break;
        case KEY_ID_DOWN:    Key_Handler_Down(key_id, event);    break;
        case KEY_ID_CONFIRM: Key_Handler_Confirm(key_id, event); break;
        case KEY_ID_CANCEL:  Key_Handler_Cancel(key_id, event);  break;
        default: break;
    }
}

void Key_Init(void) {
    XGpioPs_Config *ConfigPtr;
    ConfigPtr = XGpioPs_LookupConfig(XPAR_PS7_GPIO_0_DEVICE_ID);
    if (ConfigPtr != NULL) {
        XGpioPs_CfgInitialize(&Gpio, ConfigPtr, ConfigPtr->BaseAddr);
    }
    
    // 初始化按键结构体
    keys[KEY_ID_UP].id = KEY_ID_UP;
    keys[KEY_ID_UP].gpio_pin = 54;
    
    keys[KEY_ID_DOWN].id = KEY_ID_DOWN;
    keys[KEY_ID_DOWN].gpio_pin = 55;
    
    keys[KEY_ID_CONFIRM].id = KEY_ID_CONFIRM;
    keys[KEY_ID_CONFIRM].gpio_pin = 56;
    
    keys[KEY_ID_CANCEL].id = KEY_ID_CANCEL;
    keys[KEY_ID_CANCEL].gpio_pin = 57;

    for(uint8_t i = 0; i < KEY_COUNT; i++) {
        XGpioPs_SetDirectionPin(&Gpio, keys[i].gpio_pin, 0);
        keys[i].gpio_instance = &Gpio;
        keys[i].last_event = KEY_EVENT_NONE;
        keys[i].press_start_tick = 0;
        keys[i].is_pressed = 0;
        keys[i].long_press_triggered = 0;
    }
    
    vTaskDelay(pdMS_TO_TICKS(100));
}

void Key_Task(void *pvParameters) {
    TickType_t last_wake_time = xTaskGetTickCount();
    const TickType_t frequency = pdMS_TO_TICKS(50);
    
    while(1) {
        TickType_t current_tick = xTaskGetTickCount();

        for(uint8_t i = 0; i < KEY_COUNT; i++) {
            uint8_t current_state = XGpioPs_ReadPin(keys[i].gpio_instance, keys[i].gpio_pin);
            
            if(current_state == 0) {
                if(!keys[i].is_pressed) {
                    keys[i].is_pressed = 1;
                    keys[i].press_start_tick = current_tick;
                    keys[i].long_press_triggered = 0;
                } else {
                    TickType_t total_press_duration = current_tick - keys[i].press_start_tick;
                    if(total_press_duration >= pdMS_TO_TICKS(KEY_LONG_PRESS_MS)) {
                        
                        if(!keys[i].long_press_triggered) {
                            keys[i].long_press_triggered = 1;
                            keys[i].last_trigger_tick = current_tick;
                            keys[i].last_event = KEY_EVENT_LONG_PRESS_HOLD;
                            Key_Dispatch_Event(keys[i].id, KEY_EVENT_LONG_PRESS_HOLD);
                        } else {
                            TickType_t repeat_duration = current_tick - keys[i].last_trigger_tick;
                            if(repeat_duration >= pdMS_TO_TICKS(KEY_LONG_PRESS_REPEAT_MS)) {
                                keys[i].last_trigger_tick = current_tick;
                                keys[i].last_event = KEY_EVENT_LONG_PRESS_HOLD;
                                Key_Dispatch_Event(keys[i].id, KEY_EVENT_LONG_PRESS_HOLD);
                            }
                        }
                    }
                }
            } else {
                if(keys[i].is_pressed) {
                    TickType_t final_duration = current_tick - keys[i].press_start_tick;
                    if(!keys[i].long_press_triggered && final_duration >= pdMS_TO_TICKS(KEY_DEBOUNCE_MS)) {
                        keys[i].last_event = KEY_EVENT_SINGLE_CLICK;
                        Key_Dispatch_Event(keys[i].id, KEY_EVENT_SINGLE_CLICK);
                    }
                    keys[i].is_pressed = 0;
                    keys[i].long_press_triggered = 0;
                }
            }
        }
        vTaskDelayUntil(&last_wake_time, frequency);
    }
}

// void Key_Clear_Event(uint8_t key_id) {
//     if(key_id < KEY_COUNT) {
//         keys[key_id].last_event = KEY_EVENT_NONE;
//     }
// }
