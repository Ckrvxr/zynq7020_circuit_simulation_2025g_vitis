#include "FreeRTOS.h"
#include "task.h"

#include "xparameters.h"
#include "xil_printf.h"

#include "display.h"
#include "key.h"

TaskHandle_t xDisplayTaskHandle;

static void vDisplayTask(void *pvParameters) {
    Display_Init();

    for( ;; ) {
        Display_Refresh();
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void vTaskKeyPolling(void *pvParameters) {
    Key_Init();

    for(;;) {
        Key_Ticks();
        vTaskDelay(pdMS_TO_TICKS(10)); 
    }
}

int main( void )
{
    xTaskCreate(vDisplayTask, "Display", 2048, NULL, tskIDLE_PRIORITY + 2, &xDisplayTaskHandle);
    xTaskCreate(vTaskKeyPolling, "Key", 256, NULL, tskIDLE_PRIORITY + 2, &xDisplayTaskHandle);

    xil_printf("System Starting...\r\n");
    
    vTaskStartScheduler();

    for( ;; );
}
