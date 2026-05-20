#include "FreeRTOS.h"
#include "task.h"

#include "xil_printf.h"
#include "xil_types.h"
#include "xparameters.h"

#include "display.h"
#include "key.h"
#include "bram.h"

TaskHandle_t xDisplayTaskHandle;
TaskHandle_t xTaskKeyPollingHandle;
TaskHandle_t xMainTaskHandle;

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

static volatile uint32_t data = 0xF0000004;
static volatile uint32_t addr_w  = 3;
static volatile uint32_t addr_r  = 3;
static volatile uint32_t tmp;

void vMainTask(void *pvParameters) {
  while (1) {
	  	BRAM_Write(addr_w, data);
        vTaskDelay(pdMS_TO_TICKS(1000));
        tmp = BRAM_Read(addr_r);
        xil_printf("[MainTask] Read from BRAM: addr=0x%08X, data=0x%08X\r\n", addr_r * 4, tmp);
  }
}

int main( void )
{
    BRAM_Init();

    xTaskCreate(vDisplayTask, "Display", 2048, NULL, tskIDLE_PRIORITY + 2, &xDisplayTaskHandle);
    xTaskCreate(vTaskKeyPolling, "Key", 2048, NULL, tskIDLE_PRIORITY + 2, &xTaskKeyPollingHandle);
    xTaskCreate(vMainTask, "Main", 2048, NULL, tskIDLE_PRIORITY + 1, &xMainTaskHandle);

    xil_printf("System Starting...\r\n");
    
    vTaskStartScheduler();



    for( ;; );
}
