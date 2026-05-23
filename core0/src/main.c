#include "xil_io.h"

#define CPU1_MAILBOX   0xFFFFFFF0
#define MAIN_ELF_ENTRY 0x00100000

int main(void) {
    Xil_Out32(CPU1_MAILBOX, MAIN_ELF_ENTRY);
    __asm__("dsb\n\tisb\n\tsev");
    while (1) __asm__("wfe");
    return 0;
}
