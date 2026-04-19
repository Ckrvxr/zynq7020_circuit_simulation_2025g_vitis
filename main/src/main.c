#include "FreeRTOS.h"
#include "task.h"
#include "xparameters.h"
#include "xil_printf.h"
#include "display.h"

TaskHandle_t xDisplayTaskHandle;

static void vDisplayTask(void *pvParameters) {
    Display_Init();

    for( ;; ) {
        Display_Refresh();
    }
}

int main( void )
{
    xTaskCreate(vDisplayTask, "Display", 2048, NULL, tskIDLE_PRIORITY + 2, &xDisplayTaskHandle);

    xil_printf("System Starting...\r\n");
    
    vTaskStartScheduler();

    for( ;; );
}
