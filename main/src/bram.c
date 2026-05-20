#include "bram.h"

static XBram Bram;
static int Initialized = 0;

void BRAM_Init(void) {
  if (Initialized)
    return;

  XBram_Config *ConfigPtr = XBram_LookupConfig(XPAR_BRAM_0_DEVICE_ID);
  if (ConfigPtr == NULL) {
    xil_printf("BRAM ERROR: Device not found\r\n");
    return;
  }

  int Status =
      XBram_CfgInitialize(&Bram, ConfigPtr, ConfigPtr->CtrlBaseAddress);
  if (Status != XST_SUCCESS) {
    xil_printf("BRAM ERROR: Init failed (%d)\r\n", Status);
    return;
  }

  Status = XBram_SelfTest(&Bram, 0);
  if (Status != XST_SUCCESS) {
    xil_printf("BRAM ERROR: SelfTest failed (%d)\r\n", Status);
    return;
  }

  Initialized = 1;
  xil_printf("[BRAM] BRAM Initialize: OK\r\n");
}

int BRAM_SelfTest(void) {
  if (!Initialized)
    return XST_FAILURE;
  return XBram_SelfTest(&Bram, 0);
}

void BRAM_Write(uint32_t addr, uint32_t data) {
  xil_printf("[BRAM] BRAM Write: data=0x%08X -> addr=0x%08X\r\n", data, Bram.Config.MemBaseAddress + addr * 4);

  XBram_WriteReg(Bram.Config.MemBaseAddress, addr * 4, data);
}

uint32_t BRAM_Read(uint32_t addr) {
  if (!Initialized)
    return 0;
  return XBram_ReadReg(Bram.Config.MemBaseAddress, addr * 4);
}