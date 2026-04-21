#include "key.h"

static Key_Handle_t keys[KEY_COUNT];

static XGpioPs Gpio;

extern volatile DisplayState_t currentState;
extern volatile uint8_t menu_index;
extern volatile uint8_t slect_index;

static void Key_Handler_Up(uint8_t key_id, Key_Event_Type_t event) {
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

static void Key_Handler_Down(uint8_t key_id, Key_Event_Type_t event) {
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

static void Key_Handler_Confirm(uint8_t key_id, Key_Event_Type_t event) {
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

static void Key_Handler_Cancel(uint8_t key_id, Key_Event_Type_t event) {
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
    const TickType_t frequency = pdMS_TO_TICKS(10); // 10ms 扫描一次
    
    while(1) {
        TickType_t current_tick = xTaskGetTickCount();

        for(uint8_t i = 0; i < KEY_COUNT; i++) {
            // 读取引脚状态 (低电平有效)
            uint8_t current_state = XGpioPs_ReadPin(keys[i].gpio_instance, keys[i].gpio_pin);
            
            if(current_state == 0) { // 【按键按下中】
                if(!keys[i].is_pressed) {
                    // --- 动作：首次按下 ---
                    keys[i].is_pressed = 1;
                    keys[i].press_start_tick = current_tick;     // 记录物理按下的绝对时刻
                    keys[i].long_press_triggered = 0;
                } else {
                    // --- 动作：持续按住 ---
                    // 计算从按下到现在总共过了多久
                    TickType_t total_press_duration = current_tick - keys[i].press_start_tick;
                    
                    // 检查是否达到长按门槛 (KEY_LONG_PRESS_MS = 1000ms)
                    if(total_press_duration >= pdMS_TO_TICKS(KEY_LONG_PRESS_MS)) {
                        
                        if(!keys[i].long_press_triggered) {
                            // 【首次触发长按】
                            keys[i].long_press_triggered = 1;
                            keys[i].last_trigger_tick = current_tick; // 记录本次动作触发的时刻
                            keys[i].last_event = KEY_EVENT_LONG_PRESS_HOLD;
                            
                            // 分发第一次长按事件
                            Key_Dispatch_Event(keys[i].id, KEY_EVENT_LONG_PRESS_HOLD);
                        } else {
                            // 【长按连发阶段】
                            // 计算距离上一次“动作触发”过了多久
                            TickType_t repeat_duration = current_tick - keys[i].last_trigger_tick;
                            
                            // 检查是否达到重复触发间隔 (KEY_LONG_PRESS_REPEAT_MS = 200ms)
                            if(repeat_duration >= pdMS_TO_TICKS(KEY_LONG_PRESS_REPEAT_MS)) {
                                keys[i].last_trigger_tick = current_tick; // 更新动作触发时刻
                                keys[i].last_event = KEY_EVENT_LONG_PRESS_HOLD;
                                
                                // 分发连发事件
                                Key_Dispatch_Event(keys[i].id, KEY_EVENT_LONG_PRESS_HOLD);
                            }
                        }
                    }
                }
            } else { // 【按键已释放】
                if(keys[i].is_pressed) {
                    // 计算释放前的总按住时间
                    TickType_t final_duration = current_tick - keys[i].press_start_tick;
                    
                    // 如果从未触发过长按，且按住时间超过去抖阈值，则判定为短按(单击)
                    if(!keys[i].long_press_triggered && final_duration >= pdMS_TO_TICKS(KEY_DEBOUNCE_MS)) {
                        keys[i].last_event = KEY_EVENT_SINGLE_CLICK;
                        Key_Dispatch_Event(keys[i].id, KEY_EVENT_SINGLE_CLICK);
                    }
                    
                    // 清空该按键所有状态，准备下一次按下
                    keys[i].is_pressed = 0;
                    keys[i].long_press_triggered = 0;
                }
            }
        }
        
        // 绝对定时，确保扫描周期稳定在 10ms
        vTaskDelayUntil(&last_wake_time, frequency);
    }
}

// void Key_Clear_Event(uint8_t key_id) {
//     if(key_id < KEY_COUNT) {
//         keys[key_id].last_event = KEY_EVENT_NONE;
//     }
// }
