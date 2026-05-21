#include "xil_printf.h"
#include "xil_types.h"
#include "xparameters.h"

#include "FreeRTOS.h"
#include "task.h"

#include "bram.h"
#include "display.h"
#include "key.h"

TaskHandle_t xDisplayTaskHandle;
TaskHandle_t xTaskKeyPollingHandle;
TaskHandle_t xMainTaskHandle;

static void vDisplayTask(void *pvParameters) {
    Display_Init();

    for (;;) {
        Display_Refresh();
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

static void vTaskKeyPolling(void *pvParameters) {
    Key_Init();
    Key_Task(NULL);
}

static void vMainTask(void *pvParameters) {
    xil_printf("INFO[RTOS]: Main Task running.\r\n");
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

int main(void) {
    BRAM_Init();

    xil_printf("INFO[RTOS]: Create FreeRTOS Tasks...\r\n");
    xTaskCreate(vDisplayTask, "Display", 4096, NULL, tskIDLE_PRIORITY + 2, &xDisplayTaskHandle);
    xil_printf("INFO[RTOS]: Display Task Created.\r\n");
    xTaskCreate(vTaskKeyPolling, "Key", 4096, NULL, tskIDLE_PRIORITY + 2, &xTaskKeyPollingHandle);
    xil_printf("INFO[RTOS]: Key Task Created.\r\n");
    xTaskCreate(vMainTask, "Main", 2048, NULL, tskIDLE_PRIORITY + 1, &xMainTaskHandle);
    xil_printf("INFO[RTOS]: Main Task Created.\r\n");

    xil_printf("INFO[RTOS]: Start FreeRTOS Scheduler...\r\n");
    vTaskStartScheduler();

    for (;;) {
    }
}
