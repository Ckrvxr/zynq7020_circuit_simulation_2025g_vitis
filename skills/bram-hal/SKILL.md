---
name: bram-hal
description: 为嵌入式 SoC（FPGA+CPU 协同）编写 PS-PL 共享内存通信硬件抽象层的通用指南
---

## 架构总览

### 分层定位

```
App Logic (dds.c / fir.c / key.c)
    ↓
BRAM HAL (bram.c / bram.h)    ← 唯一直接操作硬件的层
    ↓
Vendor Driver (Xilinx XBram)
    ↓
AXI Bus → PL (FPGA)
```

BRAM HAL 是系统中**唯一与 PL 硬件通信的软件层**，所有上层模块（DDS、FIR 等）的硬件交互必须经过这一层。

### 文件职责

| 文件 | 职责 |
|------|------|
| `bram.h` | 3 个公有函数声明 |
| `bram.c` | 初始化、单字读写封装（共 51 行） |
| `refer.md` | 寄存器映射文档、典型操作示例 |

---

## 1. BRAM HAL 层设计

### 1.1 单例初始化模式

```c
static XBram Bram;
static uint8_t Initialized = 0;

void BRAM_Init(void) {
    if (Initialized == 1) return;  // 防重复初始化

    XBram_Config *ConfigPtr = XBram_LookupConfig(DEVICE_ID);
    if (ConfigPtr == NULL) { log_error(); return; }

    uint8_t Status = XBram_CfgInitialize(&Bram, ConfigPtr, ConfigPtr->CtrlBaseAddress);
    if (Status != XST_SUCCESS) { log_error(); return; }

    Status = XBram_SelfTest(&Bram, 0);
    if (Status != XST_SUCCESS) { log_error(); return; }

    Initialized = 1;
}
```

关键要素：
- **Singleton guard**：`Initialized` 旗标防止重复初始化
- **Vendor HAL 三段式**：LookupConfig → CfgInitialize → SelfTest（全平台通用模式）
- **错误不阻塞**：打印 `ERROR` 日志后 return，上层调用方通过行为异常感知失败

### 1.2 字地址 vs 字节地址

```c
void BRAM_Write(uint32_t addr, uint32_t data) {
    XBram_WriteReg(Bram.Config.MemBaseAddress, addr * 4, data);
}

uint32_t BRAM_Read(uint32_t addr) {
    return XBram_ReadReg(Bram.Config.MemBaseAddress, addr * 4);
}
```

- 对外 API 使用**字地址**（0, 1, 2, ...）
- 内部乘以 4 转换为**字节偏移**（0x00, 0x04, 0x08, ...）
- 这个转换对上层透明，避免调用方频繁做乘法

### 1.3 API 设计原则

| 设计点 | 选择 | 理由 |
|--------|------|------|
| 返回值 | void / uint32_t | 简化调用方代码，错误仅打日志 |
| 参数类型 | uint32_t | 与硬件寄存器位宽对齐 |
| 字段名 | addr, data | 清晰无歧义 |
| 调试日志 | 注释掉 | 保留以备用，生产环境关闭 |
| 越界保护 | 无 | 依赖调用方传入正确的地址 |

---

## 2. 寄存器映射约定

### 2.1 寄存器表标准格式

每个 PS-PL 协同项目应维护一份寄存器映射文档（如本项目 `refer.md`）：

| 寄存器名 | 字地址 | 字节偏移 | 属性 | 功能描述 |
|----------|--------|----------|------|----------|
| ADDR_DDS | 0 | 0x00 | RW | DDS 配置参数寄存器 |
| ADDR_GAIN | 1 | 0x04 | RW | ADC/DAC 增益配置寄存器 |
| ADDR_SWEEP | 2 | 0x08 | R | 扫频结果数据寄存器 |
| ADDR_STATUS | 3 | 0x0C | RW | 核心状态与控制寄存器 |
| ADDR_FIR | 4+ | 0x10+ | RW | FIR 滤波器系数存储区 |

### 2.2 寄存器属性分类

| 属性 | 含义 | 典型用途 |
|------|------|----------|
| R | 只读 | 状态标志、测量结果 |
| W | 只写 | 配置字、控制字（不关心回读值） |
| RW | 可读可写 | 需 Read-Modify-Write 的控制寄存器 |
| RW (自清零) | 写 1 触发，硬件清零 | 触发/应答握手 |

### 2.3 寄存器地址宏定义

```c
// 将所有地址定义为宏，集中管理
#define REG_DDS_OUT     0
#define REG_GAIN        1
#define REG_SWEEP       2
#define REG_STATUS      3
#define REG_FIR_BASE    4
```

---

## 3. 触发/应答握手协议

### 3.1 基本流程

```
PS 写数据 → PS 置位 STATUS 触发位 → [PL 检测触发 → 执行操作 → 清除触发位] → PS 轮询等待清零
```

```c
void Hal_TriggeredWrite(uint32_t data_reg, uint32_t data,
                        uint8_t trigger_bit) {
    hal_write(data_reg, data);                    // 1. 写数据
    uint32_t status = hal_read(REG_STATUS);        // 2. 读 STATUS
    status |= (1 << trigger_bit);                  // 3. 置位触发
    hal_write(REG_STATUS, status);
    do {                                           // 4. 轮询等待 PL 清零
        hal_delay_ms(1);
        status = hal_read(REG_STATUS);
    } while (status & (1 << trigger_bit));
}
```

### 3.2 单触发 vs 合并多触发

| 场景 | 触发方式 |
|------|----------|
| 单次配置（如仅 DDS 频率） | 只置位 DDS_TRIG |
| 同时配置频率 + 增益 | 同时置位 DDS_TRIG \| GAIN_TRIG |
| 先配数据再批量触发 | 先写完所有数据寄存器，最后一次性置位多个触发位 |

```c
// 合并触发示例：DDS + GAIN 同时配置
uint32_t status = hal_read(REG_STATUS);
status |= (1 << DDS_TRIG_BIT) | (1 << GAIN_TRIG_BIT);
hal_write(REG_STATUS, status);
do { status = hal_read(REG_STATUS); }
    while (status & ((1 << DDS_TRIG_BIT) | (1 << GAIN_TRIG_BIT)));
```

### 3.3 超时保护

```c
uint32_t Hal_WaitForClear(uint8_t trigger_bit, uint32_t timeout_ms) {
    uint32_t elapsed = 0;
    while (hal_read(REG_STATUS) & (1 << trigger_bit)) {
        hal_delay_ms(1);
        if (++elapsed >= timeout_ms) {
            log_error("trigger bit %d timeout after %d ms", trigger_bit, elapsed);
            return 1;  // 超时
        }
    }
    return 0;  // 成功
}
```

---

## 4. 位域操作模式

### 4.1 Read-Modify-Write 三步法

当多个参数共用一个 32 位寄存器时（如 ADDR_GAIN 同时包含 ADC_GAIN 和 DAC_GAIN），必须使用 RMW 模式：

```c
// 示例：更新 DAC_GAIN（位 [27:16]），保留 ADC_GAIN（位 [11:0]）
uint32_t val = hal_read(REG_GAIN);       // 1. 读当前值
val &= ~(0x0FFF << 16);                  // 2. 清目标位域
val |= (dac_gain & 0x0FFF) << 16;        // 3. 写新值到目标位域
hal_write(REG_GAIN, val);                // 4. 回写
```

**常见错误**：直接 `hal_write(REG_GAIN, dac_gain << 16)` — 这会将 ADC_GAIN 覆盖为 0。

### 4.2 位域操作宏

```c
#define BIT(n)              (1UL << (n))
#define BIT_MASK(width)     ((1UL << (width)) - 1)
#define REG_SET_BITS(val, shift, width, data)   \
    (((val) & ~(BIT_MASK(width) << (shift))) |  \
     (((data) & BIT_MASK(width)) << (shift)))
#define REG_GET_BITS(val, shift, width)         \
    (((val) >> (shift)) & BIT_MASK(width))
```

使用示例：

```c
// 写入 DAC_GAIN（12 位，偏移 16）
val = REG_SET_BITS(val, 16, 12, new_dac_gain);
// 写入 ADC_GAIN（12 位，偏移 0）
val = REG_SET_BITS(val, 0, 12, new_adc_gain);
// 读取 DAC_GAIN
uint32_t dac = REG_GET_BITS(hal_read(REG_GAIN), 16, 12);
```

### 4.3 常用位域配置

| 寄存器 | 位域 | 宽度 | 偏移 | 用途 |
|--------|------|------|------|------|
| STATUS | DDS_TRIG | 1 | 0 | DDS 触发 |
| STATUS | GAIN_TRIG | 1 | 1 | 增益触发 |
| STATUS | FIR_TRIG | 1 | 2 | FIR 系数重载 |
| STATUS | SEL | 1 | 3 | 通道选择 |
| STATUS | SWEEP_TRIG | 1 | 4 | 扫频触发 |
| GAIN | ADC_GAIN | 12 | 0 | ADC 增益 |
| GAIN | DAC_GAIN | 12 | 16 | DAC 增益 |

---

## 5. 批量数据传输模式

### 5.1 顺序写 + 触发

适用于传输大批量数据（如 FIR 系数加载）：

```c
void LoadBulkData(uint32_t base_addr, uint32_t *data, int num_words) {
    // 1. 顺序写入所有数据
    for (int i = 0; i < num_words; i++) {
        hal_write(base_addr + i, data[i]);
    }
    // 2. 一次性触发加载
    uint32_t status = hal_read(REG_STATUS);
    status |= (1 << FIR_TRIG_BIT);
    hal_write(REG_STATUS, status);
    // 3. 等待 PL 处理完成
    do { status = hal_read(REG_STATUS); }
        while (status & (1 << FIR_TRIG_BIT));
}
```

PL 侧状态机会从 `base_addr` 起顺次读取。软件侧需要确保数据完全写入后再触发。

### 5.2 触发读取

适用于单次测量结果读取（如扫频结果）：

```c
uint32_t Hal_TriggeredRead(uint8_t trigger_bit, uint32_t result_reg) {
    uint32_t status = hal_read(REG_STATUS);
    status |= (1 << trigger_bit);
    hal_write(REG_STATUS, status);           // 触发 PL 将数据写入 BRAM
    do { status = hal_read(REG_STATUS); }
        while (status & (1 << trigger_bit)); // 等待完成
    return hal_read(result_reg);             // 读取结果
}
```

---

## 6. 完整的 PS-PL 通信协议模板

将寄存器定义、触发/应答、位域操作整合为一个结构化的通信层：

```c
// pspl_hal.h

#pragma once
#include <stdint.h>

// === 寄存器地址映射 ===
#define REG_CFG_A       0
#define REG_CFG_B       1
#define REG_RESULT      2
#define REG_STATUS      3
#define REG_BULK_BASE   4

// === STATUS 寄存器位域定义 ===
#define STATUS_TRIG_A       BIT(0)
#define STATUS_TRIG_B       BIT(1)
#define STATUS_TRIG_BULK    BIT(2)
#define STATUS_CH_SEL       BIT(3)
#define STATUS_TRIG_SWEEP   BIT(4)

// === 位域操作宏 ===
#define BIT(n)              (1UL << (n))
#define BIT_MASK(w)         ((1UL << (w)) - 1)
#define REG_SET_BITS(v, s, w, d) \
    (((v) & ~(BIT_MASK(w) << (s))) | (((d) & BIT_MASK(w)) << (s)))
#define REG_GET_BITS(v, s, w) \
    (((v) >> (s)) & BIT_MASK(w))
```

```c
// pspl_hal.c — 平台需实现 hal_read/hal_write/hal_delay

#include "pspl_hal.h"

void PsPl_Init(void) {
    hal_init();
}

void PsPl_TriggeredWrite(uint32_t data_reg, uint32_t data, uint8_t trig_bit) {
    hal_write(data_reg, data);
    uint32_t s = hal_read(REG_STATUS);
    hal_write(REG_STATUS, s | BIT(trig_bit));
    do { s = hal_read(REG_STATUS); } while (s & BIT(trig_bit));
}

uint32_t PsPl_TriggeredRead(uint8_t trig_bit, uint32_t result_reg) {
    uint32_t s = hal_read(REG_STATUS);
    hal_write(REG_STATUS, s | BIT(trig_bit));
    do { s = hal_read(REG_STATUS); } while (s & BIT(trig_bit));
    return hal_read(result_reg);
}

void PsPl_BulkWrite(uint32_t base, uint32_t *data, int n, uint8_t trig_bit) {
    for (int i = 0; i < n; i++) hal_write(base + i, data[i]);
    uint32_t s = hal_read(REG_STATUS);
    hal_write(REG_STATUS, s | BIT(trig_bit));
    do { s = hal_read(REG_STATUS); } while (s & BIT(trig_bit));
}

uint32_t PsPl_ReadSetBits(uint32_t reg, uint8_t shift, uint8_t width) {
    return REG_GET_BITS(hal_read(reg), shift, width);
}

void PsPl_WriteSetBits(uint32_t reg, uint8_t shift, uint8_t width, uint32_t data) {
    uint32_t v = hal_read(reg);
    hal_write(reg, REG_SET_BITS(v, shift, width, data));
}
```

---

## 7. 跨平台 HAL 抽象

### 7.1 接口定义

```c
typedef struct {
    void      (*init)(void);
    void      (*write)(uint32_t addr, uint32_t data);
    uint32_t  (*read)(uint32_t addr);
    void      (*delay_ms)(uint32_t ms);
    void      (*log)(const char *fmt, ...);
} PsPl_Hal_t;
```

### 7.2 各平台实现

| 平台 | 物理层 | write/read 实现 | init 实现 |
|------|--------|------------------|-----------|
| **Zynq** | AXI BRAM | `XBram_WriteReg(base, addr*4, data)` | `XBram_LookupConfig → CfgInitialize → SelfTest` |
| **Zynq** | AXI GPIO | `XGpio_WriteReg(base, offset, data)` | `XGpio_Initialize` |
| **STM32 + FPGA** | FMC | `*(__IO uint32_t*)(FMC_BANK1 + addr*4) = data` | `MX_FMC_Init()` (CubeMX 生成) |
| **ESP32 + FPGA** | SPI | `spi_device_transmit(handle, &t)` | `spi_bus_add_device` |
| **通用 MCU + FPGA** | 并口 | `*(volatile uint32_t*)(CS_BASE + offset) = data` | GPIO 初始化 + 时序配置 |

示例：STM32 FMC 映射模式

```c
// STM32F746 + FPGA via FMC (16-bit multiplexed)
#define FMC_PSPL_BASE   ((uint32_t)0x60000000)

static void stm32_write(uint32_t addr, uint32_t data) {
    *(volatile uint16_t*)(FMC_PSPL_BASE + addr * 4)     = (uint16_t)(data);
    *(volatile uint16_t*)(FMC_PSPL_BASE + addr * 4 + 2) = (uint16_t)(data >> 16);
}

static uint32_t stm32_read(uint32_t addr) {
    uint16_t lo = *(volatile uint16_t*)(FMC_PSPL_BASE + addr * 4);
    uint16_t hi = *(volatile uint16_t*)(FMC_PSPL_BASE + addr * 4 + 2);
    return ((uint32_t)hi << 16) | lo;
}

const PsPl_Hal_t Hal_STM32 = {
    .init = MX_FMC_Init,
    .write = stm32_write,
    .read = stm32_read,
    .delay_ms = HAL_Delay,
    .log = printf,
};
```

### 7.3 使用 HAL 指针的上层代码

```c
// 上层代码完全与平台解耦
static const PsPl_Hal_t *HAL = NULL;

void App_SetHal(const PsPl_Hal_t *hal) { HAL = hal; }

void App_DoSomething(void) {
    HAL->init();
    PsPl_TriggeredWrite(REG_CFG_A, 0x1234, STATUS_TRIG_A);
    uint32_t result = PsPl_TriggeredRead(STATUS_TRIG_SWEEP, REG_RESULT);
    HAL->log("result = %lu", result);
}
```

---

## 8. 与上层模块的协作

```
┌──────────────────────────────────────────┐
│  dds.c / fir.c / key.c                   │
│  (使用 BRAM_Write/Read 构建上层协议)       │
├──────────────────────────────────────────┤
│  bram.h    BRAM_Init / BRAM_Write / Read  │
│  bram.c    (单例 + 字地址转换)             │
├──────────────────────────────────────────┤
│  Xilinx XBram Driver (Vendor HAL)         │
├──────────────────────────────────────────┤
│  AXI BRAM Controller → PL bram_2_rtl_ctrl│
└──────────────────────────────────────────┘
```

- **bram.h 是系统的"唯一硬件写入点"**
- 上层模块只调用 BRAM_Write/Read，不直接操作 XBram API
- 如果有多个 PS-PL 通信通道（BRAM + GPIO + SPI），各自封装 HAL，上层通过 `PsPl_Hal_t` 函数指针选择

---

## 9. 调试建议

### 读回验证

```c
void BRAM_Write(uint32_t addr, uint32_t data) {
    XBram_WriteReg(base, addr * 4, data);
    // DEBUG: 读回校验
    // uint32_t rb = XBram_ReadReg(base, addr * 4);
    // if (rb != data) log_error("write-back mismatch at addr %lu", addr);
}
```

### 常见问题排查表

| 症状 | 原因 | 对策 |
|------|------|------|
| 触发位永不归零 | PL 未收到数据或状态机异常 | 检查地址映射、增加稳定延时、用 ILA 查看 PL 内部信号 |
| 位域写入无效 | 未 Read-Modify-Write 直接覆盖 | 确认先读再写，而非直接 `hal_write(REG, new_val)` |
| 批量数据错位 | 字地址 vs 字节地址混淆 | 检查 HAL 层是否做了 *4 转换，PL 期望哪种地址 |
| 读回始终为 0 | 未初始化、地址错误或 PL 未驱动总线 | 确认 Init 成功、地址在有效范围内 |
| 偶发通信失败 | 触发竞争、PL 还在处理上次请求 | 增加延时、在触发前确认所有触发位已归零 |

---

## 10. 与 fir-filter-adaptation skill 的关系

| 方面 | bram-hal skill | fir-filter-adaptation skill |
|------|---------------|----------------------------|
| 层次 | 最底层硬件抽象 | 中层应用协议 |
| 职责 | 单字读写、触发/应答、位域操作 | 扫频循环、分类、系数生成 |
| 依赖 | 无下层依赖 | 深度依赖 BRAM HAL |
| 跨平台度 | 全部与硬件耦合 | 算法部分通用，通信部分依赖 BRAM HAL |

建议先掌握 bram-hal 理解硬件通信机制，再学习 fir-filter-adaptation 构建上层流水线。

