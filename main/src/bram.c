#include "bram.h"

#include "xbram.h"
#include "xil_printf.h"
#include "xil_types.h"
#include "xparameters.h"

static XBram Bram;
static uint8_t Initialized = 0;

void BRAM_Init(void) {
    if (Initialized == 1) {
        return;
    }

    xil_printf("INFO[BRAM]: Initializing BRAM...\r\n");

    XBram_Config *ConfigPtr = XBram_LookupConfig(XPAR_BRAM_0_DEVICE_ID);
    if (ConfigPtr == NULL) {
        xil_printf("ERROR[BRAM]: BRAMDevice not found.\r\n");
        return;
    }

    uint8_t Status = XBram_CfgInitialize(&Bram, ConfigPtr, ConfigPtr->CtrlBaseAddress);
    if (Status != XST_SUCCESS) {
        xil_printf("ERROR[BRAM]: BRAM Init failed. (%d)\r\n", Status);
        return;
    }

    Status = XBram_SelfTest(&Bram, 0);
    if (Status != XST_SUCCESS) {
        xil_printf("ERROR[BRAM]: SelfTest failed. (%d)\r\n", Status);
        return;
    }

    Initialized = 1;
    xil_printf("INFO[BRAM]: BRAM Initialize Success.\r\n");
}

void BRAM_Write(uint32_t addr, uint32_t data_tx) {
    XBram_WriteReg(Bram.Config.MemBaseAddress, addr * 4, data_tx);
    // xil_printf("INFO[BRAM]: Write data 0x%08X to address addr=0x%08X\r\n",
    // data_tx, Bram.Config.MemBaseAddress + addr * 4);
}

uint32_t BRAM_Read(uint32_t addr) {
    uint32_t data_rx = XBram_ReadReg(Bram.Config.MemBaseAddress, addr * 4);
    // xil_printf("INFO[BRAM]: Read data 0x%08X from address addr=0x%08X\r\n",
    // data_rx, Bram.Config.MemBaseAddress + addr * 4);
    return data_rx;
}
