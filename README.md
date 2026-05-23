# zynq7020_circuit_simulation_2025g_vitis

Zynq-7020 平台电路仿真系统，基于 Xilinx Vitis 2023.1 开发。

## 硬件平台

- **芯片：** Xilinx Zynq-7020 (XC7Z020)
- **处理器：** 双核 Cortex-A9
  - CPU0 (Standalone) — 辅助唤醒，启动后休眠
  - CPU1 (FreeRTOS) — 主应用，运行全部业务逻辑
- **外设：**
  - OLED 128x64 (SSD1306/SSD1315) — I2C-0
  - 按键 ×4（上/下/确认/取消） — MIO 54–57
  - PL BRAM — PS-PL 寄存器通信接口
  - PL DDS IP 核 — 波形生成
  - PL FIR IP 核 — 实时滤波

## 项目结构

```
All_desing_wrapper/       硬件平台定义（bitstream、XSA、FSBL 源码）
core0/                    从核应用（Standalone，CPU0 唤醒代码）
main/                     主应用（FreeRTOS，CPU1）
main_system/              系统项目（生成 BOOT.bin）
refer.v                   PL 侧 BRAM 控制器 RTL 源码
refer.md                  BRAM 寄存器映射与驱动 API 参考
```

## 功能介绍

### 主应用 (CPU1 / FreeRTOS)

基于 FreeRTOS 实时系统，包含三个并发任务：
- **显示任务** — 50 Hz 刷新 OLED
- **按键扫描任务** — 50 ms 周期轮询 GPIO
- **主任务** — 保留逻辑

### OLED 显示与菜单系统

128x64 单色 OLED，基于 u8g2 图形库驱动，状态机管理的多级菜单：

- **主菜单** — DDS 模式 / FIR 模式
- **DDS 模式** — 编辑幅值（Vpp）和频率
- **FIR 模式** — 运行模型 / 开始学习 / 校准
- **实时频谱图** — 扫频完成后绘制频率响应曲线（dB vs 频率）
- **动态效果** — 动画光标、波形预览、进度条

### 按键交互

4 个物理按键，支持：
- **短按** — 菜单导航、参数编辑
- **长按加速** — 编辑频率时长按自动增大步进（100 Hz → 10 kHz）

### DDS 信号发生器

- **频率范围：** 0–1 MHz
- **幅值范围：** 0–7500 mV
- 通过 BRAM 寄存器向 PL DDS IP 核下发配置（FTW + DAC 增益）
- 内置 DAC 增益校准公式

### FIR 滤波器自动适配

对未知电路进行扫频分析，自动生成匹配的 FIR 滤波器系数：

1. **扫频** — 50 Hz–500 kHz，1040 个步进，采集 I/Q 数据
2. **校准** — 存储参考幅值
3. **学习** — 测定幅频响应
4. **识别** — 自动判断电路类型（低通/高通/带通/带阻/全通等）
5. **生成系数** — KISS FFT 逆变换 → 汉明窗 → Q15 量化，2080 阶
6. **加载运行** — 写入 PL FIR IP 核，实时滤波

### BRAM 驱动

PS-PL 通信桥梁，封装 Xilinx XBram 驱动，提供 32 位寄存器读写。

### 从核 (CPU0)

仅负责通过 mailbox 唤醒 CPU1 主应用，之后进入 WFE 休眠。

## 参考文档

- `refer.md` — BRAM 寄存器映射与驱动 API 参考
- `refer.v` — PL 侧 BRAM 控制器 RTL 源码

## 构建

在 Vitis 中按以下顺序构建：

1. `All_desing_wrapper` — 硬件平台
2. `core0` — 从核应用
3. `main` — 主应用
4. `main_system` — 生成 BOOT.bin

将生成的 `BOOT.bin` 写入 SD 卡，Zynq 上电后自动启动。
