#include "key.h"

static Button btn_up;
static Button btn_down;
static Button btn_confirm;
static Button btn_cancel;

XGpioPs Gpio;

void Key_Init(void) {
    XGpioPs_Config *ConfigPtr;
    ConfigPtr = XGpioPs_LookupConfig(XPAR_PS7_GPIO_0_DEVICE_ID);
    XGpioPs_CfgInitialize(&Gpio, ConfigPtr, ConfigPtr->BaseAddr);
    XGpioPs_SetDirectionPin(&Gpio, 54, 0); // UP
    XGpioPs_SetDirectionPin(&Gpio, 55, 0); // DOWN
    XGpioPs_SetDirectionPin(&Gpio, 56, 0); // CONFIRM
    XGpioPs_SetDirectionPin(&Gpio, 57, 0); // CANCEL
    vTaskDelay(pdMS_TO_TICKS(100)); 

    // 初始化每个按键 (假设都是低电平有效)
    button_init(&btn_up,      Key_Read_Callback, 0, 0);
    button_init(&btn_down,    Key_Read_Callback, 0, 1);
    button_init(&btn_confirm, Key_Read_Callback, 0, 2);
    button_init(&btn_cancel,  Key_Read_Callback, 0, 3);

    // 绑定事件
    button_attach(&btn_up,      BTN_SINGLE_CLICK, Key_Event_Up_SingleClcick_Handler,      NULL);
    button_attach(&btn_down,    BTN_SINGLE_CLICK, Key_Event_Down_SingleClcick_Handler,    NULL);
    button_attach(&btn_confirm, BTN_SINGLE_CLICK, Key_Event_Confirm_SingleClcick_Handler, NULL);
    button_attach(&btn_cancel,  BTN_SINGLE_CLICK, Key_Event_Cancel_SingleClcick_Handler,  NULL);
    
    // 启动所有按键
    button_start(&btn_up);
    button_start(&btn_down);
    button_start(&btn_confirm);
    button_start(&btn_cancel);
}

void Key_Ticks(void) {
    button_ticks();
}

uint8_t Key_Read_Callback(uint8_t button_id) {
    switch(button_id) {
        case 0: return XGpioPs_ReadPin(&Gpio, 54);
        case 1: return XGpioPs_ReadPin(&Gpio, 55);
        case 2: return XGpioPs_ReadPin(&Gpio, 56);
        case 3: return XGpioPs_ReadPin(&Gpio, 57);
        default: return 1;
    }
}

void Key_Event_Up_SingleClcick_Handler(Button* btn, void* user_data) {
    if(currentState == STATE_MAIN_MENU) {
        if(menu_index == 1) {}
        if(menu_index == 2) { menu_index = 1; }
    }
}

void Key_Event_Down_SingleClcick_Handler(Button* btn, void* user_data) {
    if(currentState == STATE_MAIN_MENU) {
        if(menu_index == 1) { menu_index = 2; }
        if(menu_index == 2) {}
    }
}

void Key_Event_Confirm_SingleClcick_Handler(Button* btn, void* user_data) {
    // 逻辑代码
}

void Key_Event_Cancel_SingleClcick_Handler(Button* btn, void* user_data) {
    // 逻辑代码
}
