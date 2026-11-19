# Cloud-Drone-Remote 遥控器

STM32F103C8T6 + FreeRTOS V9.0.0 的四轴遥控器端（发送端）。100 Hz 组帧，通过 NRF24L01+ 单向链路发给飞控。

配套飞控：[Cloud-Drone](../Cloud-Drone)

## 运行架构

```text
ADC1 + DMA1_CH1 → 4 路摇杆后台循环刷新

Task_RC_Tx（10 ms，Prio 4）
  读 ADC 快照 → 映射 / 低通 / 钳位 → 组 32 B 帧 → NRF24L01+ 发送
  上报心跳位

Task_Input（20 ms，Prio 3）
  扫描 8 路按键 → 翻转 AUX / 调整微调 → 覆盖写存储队列 / 投递提示音
  上报心跳位

Task_Monitor（500 ms，Prio 2）→ 汇总两个心跳位，齐全才喂 IWDG
Task_Storage（Prio 2）→ 队列阻塞，醒来异步写 AT24C02 并回读校验
Task_Buzzer（Prio 1）→ 队列阻塞，按键音与故障报警统一输出
```

## 任务

| 任务 | 优先级 | 栈（字） | 触发 | 独占资源 |
|---|---|---|---|---|
| `Task_RC_Tx` | 4 | 160 | `vTaskDelayUntil` 10 ms | NRF24L01+ |
| `Task_Input` | 3 | 128 | `vTaskDelayUntil` 20 ms | 8 路按键 GPIO |
| `Task_Storage` | 2 | 128 | 队列阻塞 | 软件 I2C / AT24C02 |
| `Task_Monitor` | 2 | 128 | `vTaskDelayUntil` 500 ms | IWDG 喂狗 |
| `Task_Buzzer` | 1 | 128 | 队列阻塞 | 蜂鸣器 PA12 |
| `IdleTask` | 0 | 128 | 空闲 | 进入 `__WFI()` 休眠 |

单一硬件所有权，全工程无互斥锁。三种 IPC：任务通知（心跳位）、覆盖队列（微调快照）、事件队列（提示音）。

## 按键

全部按键均为 20 ms 扫描 + 二拍去抖 + 松手解锁防连发。

| 按键 | 引脚 | 功能 | 蜂鸣器 |
|---|---|---|---|
| AUX1 | `PA15` | 飞行模式循环：手动 → 定高 → 定点 | 单短鸣 |
| AUX2 | `PB11` | 预留，飞控端不响应 | 单短鸣 |
| AUX3 | `PC14` | 触发飞控 MPU 校准 | 单短鸣 |
| AUX4 | `PC15` | 预留，飞控端不响应 | 单短鸣 |
| 左 / 右 | `PA6` / `PA7` | Roll 中位 −5 / +5 | 单短鸣 / 触顶双短促 |
| 前 / 后 | `PB0` / `PB1` | Pitch 中位 −5 / +5 | 单短鸣 / 触顶双短促 |

微调范围 ±250，步进 5，异步写入 AT24C02，掉电保存。

## 声光反馈

| 场景 | 声型 |
|---|---|
| 上电正常 | 短鸣 200 ms |
| 按键确认 | 短鸣 40 ms |
| 微调到限位 | 双短促 30 + 40 + 30 ms |
| NRF 故障（禁止起飞） | 每秒长鸣（响 400 ms / 停 600 ms），同时停止发送 |
| EEPROM 故障（仍可飞行） | 每秒两短鸣（100 / 100 / 100 / 700 ms），微调归零并禁止写入 |
| 栈溢出 / 堆耗尽 | 立即关中断长鸣 |

## 无线帧

| 偏移 | 内容 |
|---|---|
| 0 / 1 | `0xAA` / `0xAF` |
| 2 / 3 | `0x03` / `0x1B` |
| 4~19 | 8 通道大端 `uint16_t`：roll、pitch、thr、yaw、AUX1~4，量程 1000~2000 |
| 20~30 | 保留，全 `0x00` |
| 31 | 前 31 字节累加和的低 8 位 |

发射端 100 Hz 单向广播，无自动应答。完整参数见 [docs/01_硬件与引脚.md](docs/01_硬件与引脚.md)。

## 编译

Keil MDK-ARM AC5（C89 模式），工程文件 `remote.uvprojx`。

```bash
UV4.exe -j0 -b "remote.uvprojx" -o "build\logs\build.log"
```

- `-b` 增量编译，`-r` 全量重编，`-j0` 不弹 IDE 窗口
- 成功标志：日志末尾 `0 Error(s), 0 Warning(s)`
- 产物：`build/keil/remote.hex`
- 当前占用：ROM 16.4 KB / RAM 10.1 KB（含 8 KB FreeRTOS 堆）

## 调试观测

在 Keil / J-Link Watch 窗口加 `g_boot_fault` 与 `g_stack_hwm`：

| 变量 | 说明 |
|---|---|
| `g_boot_fault` | bit0 = NRF 故障，bit1 = EEPROM 故障 |
| `g_stack_hwm` | 5 个任务的剩余栈高水位（字），越接近 0 越危险 |
| `Remote` | 当前 8 通道实时值 |

## 文档

- [docs/01_硬件与引脚.md](docs/01_硬件与引脚.md) — 时钟树、引脚分配、外设参数、无线协议
- [docs/02_软件架构.md](docs/02_软件架构.md) — 分层、启动流程、任务与 IPC、按键状态机、自愈逻辑
