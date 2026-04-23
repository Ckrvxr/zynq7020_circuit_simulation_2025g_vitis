#include "FreeRTOS.h"
#include "task.h"

#include "xparameters.h"
#include "xil_printf.h"

#include "display.h"
#include "key.h"

TaskHandle_t xDisplayTaskHandle;
TaskHandle_t xTaskKeyPollingHandle;

static void vDisplayTask(void *pvParameters) {
    Display_Init();

    for( ;; ) {
        Display_Refresh();
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

void vTaskKeyPolling(void *pvParameters) {
    Key_Init();
    Key_Task(NULL);
}

int main( void )
{
    xTaskCreate(vDisplayTask, "Display", 2048, NULL, tskIDLE_PRIORITY + 2, &xDisplayTaskHandle);
    xTaskCreate(vTaskKeyPolling, "Key", 2048, NULL, tskIDLE_PRIORITY + 2, &xTaskKeyPollingHandle);

    xil_printf("System Starting...\r\n");
    
    vTaskStartScheduler();

    for( ;; );
}
