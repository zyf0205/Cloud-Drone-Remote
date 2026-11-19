# Cloud-Drone-Remote 遥控器

基于 **STM32F103C8T6 + FreeRTOS V9.0.0** 的四轴飞行器**遥控器端（发送端）**，通过 NRF24L01+ 2.4GHz 单向无线链路与 [Cloud-Drone](../Cloud-Drone)（飞控端）对接。

## FreeRTOS 多任务架构

项目采用「**4 业务任务 + 1 监控守护任务**」的解耦设计，严格隔离 100Hz 硬实时无线发包链路与慢速外设（EEPROM 写入、蜂鸣器声型），并遵循**单一硬件所有权原则**（每个外设由唯一任务独占操作）：

```text
  Task_RC_Tx (Prio 4, 10ms) ──► ADC DMA 快照 → 映射/滤波/钳位 → 32B 组帧 → NRF24L01+ 发送
         │                               ▲
         │ 心跳位(TaskNotify)            │ 临界区快照(AUX/Trim)
         ▼                               │
  Task_Monitor (Prio 2, 500ms) ◄── Task_Input (Prio 3, 20ms) ──► 8 路单键去抖边沿状态机
         │                           │               │
         ▼                           │ Overwrite     │ QueueSend
     IWDG 喂狗                       ▼               ▼
     (2.56s 超时)           Task_Storage (Prio 2)   Task_Buzzer (Prio 1)
                            异步写 AT24C02 + 校验   按键音 / 限幅音 / 故障报警
```

- **零发包抖动**：EEPROM 页写周期（10ms）在低优先级的 `Task_Storage` 中通过 `vTaskDelay()` 异步完成，彻底不再阻塞 100Hz 无线发送；连续快按微调键由长度 1 的覆写队列自动合并，延长 EEPROM 擦写寿命。
- **低功耗待机**：`Task_Storage`、`Task_Buzzer` 空闲时阻塞挂起，空闲任务经 Idle Hook 执行 `__WFI()` 睡眠，电池供电空耗极低。
- **多任务看门狗**：`Task_Monitor` 汇总 `Task_RC_Tx` 与 `Task_Input` 的 Task Notification 心跳位，全部存活才喂 IWDG；任一核心任务挂死均会在 ~2.56s 内复位恢复（快于飞控端 5s 失联门限）。

---

## 硬件与外设速查（详见 `docs/01_硬件平台说明.md`）

- **主控与时钟**：STM32F103C8T6，**内部 HSI PLL 48MHz**（无外部晶振，`8MHz / 2 × 12`）；SysTick 归 FreeRTOS（1kHz），TIM3 提供独立 1kHz 硬件毫秒时钟（`TIM3CLK = 48MHz，PSC = 47，ARR = 999`）。
- **4 路摇杆**：ADC1 + DMA1_CH1 后台循环采集（`PA0=Yaw`，`PA1=THR`，`PA2=Pitch`，`PA3=Roll`）。
- **NRF24L01+**：软件 SPI（`PB13/PB14/PB15`），控制脚 `PB12=CSN` / `PA11=CE` / `PA8=IRQ`；1Mbps / 0dBm / `RF_CH=1`，单向无 ACK，带 5ms IRQ 软件超时与连续超时自愈。
- **AT24C02 EEPROM**：软件 I2C（`PB6=SCL`，`PB7=SDA`），保存 Roll（地址 0）与 Pitch（地址 4）微调值，上电加载带合法范围及步进校验（自动拦下新片 `0xFFFF`）。
- **蜂鸣器**：`PA12` 推挽输出（高电平响、低电平停），由 `Task_Buzzer` 统一独占控制。
- **8 路按键**：输入上拉（按下为低），`AUX1~4`（`PA15/PB11/PC14/PC15`）、左右前后微调（`PA6/PA7/PB0/PB1`）。

---

## 按键操作与声光反馈

全部 8 个按键均采用 **20ms 去抖 + 单键按下边沿触发 + 全键松手解锁防连发** 机制：

| 按键 | 引脚 | 功能 | 蜂鸣器反馈 |
| :--- | :---: | :--- | :--- |
| **AUX1** | PA15 | 飞控定高开关（1000 ↔ 2000） | 单短鸣 40ms |
| **AUX2** | PB11 | 飞控光流定点开关（需先开定高） | 单短鸣 40ms |
| **AUX3** | PC14 | 飞控陀螺校准触发（未解锁 + 最低油门） | 单短鸣 40ms |
| **AUX4** | PC15 | 预留辅助开关（1000 ↔ 2000） | 单短鸣 40ms |
| **左 / 右微调** | PA6 / PA7 | Roll 中位 -5 / +5（限幅 ±250），异步落盘 EEPROM | 正常单短鸣 / 触顶双短促 |
| **前 / 后微调** | PB0 / PB1 | Pitch 中位 -5 / +5（限幅 ±250），异步落盘 EEPROM | 正常单短鸣 / 触顶双短促 |

**系统状态声型**：
- **上电正常**：短鸣 200ms。
- **NRF 故障（禁止起飞）**：每秒 1 长鸣（400ms 响 / 600ms 停），同时停止无线发送。
- **EEPROM 故障（仍可飞行）**：每秒 2 短鸣（100ms 响 / 100ms 停 / 100ms 响 / 700ms 停），微调归零且禁用写入。
- **栈溢出 / 堆分配失败**：立即关中断并蜂鸣器长鸣告警。

---

## 无线帧格式（与飞控端严格兼容）

```text
字节偏移:  0    1    2    3    4..19              20..30    31
内容:     0xAA 0xAF 0x03 0x1B  8 通道大端(2B/个)   固定 0x00  前 31 字节求和低 8 位
通道顺序:  roll, pitch, thr, yaw, AUX1, AUX2, AUX3, AUX4（量程 1000~2000）
```

100Hz（10ms/帧）单向发送；油门通道保留 `RC_THR_SCALE = 0.20f`（配合飞控端定高死区上限 1750，切勿改为 `0.25f`）。完整协议与联调清单见 `docs/02_遥控协议说明.md`。

---

## 目录结构

```text
Cloud-Drone-Remote/
├── src/
│   ├── app/       FreeRTOS 任务层（app_tasks, task_rc, task_input, task_storage, task_buzzer, task_monitor, hooks）
│   ├── control/   遥控业务层（rc_protocol 纯算法与组帧，remote 线程安全状态中心）
│   ├── bsp/       板级器件驱动（nrf24l01, eeprom, key, buzzer）
│   ├── driver/    底层外设驱动（adc DMA, i2c, spi, tim）
│   ├── core/      启动入口（main, init, delay, sys）
│   ├── include/   全局共享类型与汇总头（all_data.h, all_define.h）
│   ├── FreeRTOS/  FreeRTOS V9.0.0 内核与 FreeRTOSConfig.h
│   ├── Lib/       STM32F10x 标准外设库
│   └── StartUp/   启动文件与 CMSIS
├── docs/
│   ├── 01_硬件平台说明.md
│   ├── 02_遥控协议说明.md
│   └── 03_FreeRTOS架构与代码导读.md
├── remote.uvprojx
└── .eide/eide.yml
```

---

## 编译与烧录

WSL2 / Windows 下调用 Keil MDK-ARM AC5 构建：

```bash
/mnt/d/Keil_v5/UV4/UV4.exe -j0 -b "D:\_study\projects\_MyProject\Drone\Cloud-Drone-Remote\remote.uvprojx" -o "D:\_study\projects\_MyProject\Drone\Cloud-Drone-Remote\build\logs\build.log"
```

- 成功标志：`build/logs/build.log` 末尾为 `0 Error(s), 0 Warning(s)`。
- 烧录产物：`build/keil/remote.hex`（支持 J-Link / SWD 下载，`.eide/eide.yml` 亦已同步）。
- 代码风格：严格遵循 `.clang-format`（4 空格、无 Tab、Allman 大括号）。

---

## 文档导航

- [`docs/01_硬件平台说明.md`](docs/01_硬件平台说明.md) — MCU 时钟树、完整引脚表、外设配置与中断优先级规划
- [`docs/02_遥控协议说明.md`](docs/02_遥控协议说明.md) — NRF24L01+ 链路参数、32 字节帧格式、油门定高约束与联调核对清单
- [`docs/03_FreeRTOS架构与代码导读.md`](docs/03_FreeRTOS架构与代码导读.md) — 5 任务优先级设计、任务间通信、单一所有权、看门狗监控与二次开发扩展指南
