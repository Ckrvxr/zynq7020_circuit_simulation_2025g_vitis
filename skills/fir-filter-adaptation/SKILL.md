---
name: fir-filter-adaptation
description: 为嵌入式系统编写自动扫频分析、电路类型识别、FIR 滤波器系数生成与硬件加载的通用指南
---

## 架构总览

### 整体流水线

```
Calibrate() ──┐
               ├──> Sweep(freq_range) ──> AnalyzeResponse() ──> CalcCoeffs() ──> LoadToHW()
Learn() ──────┘
```

### 典型调用流程

```
1. FIR_Calibrate()    → 扫频测量环境基准（如线缆、探棒影响）
2. FIR_Learn()        → 扫频测量待测电路 → 自动分类电路类型 → 准备数据
3. FIR_Run()          → IFFT 生成 FIR 系数 → 量化 → 加载到硬件滤波器
```

### 文件职责

| 文件 | 职责 |
|------|------|
| `fir.h` | 公开 API 声明、状态变量 `extern`、电路类型枚举、常数定义 |
| `fir.c` | 扫频、分类、系数生成全流程实现 |
| `bram.h/c` | 硬件通信层（读写寄存器 + 触发/应答握手） |
| `dds.h/c` | 激励源控制（设置频率/幅度） |
| `kiss_fft.h/c` | FFT/IFFT 计算库 |

### 核心数据流

```
扫频采集 (每频点)
  → 复数 I/Q (int16)  → 幅度平方 mag2 (uint32)
  → 校准模式: 存入 fir_ref_mag2[]
  → 学习模式: 存入 fir_i_data[], fir_r_data[]

分类阶段
  → 三频段统计 → 百分比阈值 → FIR_CircuitType_e

系数生成
  → 复数频响 → 共轭对称填充 → IFFT → 窗函数 → Q15 量化 → 加载
```

---

## 1. 扫频基础设施

### 频率范围与步进

```c
// 线性扫频：50Hz ~ 500kHz，1040 个频点
uint32_t freq = 50 + (uint32_t)((uint64_t)i * 499950 / 1039);
```

- 起点 50Hz，终点 500kHz，步进约 481Hz
- `uint64_t` 中间计算防止 32 位溢出
- 频点数和范围根据需求可配置

### 触发/应答握手协议

扫频循环中，每个频点都遵循以下硬件通信模式：

```
1. 写频率控制字到寄存器 A
2. 置位寄存器 C 的 bit X 和 bit Y → PL 检测到后更新 DDS 配置
3. 轮询寄存器 C，等待 PL 清除 bit X 和 bit Y
4. vTaskDelay(10ms) 等待信号稳定
5. 置位寄存器 C 的 bit Z → PL 启动 ADC 采集
6. 轮询寄存器 C，等待 PL 清除 bit Z
7. 从寄存器 B 读取 I/Q 结果（32-bit 打包：高16位=I，低16位=Q）
```

伪代码：

```c
for each freq in sweep_range:
    // 发送频率
    write_reg(REG_FREQ, ftw)
    trigger = read_reg(REG_STATUS)
    trigger |= FREQ_TRIGGER_MASK | GAIN_TRIGGER_MASK
    write_reg(REG_STATUS, trigger)
    do {
        delay(1ms)
        status = read_reg(REG_STATUS)
    } while (status & (FREQ_TRIGGER_MASK | GAIN_TRIGGER_MASK))
    delay(10ms)   // 稳定延时

    // 触发测量
    trigger = read_reg(REG_STATUS)
    trigger |= MEASURE_TRIGGER_MASK
    write_reg(REG_STATUS, trigger)
    do {
        delay(1ms)
        status = read_reg(REG_STATUS)
    } while (status & MEASURE_TRIGGER_MASK)
    delay(10ms)

    // 读取结果
    result = read_reg(REG_RESULT)
    i = (int16_t)(result >> 16)
    r = (int16_t)(result & 0xFFFF)
```

### 复数 I/Q 数据处理

- 每个频点返回 16 位实部（I）和 16 位虚部（Q）
- 幅度平方：`mag2 = (uint32_t)(i*i) + (uint32_t)(r*r)`
- 转换为 uint32_t 避免 int16 溢出

### 平台相关 vs 通用部分

| 部分 | 性质 |
|------|------|
| 扫频循环逻辑 | **通用** |
| 频率步进公式 | **通用** |
| 触发/应答协议 | 平台相关（此处用 BRAM 寄存器，可用 SPI/UART/I2C 替代） |
| 寄存器地址和 bit 定义 | 平台相关 |
| 稳定延时 | **通用**（取决于具体硬件响应时间） |

---

## 2. 两阶段测量模式

### 校准阶段

```c
void FIR_Calibrate(void) {
    fir_is_calibrating = 1;   // Sweep 中按此标志存储 ref_mag2
    FIR_Sweep();
    fir_calibrated = 1;
    fir_is_calibrating = 0;
}
```

- 测量空载或直通路径的幅度响应
- 存储到 `fir_ref_mag2[]`（uint32 数组）
- 用作后续学习阶段的 Baseline

### 学习阶段

```c
void FIR_Learn(void) {
    fir_is_calibrating = 0;   // Sweep 中按此标志存储 i/r 数据
    FIR_Sweep();
    fir_circuit_type = FIR_AnalyzeResponse();  // 自动分类
}
```

- 测量待测电路的 I/Q 响应
- 存储到 `fir_i_data[]` / `fir_r_data[]`

### 共享扫频函数

关键设计：`FIR_Sweep()` 通过 `fir_is_calibrating` 标志区分存储目标

```c
static void FIR_Sweep(void) {
    for (int i = 0; i < N_POINTS; i++) {
        // ... 测量每个频点得到 i_val, r_val, mag2 ...

        if (fir_is_calibrating) {
            ref_mag2[i] = mag2;      // 存基准
        } else {
            i_data[i] = i_val;       // 存测量结果
            r_data[i] = r_val;
        }
    }
}
```

---

## 3. 电路类型分类

### 频段划分

```
低频段: 索引  0 - 346  (占 1/3)
中频段: 索引 347 - 693  (占 1/3)
高频段: 索引 694 - 1039 (占 1/3)
```

### 有校准模式

```c
// 以基准为参考，判定该频点是否"通"
uint8_t above = (mag2 * 100 / ref >= 50) ? 1 : 0;
```

- `mag2 * 100 / ref >= 50` 表示当前幅值不低于基准的 50%

### 无校准模式

```c
// 以全局最大值的 45% 为阈值
uint32_t threshold = max_mag2 * 45 / 100;
uint8_t above = (mag2 >= threshold) ? 1 : 0;
```

### 分类规则

统计三个频段中"通"的百分比：

```c
low_pct  = low_cnt  * 100 / 347;
mid_pct  = mid_cnt  * 100 / 347;
high_pct = high_cnt * 100 / 346;
```

应用阈值判定：

| low_pct | mid_pct | high_pct | 结论 |
|---------|---------|----------|------|
| > 60% | — | < 40% | LOW_PASS |
| < 40% | — | > 60% | HIGH_PASS |
| > 60% | < 40% | > 60% | BAND_STOP |
| < 40% | > 60% | < 40% | BAND_PASS |
| > 60% | > 60% | > 60% | ALL_PASS |
| < 40% | < 40% | < 40% | ALL_STOP |
| 其他 | 其他 | 其他 | OTHER |

> 阈值可根据实际电路特性调节。这里使用 >60% / <40% 作为判据，留有 40%-60% 的模糊区间避免误判。

### 频段数与阈值可配置

```c
// 可定制参数
#define N_BANDS     3                // 频段数
#define N_POINTS    1040             // 总采样点数
#define PCT_THRESH_HIGH  60          // 判定为"通"的百分比上限
#define PCT_THRESH_LOW   40          // 判定为"断"的百分比下限
#define MAG_THRESH_PCT   50          // 有校准模式下相对基准的百分比阈值
#define MAG_THRESH_PCT_NC 45         // 无校准模式下相对最大值的百分比阈值
```

---

## 4. IFFT 生成 FIR 系数

### 基本流程

```
测量频响 H[k] (k=0..N-1, 复数)
  → 构建共轭对称的完整频谱
  → IFFT → 冲激响应 h[n]
  → 加窗 → 截断 → 量化
```

### 共轭对称填充

实际测量只覆盖了正频率部分（index 0 到 N_POINTS-1）。进行 IFFT 前需要构造完整频谱：

```c
// N_POINTS = 1040, FFT_SIZE = 4096

// 1. 测量数据填入前 N_POINTS 个点
for (k = 0; k < N_POINTS; k++) {
    freq[k].r = i_data[k];
    freq[k].i = r_data[k];
}

// 2. 中间补零
for (k = N_POINTS; k < FFT_SIZE - N_POINTS + 1; k++) {
    freq[k].r = 0;
    freq[k].i = 0;
}

// 3. 共轭对称：freq[FFT_SIZE - k] = conj(freq[k])
for (k = 1; k < N_POINTS; k++) {
    freq[FFT_SIZE - k].r =  freq[k].r;
    freq[FFT_SIZE - k].i = -freq[k].i;
}
```

- 索引 0 是直流分量（DC），保持原值
- 共轭对称保证 IFFT 输出为实数冲激响应

### IFFT 计算

```c
// 使用 KISS FFT（通用纯 C FFT 库）
kiss_fft_cfg cfg = kiss_fft_alloc(FFT_SIZE, 1 /* inverse */, NULL, NULL);
kiss_fft(cfg, freq, time);   // freq → time 域
kiss_fft_free(cfg);

float scale = 1.0f / FFT_SIZE;   // IFFT 归一化因子
```

- `kiss_fft_alloc(FFT_SIZE, 1, ...)` 第二参数 1 = inverse FFT
- IFFT 输出需要除以 `FFT_SIZE` 以归一化
- 其他 FFT 库（CMSIS-DSP, ARM DSP, ESP-DSP）可替换此步骤

### 系数截断

关键设计：FIR 抽头数 = 2080，为 FFT 大小的一半

```c
#define FIR_TAPS   2080   // 滤波器阶数
#define FFT_SIZE   4096   // FFT 点数
```

从 IFFT 结果中取前 `FIR_TAPS` 个点作为滤波器系数（保留主瓣，舍弃远端旁瓣）。

---

## 5. 窗函数与定点量化

### Hamming 窗

```c
// Hamming window: w[n] = α - β*cos(2πn/(N-1))
// 标准系数: α = 0.54, β = 0.46
float win = 0.54f - 0.46f * cosf(2.0f * (float)M_PI * n / (FIR_TAPS - 1));
```

Hamming 窗压低旁瓣到 -43dB，主瓣宽度为 `8π/N`。也可以替换为其他窗函数：

| 窗函数 | 旁瓣抑制 | 主瓣宽度 | 适用场景 |
|--------|----------|----------|----------|
| Hamming | -43dB | 8π/N | **通用（推荐）** |
| Hann | -31dB | 8π/N | 频率分辨精度要求不高 |
| Blackman | -58dB | 12π/N | 需要强旁瓣抑制 |
| Kaiser | 可配置 | 可配置 | 需要灵活权衡 |

### 应用窗函数与归一化

```c
for (n = 0; n < FIR_TAPS; n++) {
    float v = time[n].r * scale;      // IFFT 结果 * 归一化因子
    v *= win;                         // 加窗
    // 钳位防溢出
    if (v >  1.0f) v =  1.0f;
    if (v < -1.0f) v = -1.0f;
    coeffs[n] = (int16_t)(v * 32767.0f);  // Q15 量化
}
```

### Q15 定点量化

- float 范围 [-1.0, 1.0] → int16 范围 [-32768, 32767]
- 量化公式：`q15 = (int16_t)(float_val * 32767.0f)`
- 钳位保护：量化前确保 float 在 [-1.0, 1.0] 区间内
- 使用 `int16_t` 存储，节省存储和计算资源

### 系数打包与加载到硬件

```c
// 系数打包：两个 int16 合并为一个 uint32
// 对应硬件期望的 32-bit 字宽
for (int i = 0; i < N_WORDS; i++) {
    uint32_t word = ((uint16_t)coeffs[i * 2 + 1] << 16) |
                     (uint16_t)coeffs[i * 2];
    write_reg(REG_COEFF_BASE + i, word);
}

// 触发硬件重载滤波器
trigger = read_reg(REG_STATUS);
trigger |= FIR_ENABLE_MASK;
write_reg(REG_STATUS, trigger);
```

---

## 6. 状态变量共享与任务交互

### 关键状态变量

| 变量 | 类型 | 读/写方 | 用途 |
|------|------|---------|------|
| `fir_i_data[]` | `volatile int16_t` | fir.c 写, display.c 读 | 扫频实部 |
| `fir_r_data[]` | `volatile int16_t` | fir.c 写, display.c 读 | 扫频虚部 |
| `fir_ref_mag2[]` | `volatile uint32_t` | fir.c 写 | 校准基准幅度平方 |
| `fir_progress` | `volatile int` | fir.c 写, display.c 读 | 当前扫频进度 (0..1040) |
| `fir_curr_freq` | `volatile uint32_t` | fir.c 写, display.c 读 | 当前频点频率值 |
| `fir_curr_i/r` | `volatile int16_t` | fir.c 写, display.c 读 | 当前频点 I/Q |
| `fir_calibrated` | `volatile uint8_t` | fir.c 读写 | 是否已完成校准 |
| `fir_is_calibrating` | `volatile uint8_t` | fir.c 读写 | 当前是否在校准模式 |
| `fir_circuit_type` | `volatile FIR_CircuitType_e` | fir.c 写, display.c 读 | 分类结果 |

### 与显示和按键的配合

```
Key Confirm 触发:
  → currentState = STATE_FIR_MODE_LEARNING
  → FIR_Learn()  [阻塞执行]
  → currentState = STATE_FIR_MODE_LEARN_COMPLETE

Display 50Hz 轮询:
  → fir_progress 更新进度条
  → fir_curr_freq 更新频率值
  → fir_curr_i/r 更新 I/Q 数值
```

> 注意：`FIR_Learn()` 是阻塞式 API，扫频期间占满当前任务。如果运行在低优先级任务中，高优先级任务仍可抢占。确保在按键 handler 或独立任务中调用时不阻塞关键任务。

---

## 7. 流水线整合示意

### 完整调用序列

```c
// === 步骤 1：校准 ===
FIR_Calibrate();
// → 扫频 1040 点，存储基准幅度到 fir_ref_mag2[]
// → 耗时约 1040 * 30ms ≈ 31s

// === 步骤 2：学习 ===
FIR_Learn();
// → 扫频 1040 点，存储 I/Q 到 fir_i_data[] / fir_r_data[]
// → 自动分类，结果存 fir_circuit_type
// → 耗时约 1040 * 30ms ≈ 31s

// === 步骤 3：生成并加载系数 ===
FIR_Run();
// → IFFT (4096 点) + 加窗 + Q15 量化
// → 写入硬件寄存器
// → 触发 FIR 使能
// → 耗时约 < 100ms（取决于 CPU 和 FFT 库）
```

### 一般化 API 设计

```c
// 通用化接口（平台无关）
typedef struct {
    uint32_t freq_start_hz;
    uint32_t freq_end_hz;
    uint32_t num_points;
    uint32_t settling_ms;
} SweepConfig_t;

typedef struct {
    SweepConfig_t sweep;
    uint32_t      fft_size;
    uint32_t      fir_taps;
    float         window_beta;     // 窗函数参数
} FilterDesignConfig_t;

// 硬件通信回调（平台需实现）
typedef struct {
    void (*write_reg)(uint32_t addr, uint32_t data);
    uint32_t (*read_reg)(uint32_t addr);
    void (*delay_ms)(uint32_t ms);
} HalInterface_t;

// 流水线 API
void FIR_Sweep(HalInterface_t *hal, SweepConfig_t *cfg,
               int16_t *i_data, int16_t *r_data,
               volatile int *progress);
void FIR_Classify(int16_t *i_data, int16_t *r_data,
                  uint32_t *ref_mag2, uint8_t calibrated,
                  int num_points, int *circuit_type);
void FIR_DesignCoeffs(int16_t *i_data, int16_t *r_data,
                      int16_t *coeffs,
                      FilterDesignConfig_t *cfg);
void FIR_LoadCoeffs(HalInterface_t *hal, int16_t *coeffs,
                    uint32_t base_addr, int num_words);
```

---

## 8. 跨平台适配要点

| 模块 | 平台相关部分 | 通用部分 |
|------|-------------|----------|
| 扫频 | 硬件寄存器读写、触发/应答位定义、延时实现 | 频率步进逻辑、I/Q 提取、mag2 计算 |
| 分类 | 无 | 纯算法，完全可移植 |
| IFFT | FFT 库接口（KISS FFT / CMSIS-DSP / ESP-DSP） | 共轭对称填充、频响构建 |
| 加窗量化 | 无 | 纯算法，完全可移植 |
| 系数加载 | 硬件寄存器写入、触发 | 系数打包逻辑 |

### FFT 库替换建议

| 平台 | 推荐 FFT 库 |
|------|-------------|
| Zynq (ARM Cortex-A9) | KISS FFT（已用），也可用 Ne10 |
| STM32F4/F7/H7 | CMSIS-DSP `arm_rfft_fast_f32`（硬件 FPU 加速） |
| ESP32 | ESP-DSP `dsps_fft2r_fc32`（Xtensa 优化） |
| 通用 | KISS FFT（纯 C，无依赖） |

### 硬件通信层提取

当前实现假设通过 BRAM 寄存器进行触发/应答通信。通用化建议：

- 将 `write_reg()` / `read_reg()` / `delay_ms()` 提取为回调函数
- 触发/应答的 bit 位置通过宏或配置结构体注入
- 支持 SPI、I2C、UART、共享内存等不同物理层

---

## 9. 调试与验证

### 打印关键中间值

```c
xil_printf("INFO[FIR]: Frequency sweep completed! Type: %s\n\r",
           type_str[circuit_type]);
```

关键检查点：
- 扫频完成后检查排除异常值（mag2 是否合理范围）
- 分类结果用手工计算验证
- IFFT 系数用已知滤波器验证（如全通 → 冲激响应应为单位脉冲）
- 系数加载后用示波器/频谱仪验证滤波效果

### 常见问题

| 问题 | 可能原因 | 解决方案 |
|------|----------|----------|
| 分类总是 OTHER | 阈值不合适或信号太弱 | 调节 PCT_THRESH_HIGH/LOW，检查 ADC 增益 |
| IFFT 系数不收敛 | 共轭对称填充错误 | 验证 freq[FFT_SIZE-k] = conj(freq[k]) |
| 滤波后信号失真 | Q15 量化溢出 | 检查钳位逻辑，确保 |float| ≤ 1.0 |
| 扫频结果全零 | 触发/应答死锁 | 检查延时是否足够，握手位定义是否匹配 |

