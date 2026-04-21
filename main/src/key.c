#include "key.h"

static Key_Handle_t keys[KEY_COUNT];

static XGpioPs Gpio;

extern volatile DisplayState_t currentState;
extern volatile uint8_t menu_index;
extern volatile uint8_t slect_index;

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
    const TickType_t frequency = pdMS_TO_TICKS(10);
    
    while(1) {
        TickType_t current_tick = xTaskGetTickCount();

        for(uint8_t i = 0; i < KEY_COUNT; i++) {
            // 读取引脚状态 (假设低电平有效)
            uint8_t current_state = XGpioPs_ReadPin(keys[i].gpio_instance, keys[i].gpio_pin);
            
            if(current_state == 0) { // 按键按下
                if(!keys[i].is_pressed) {
                    // 刚开始按下
                    keys[i].is_pressed = 1;
                    keys[i].press_start_tick = current_tick;
                    keys[i].long_press_triggered = 0;
                } else {
                    // 持续按下
                    TickType_t press_duration = current_tick - keys[i].press_start_tick;
                    
                    // 检查是否达到长按阈值
                    if(press_duration >= pdMS_TO_TICKS(KEY_LONG_PRESS_MS)) {
                        if(!keys[i].long_press_triggered) {
                            // 第一次触发长按
                            keys[i].long_press_triggered = 1;
                            keys[i].last_event = KEY_EVENT_LONG_PRESS_HOLD;
                            
                            // 分发事件
                            switch(keys[i].id) {
                                case KEY_ID_UP: Key_Handler_Up(keys[i].id, KEY_EVENT_LONG_PRESS_HOLD); break;
                                case KEY_ID_DOWN: Key_Handler_Down(keys[i].id, KEY_EVENT_LONG_PRESS_HOLD); break;
                                case KEY_ID_CONFIRM: Key_Handler_Confirm(keys[i].id, KEY_EVENT_LONG_PRESS_HOLD); break;
                                case KEY_ID_CANCEL: Key_Handler_Cancel(keys[i].id, KEY_EVENT_LONG_PRESS_HOLD); break;
                            }
                            
                            // 重置计时器以便后续重复触发
                            keys[i].press_start_tick = current_tick;
                        } else {
                            // 长按重复触发
                            TickType_t repeat_duration = current_tick - keys[i].press_start_tick;
                            if(repeat_duration >= pdMS_TO_TICKS(KEY_LONG_PRESS_REPEAT_MS)) {
                                keys[i].last_event = KEY_EVENT_LONG_PRESS_HOLD;
                                
                                // 分发事件
                                switch(keys[i].id) {
                                    case KEY_ID_UP: Key_Handler_Up(keys[i].id, KEY_EVENT_LONG_PRESS_HOLD); break;
                                    case KEY_ID_DOWN: Key_Handler_Down(keys[i].id, KEY_EVENT_LONG_PRESS_HOLD); break;
                                    case KEY_ID_CONFIRM: Key_Handler_Confirm(keys[i].id, KEY_EVENT_LONG_PRESS_HOLD); break;
                                    case KEY_ID_CANCEL: Key_Handler_Cancel(keys[i].id, KEY_EVENT_LONG_PRESS_HOLD); break;
                                }
                                
                                // 重置计时器以便下一次重复触发
                                keys[i].press_start_tick = current_tick;
                            }
                        }
                    }
                }
            } else { // 按键释放
                if(keys[i].is_pressed) {
                    TickType_t press_duration = current_tick - keys[i].press_start_tick;
                    
                    // 如果是短按 (大于去抖时间，且未触发长按，避免长按重复触发刷新计时后误判)
                    if(!keys[i].long_press_triggered && press_duration >= pdMS_TO_TICKS(KEY_DEBOUNCE_MS)) {
                        keys[i].last_event = KEY_EVENT_SINGLE_CLICK;
                        Key_Dispatch_Event(keys[i].id, KEY_EVENT_SINGLE_CLICK);
                    }
                    
                    // 重置状态
                    keys[i].is_pressed = 0;
                    keys[i].long_press_triggered = 0;
                }
            }
        }
        
        vTaskDelayUntil(&last_wake_time, frequency);
    }
}

void Key_Clear_Event(uint8_t key_id) {
    if(key_id < KEY_COUNT) {
        keys[key_id].last_event = KEY_EVENT_NONE;
    }
}

void Key_Handler_Up(uint8_t key_id, Key_Event_Type_t event) {
    if(event == KEY_EVENT_SINGLE_CLICK) {
        if(currentState == STATE_MAIN_MENU) {
            if(menu_index > 1) { menu_index--; }
        }
        else if(currentState == STATE_DDS_MODE_MENU) {
            if(slect_index == 0) {
                if(menu_index > 1) { menu_index--; }
            }
            else if(slect_index == 1) {
                DDS_Vpp_Plus();
            }
            else if(slect_index == 2) {
                DDS_Freq_Plus(); 
            }
        }
    }
    else if(event == KEY_EVENT_LONG_PRESS_HOLD) {
        if(currentState == STATE_DDS_MODE_MENU) {
            if(slect_index == 1) { DDS_Vpp_Plus(); }
            else if(slect_index == 2) { DDS_Freq_Plus(); }
        }
    }
}

void Key_Handler_Down(uint8_t key_id, Key_Event_Type_t event) {
    if(event == KEY_EVENT_SINGLE_CLICK) {
        if(currentState == STATE_MAIN_MENU) {
            if(menu_index < 2) { menu_index++; }
        }
        else if(currentState == STATE_DDS_MODE_MENU) {
            if(slect_index == 0) {
                if(menu_index < 2) { menu_index++; }
            }
            else if(slect_index == 1) {
                DDS_Vpp_Minus();
            }
            else if(slect_index == 2) {
                DDS_Freq_Minus();
            }
        }
    }
    else if(event == KEY_EVENT_LONG_PRESS_HOLD) {
        if(currentState == STATE_DDS_MODE_MENU) {
            if(slect_index == 1) { DDS_Vpp_Minus(); }
            else if(slect_index == 2) { DDS_Freq_Minus(); }
        }
    }
}

void Key_Handler_Confirm(uint8_t key_id, Key_Event_Type_t event) {
    if(event == KEY_EVENT_SINGLE_CLICK) {
        if(currentState == STATE_MAIN_MENU) {
            if(menu_index == 1) { currentState = STATE_DDS_MODE_MENU; }
            else if(menu_index == 2) { currentState = STATE_FIR_MODE_MENU; }
        }
        else if(currentState == STATE_DDS_MODE_MENU) {
            if(slect_index == 0) {
                slect_index = menu_index; 
                if(slect_index == 1) { DDS_Vpp_Config(); }
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
    }
}

void Key_Handler_Cancel(uint8_t key_id, Key_Event_Type_t event) {
    if(event == KEY_EVENT_SINGLE_CLICK) {
        if(currentState == STATE_MAIN_MENU) {
            return;
        }
        else if(currentState == STATE_DDS_MODE_MENU) {
            if(slect_index == 0) {
                currentState = STATE_MAIN_MENU;
                menu_index = 1;
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
    }
}
