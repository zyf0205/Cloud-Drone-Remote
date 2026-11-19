# 03 FreeRTOS 架构与代码导读

> 本文档面向需要阅读、维护或二次开发 Cloud-Drone-Remote 的开发者，系统阐述分层架构、FreeRTOS 多任务设计依据、任务间通信机制、单一硬件所有权原则与扩展指南。

---

## 1. 四层模块化软件架构

项目按职责由底向上划分为四层，严格禁止下层反向依赖上层：

```text
┌─────────────────────────────────────────────────────────────────────────┐
│  App 应用任务层 (src/app/)                                              │
│  app_tasks · task_rc · task_input · task_storage · task_buzzer          │
│  task_monitor · freertos_hooks                                          │
│  职责：FreeRTOS 任务编排、周期调度、队列/通知通信、看门狗监督           │
├─────────────────────────────────────────────────────────────────────────┤
│  Control 控制业务层 (src/control/)                                      │
│  rc_protocol（纯 C 算法/组帧，零硬件依赖） · remote（线程安全状态中心） │
│  职责：摇杆映射滤波、微调校验、32B 协议打包、跨任务安全读写通道状态     │
├─────────────────────────────────────────────────────────────────────────┤
│  BSP 板级器件层 (src/bsp/)                                              │
│  nrf24l01 · eeprom · key · buzzer                                       │
│  职责：器件协议封装、按键 20ms 去抖边沿状态机、NRF 5ms 超时保护         │
├─────────────────────────────────────────────────────────────────────────┤
│  Driver & Core 底层驱动层 (src/driver/, src/core/)                      │
│  adc (DMA) · spi · i2c · tim (TIM3 1kHz) · main · init · delay · sys    │
│  职责：STM32 寄存器/标准库初始化、48MHz 时钟树、总线位操作              │
└─────────────────────────────────────────────────────────────────────────┘
```

**为什么单独拆分 `rc_protocol.c`？**
将所有纯数学逻辑（通道钳位、一阶低通、微调校验与限幅、32 字节大端组包与校验和）抽离为无 STM32/FreeRTOS 依赖的纯 C 模块，阅读协议规则一目了然，且可直接用主机端 `gcc` 编译运行离线单元测试。

---

## 2. FreeRTOS 多任务协作全景

```text
                     ┌────────────────────────────┐
                     │    ADC1 + DMA1_CH1 后台    │
                     │    4 路摇杆自动循环刷新    │
                     └─────────────┬──────────────┘
                                   │ ADC1_ReadSnapshot()
                                   ▼
┌─────────────────────┐  临界区  ┌────────────────────┐  临界区  ┌─────────────────────┐
│  Task_RC_Tx (Prio4) │◄────────►│  remote 状态中心   │◄────────►│  Task_Input (Prio3) │
│  10ms 严格周期      │  读AUX/  │  Remote + s_trim   │  改AUX/  │  20ms 周期扫描      │
│  滤波→组帧→NRF发送  │  Trim    └────────────────────┘  Trim    │  8路按键去抖状态机  │
└──────────┬──────────┘                                          └───┬─────────────┬───┘
           │ xTaskNotify(eSetBits)           xTaskNotify(eSetBits)   │             │
           │ BIT_RC_TX                       BIT_INPUT               │             │
           ▼                                                         │             │
┌─────────────────────┐                          xQueueOverwrite     │             │ xQueueSend
│Task_Monitor (Prio2) │◄─────────────────────────────────────────────┘             │
│  500ms 校验心跳掩码 │                                              ▼             ▼
│  全存活 → 喂 IWDG   │                                    ┌──────────────────┐ ┌──────────────────┐
│  采样各任务栈高水位 │                                    │Task_Storage(P2)  │ │Task_Buzzer(P1)   │
└─────────────────────┘                                    │阻塞等队列→vTask  │ │阻塞等队列+故障   │
                                                           │Delay→写EEPROM校验│ │声型统一输出 PA12 │
                                                           └──────────────────┘ └──────────────────┘
```

---

## 3. 任务配置与优先级设计依据

`FreeRTOSConfig.h` 中 `configMAX_PRIORITIES = 8`（Idle 任务优先级 0），5 个应用任务规划如下：

| 任务 | 优先级 | 栈深（字 / 字节） | 调度节拍 | 设计依据 |
| :--- | :---: | :---: | :--- | :--- |
| `Task_RC_Tx` | **4** | 160 / 640 B | `vTaskDelayUntil(10ms)` | 最高业务优先级。100Hz 无线控制是遥控器核心生命线，可随时抢占存储、蜂鸣与按键任务，保障发包抖动最小。 |
| `Task_Input` | **3** | 128 / 512 B | `vTaskDelayUntil(20ms)` | 次高优先级。20ms 扫描兼顾硬件消抖与手感延迟（<40ms），执行耗时极短（微秒级），不影响发包。 |
| `Task_Storage` | **2** | 128 / 512 B | 队列事件阻塞 | 中优先级。写 AT24C02 需要 10ms 内部写周期，在此使用 `vTaskDelay(10ms)` 主动让出 CPU，绝不阻塞高优先级任务。 |
| `Task_Monitor` | **2** | 128 / 512 B | `vTaskDelayUntil(500ms)` | 中优先级。每 500ms 仅唤醒一次，原子取走心跳位并决定是否喂狗，系统开销近乎为零。 |
| `Task_Buzzer` | **1** | 128 / 512 B | 队列等待 + 故障周期 | 最低业务优先级。蜂鸣时长为数十至数百毫秒，对毫秒级抖动不敏感，放在最低优先级最合理。 |

### FreeRTOS 堆内存预算（`heap_4`，总配额 8192 B）
- 5 个业务任务栈 + TCB：`(160 + 128 × 4) × 4 + 5 × ~96 ≈ 3,168 B`
- Idle 任务栈 + TCB：`128 × 4 + ~96 ≈ 608 B`
- 2 个消息队列（`g_queue_storage`、`g_queue_buzzer`）：约 `200 B`
- **总计约 4.0 KB**（占 8 KB Heap 不到 50%，余量充裕）。
- `configCHECK_FOR_STACK_OVERFLOW = 2`（栈底魔数模式校验）且 `configUSE_MALLOC_FAILED_HOOK = 1`，`Task_Monitor` 同时持续把各任务剩余栈字数刷新到全局变量 `g_stack_hwm`，在 Keil Watch 窗口即可直接观察真实栈水位。

---

## 4. 三大核心工程约束

### 4.1 单一硬件所有权（Single Ownership）
调度器启动后，每个外部硬件模块仅允许**唯一一个任务**直接读写，彻底避免并发总线冲突与状态互相覆盖：

| 硬件资源 | 唯一所有权任务 | 其他任务如何间接使用 |
| :--- | :--- | :--- |
| **NRF24L01+ & 软件 SPI** | `Task_RC_Tx` | 通过 `remote.c` 更新通道状态即可，无需触碰无线驱动 |
| **AT24C02 & 软件 I2C** | `Task_Storage` | 调用 `App_PostTrimSave(&trim)` 投递覆写队列 |
| **蜂鸣器 PA12** | `Task_Buzzer` | 调用 `App_PostBuzzerEvent(evt)` 投递音效队列（仅致命错误钩子例外） |
| **8 路按键 GPIO** | `Task_Input` | 读取 `remote.c` 暴露的 AUX / Trim 状态 |
| **IWDG 独立看门狗** | `Task_Monitor` | 调用 `App_NotifyHeartbeat(bit)` 上报心跳位 |

### 4.2 轻量级任务间通信选型
1. **临界区快照（`taskENTER_CRITICAL`）**：保护 `remote.c` 中不足 16 字节的通道与微调结构体拷贝，耗时 < 2us，比互斥锁更轻且绝无优先级反转或死锁风险。
2. **深度 1 覆写队列（`xQueueOverwrite`）**：用于 `Task_Input → Task_Storage`。飞手快速连按 5 次微调键时，队列始终只保留最新目标值，避免 5 次连续 EEPROM 写入，配合 `Task_Storage` 内的轴级脏检查，大幅延长 EEPROM 寿命。
3. **定长事件队列（`xQueueSend`）**：用于 `Task_Input → Task_Buzzer`，队列满时 0 超时非阻塞丢弃，保证按键任务永远不会因蜂鸣器正在播放长报警音而卡顿。
4. **任务通知位掩码（`xTaskNotify(..., eSetBits)`）**：零额外 RAM 消耗，取代重型 `EventGroup`，向 `Task_Monitor` 上报多任务健康心跳。

### 4.3 延时接口使用规范
- **调度器启动前**（`main.c`、`ALL_Init()`）：使用 `delay_ms()`（基于 TIM3 精确 1kHz 中断，局部时间戳，可重入）。
- **调度器启动后**（任何 `Task_*` 内）：一律使用 `vTaskDelayUntil()`（周期任务）或 `vTaskDelay()`（相对延时），严禁调用忙等 `delay_ms()`，以确保空闲任务能及时进入 `__WFI()` 睡眠省电。

---

## 5. 二次开发扩展指南

### 示例 A：新增一个新按键事件（如一键微调归零）
1. 在 `src/bsp/key.h` 的 `key_event_t` 枚举中添加新事件，在 `key.c` 中识别对应触发条件；
2. 在 `src/control/remote.c` 增加线程安全的状态变更接口；
3. 在 `src/app/task_input.c` 的 `switch (evt)` 中调用该接口，并通过 `App_PostTrimSave()` 与 `App_PostBuzzerEvent()` 通知存储与蜂鸣器任务。

### 示例 B：新增一种蜂鸣器提示音
1. 在 `src/app/app_tasks.h` 的 `buzzer_event_t` 枚举添加新声型；
2. 在 `src/app/task_buzzer.c` 的 `buzzer_play_event()` 补充对应的响/停节拍；
3. 在任意业务任务中调用 `App_PostBuzzerEvent(新枚举)` 即可，绝不直接操作 `Buzzer_On/Off`。

### 示例 C：新增一个需要受看门狗保护的核心任务
1. 在 `src/app/app_tasks.h` 中分配优先级、栈深，并在 `HEARTBEAT_ALL_MASK` 中新增对应心跳位；
2. 在 `src/app/app_tasks.c` 的 `App_TasksInit()` 中创建该任务；
3. 在新任务主循环周期末尾调用 `App_NotifyHeartbeat(HEARTBEAT_BIT_XXX)`；
4. 同步把新 `.c` 文件添加到 `remote.uvprojx` 与 `.eide/eide.yml`。
