# PS-BRAM 软硬件协同操作参考文档 (bram_2_rtl_ctrl)

本手册用于指导 PS 侧软件工程师通过 AXI BRAM Controller 读写 PL 侧的 `bram_2_rtl_ctrl` 模块。本设计采用标准字节寻址（Byte Addressing）机制。

---

## 1. 内存映射与地址映射表

PS 端编程时请直接使用字节偏移量（Byte Offset）进行寄存器寻址。

| 寄存器名称 | 字地址索引 | 字节偏移量 | 属性 | 功能描述 |
|---|---|---|---|---|
| ADDR_DDS | `0` | `0x00` | R | DDS 配置参数寄存器 |
| ADDR_GAIN | `1` | `0x04` | R | ADC/DAC 增益配置寄存器 |
| ADDR_SWEEP | `2` | `0x08` | W | 扫频结果数据写入寄存器 |
| ADDR_STATUS | `3` | `0x0C` | RW | 核心状态与控制寄存器 |
| ADDR_FIR | `4+` | `0x10+` | R | FIR 滤波器系数存储区（共 1040 个字） |

---

## 2. 核心寄存器位域详细解析

### 2.1 ADDR_STATUS (偏移量 `0x0C`)

该寄存器负责握手、触发以及通道/延迟配置。触发位由 **PS 软件置 `1` 触发，PL 硬件处理完成后自动清零**。

| 位域 | 信号名称 | 属性 | 复位值 | 功能描述 |
|---|---|---|---|---|
| `[0]` | `DDS_TRIG` | RW | `0` | DDS 配置触发位：写 `1` 启动 DDS 传输，完成后自动清零 |
| `[1]` | `GAIN_TRIG` | RW | `0` | 增益配置触发位：写 `1` 启动增益更新，完成后自动清零 |
| `[2]` | `FIR_TRIG` | RW | `0` | FIR 系数重载触发位：写 `1` 启动系数流传输，完成后自动清零 |
| `[3]` | `SEL` | RW | `0` | 通道选择位：`1` 通道 A，`0` 通道 B |
| `[4]` | `SWEEP_TRIG` | RW | `0` | 扫频触发位：写 `1` 等待并捕获扫频数据，完成后自动清零 |
| `[5]` | `DELAY_TRIG` | RW | `0` | 延迟配置触发位：写 `1` 更新延迟，完成后自动清零，同步置位 `DELAY_DONE` |
| `[6]` | `DELAY_DONE` | R | `0` | 延迟配置完成标志：PL 侧硬件拉高，表示延迟生效 |
| `[22:7]` | `DELAY_VAL` | RW | `0` | 延迟配置参数值：16 位数据，随 `DELAY_TRIG` 触发发送至底层 |
| `[31:23]` | `RESERVED` | R | `0` | 保留位 |

### 2.2 ADDR_GAIN (偏移量 `0x04`)

写入一个 32 位数据，PL 侧自动拆分为 ADC 和 DAC 两组独立增益。

| 位域 | 信号名称 | 属性 | 复位值 | 功能描述 |
|---|---|---|---|---|
| `[11:0]` | `ADC_GAIN` | W | `0` | ADC 增益参数：12 位有效数据 |
| `[15:12]` | `RESERVED` | R | `0` | 保留位 |
| `[27:16]` | `DAC_GAIN` | W | `0` | DAC 增益参数：12 位有效数据 |
| `[31:28]` | `RESERVED` | R | `0` | 保留位 |

---

## 3. C 语言底层驱动封装

基于 Xilinx 官方驱动库 `xbram.h` 的寄存器读写封装：

```c
#include "xbram.h"
#include "xil_printf.h"

extern XBram Bram;
extern u8 Initialized;

uint32_t BRAM_ReadReg(uint32_t reg_idx) {
    if (!Initialized) return 0;
    return XBram_ReadReg(Bram.Config.MemBaseAddress, reg_idx * 4);
}

void BRAM_WriteReg(uint32_t reg_idx, uint32_t data) {
    if (!Initialized) return;
    XBram_WriteReg(Bram.Config.MemBaseAddress, reg_idx * 4, data);
}
```

---

## 4. 典型操作代码示例

### 4.1 通道切换与 DDS 参数配置

先配置 DDS 数据，再修改通道并拉高触发位。

```c
void Config_DDS(uint8_t channel_sel, uint32_t dds_data) {
    BRAM_WriteReg(0, dds_data);

    uint32_t status = BRAM_ReadReg(3);
    if (channel_sel == 1)
        status |= (1 << 3);    // 通道 A
    else
        status &= ~(1 << 3);   // 通道 B
    status |= (1 << 0);        // DDS_TRIG
    BRAM_WriteReg(3, status);

    while (BRAM_ReadReg(3) & (1 << 0));
    xil_printf("[BRAM] DDS Configuration Done!\r\n");
}
```

### 4.2 更新 ADC/DAC 增益

拼接 12 位双通道增益数据，一并写入并触发。

```c
void Update_Gain(uint16_t adc_gain, uint16_t dac_gain) {
    uint32_t gain_payload = ((uint32_t)(dac_gain & 0xFFF) << 16) | (adc_gain & 0xFFF);
    BRAM_WriteReg(1, gain_payload);

    uint32_t status = BRAM_ReadReg(3);
    status |= (1 << 1);        // GAIN_TRIG
    BRAM_WriteReg(3, status);

    while (BRAM_ReadReg(3) & (1 << 1));
}
```

### 4.3 配置延迟参数

将 16 位延迟量写入状态寄存器 `[22:7]` 并触发，随后检测 `DELAY_DONE`。

```c
void Set_Delay_Config(uint16_t delay_val) {
    uint32_t status = BRAM_ReadReg(3);
    status &= ~(0xFFFF << 7);
    status &= ~(1 << 5);
    status |= ((uint32_t)delay_val << 7);
    status |= (1 << 5);        // DELAY_TRIG
    BRAM_WriteReg(3, status);

    while (!(BRAM_ReadReg(3) & (1 << 6)));
    xil_printf("[BRAM] Delay configuration verified!\r\n");
}
```

### 4.4 重载 FIR 滤波器系数

硬件状态机单次读取 BRAM 32 位数据，内部拆分为低 16 位和高 16 位发送给 FIR IP 核。重载需传输 **2080 个 16 位系数**（对应 **1040 个 32 位字** 空间）。

> 注：当前 PL 源码 `SEND_FIR_HIGH` 状态中存在 `addr_counter <= addr_counter;` 地址未递增的 Bug，待 PL 修复后软件侧数据从地址 `4` 顺序写入即可。

```c
void Reload_FIR_Coefficients(uint32_t *coeffs_1040_words) {
    for (int i = 0; i < 1040; i++) {
        BRAM_WriteReg(4 + i, coeffs_1040_words[i]);
    }

    uint32_t status = BRAM_ReadReg(3);
    status |= (1 << 2);        // FIR_TRIG
    BRAM_WriteReg(3, status);

    while (BRAM_ReadReg(3) & (1 << 2));
    xil_printf("[BRAM] 2080 FIR Coefficients reloaded!\r\n");
}
```

### 4.5 读取扫频捕获结果

软件拉高 SWEEP_TRIG 通知硬件就绪，等待 PL 将 AXI-Stream 总线数据写入 BRAM 后读取。

```c
uint32_t Read_Sweep_Result(void) {
    uint32_t status = BRAM_ReadReg(3);
    status |= (1 << 4);        // SWEEP_TRIG
    BRAM_WriteReg(3, status);

    while (BRAM_ReadReg(3) & (1 << 4));

    uint32_t sweep_data = BRAM_ReadReg(2);
    return sweep_data;
}
```
