# Cloud-Drone-Remote 项目编译与开发指南

## 编译方法

### WSL2 环境编译（推荐）
**关键：Keil 是 Windows 程序，必须直接使用 Windows 路径，不能用 `wslpath` 转换。**

```bash
/mnt/d/Keil_v5/UV4/UV4.exe -j0 -b "D:\_study\projects\_MyProject\Drone\Cloud-Drone-Remote\remote.uvprojx" -o "D:\_study\projects\_MyProject\Drone\Cloud-Drone-Remote\build\logs\build.log"
```

### 命令行参数
- `-j0`：不打开 IDE 窗口
- `-b`：build（增量编译）
- `-r`：rebuild（完全重新编译）
- `-o`：日志输出路径

### 编译结果检查
- Keil 构建日志：`build/logs/build.log`
- 成功标志：日志末尾包含 `0 Error(s), 0 Warning(s)`
- 输出文件：`build/keil/remote.axf`、`build/keil/remote.hex`
- EIDE 构建产物（若用）：`build/eide/remote/`

## 项目结构
```text
Cloud-Drone-Remote/           # 遥控器（发送端）工程，与 Cloud-Drone 飞控端严格协议对接
├── src/                      # 全部源码
│   ├── driver/               # 底层外设驱动（ADC DMA, 软件I2C, 软件SPI, TIM3 1kHz）
│   ├── bsp/                  # 板级器件驱动（NRF24L01+, AT24C02 EEPROM, 按键状态机, 蜂鸣器）
│   ├── control/              # 遥控业务层（rc_protocol 纯算法与组帧, remote 线程安全状态中心）
│   ├── app/                  # FreeRTOS 多任务层（app_tasks, task_rc, task_input, task_storage, task_buzzer, task_monitor, hooks）
│   ├── core/                 # 系统核心（main, init, delay, sys）
│   ├── include/              # 全局类型定义（all_data.h, all_define.h）
│   ├── FreeRTOS/             # FreeRTOS V9.0.0 内核（含 FreeRTOSConfig.h, RVDS/ARM_CM3 移植）
│   ├── Lib/                  # STM32F10x 标准外设库
│   └── StartUp/              # 启动文件（CMSIS, 中断向量表）
├── build/                    # 编译产物与日志（git 忽略）
├── docs/                     # 设计与架构文档（01 硬件平台 / 02 遥控协议 / 03 FreeRTOS 架构导读）
├── remote.uvprojx            # Keil 工程文件（增删 .c 文件须与 .eide/eide.yml 同步）
└── .eide/eide.yml            # EIDE 工程配置
```

## 关键架构与维护说明
- **FreeRTOS 版本与任务架构**：V9.0.0（RVDS/ARM_CM3 移植，Keil AC5 编译），采用「4 业务任务 + 1 监控守护任务」解耦架构（详见 `docs/03_FreeRTOS架构与代码导读.md`），堆 8KB，开启 `configCHECK_FOR_STACK_OVERFLOW=2` 与 `configUSE_MALLOC_FAILED_HOOK=1`。
- **多任务看门狗（IWDG）**：LSI 标称 40kHz，分频 128、重载 800 → 超时约 2.56s。由 `Task_Monitor` 启动并每 500ms 汇总 `Task_RC_Tx` 与 `Task_Input` 的任务通知心跳位，全部存活才喂狗；任一核心任务挂死 → ~2.6s 内复位重启恢复发送（快于飞控端 5s 失联门限）。`main.c` 已开启 `DBGMCU_IWDG_STOP`，断点调试不受影响。
- **启动自检与运行期容错**：NRF 在位检测最多 20 次（×50ms）、EEPROM 自检最多 20 次；失败置 `g_boot_fault` 后继续启动，由 `Task_Buzzer` 周期报警（NRF 故障=每秒长鸣 0.4s 且跳过发送；EEPROM 故障=每秒两短鸣、微调清零并禁用写入）。运行期 NRF 连续发送超时亦具备自动重检与降级报警能力。
- **系统时钟与 TIM3**：**HSI 内部时钟 48MHz**（`8MHz / 2 × 12`，无外部晶振，`SYSCLK_FREQ_48MHz`）。APB1=24MHz 时定时器输入时钟 `TIM3CLK = 48MHz`，**TIM3 配置为 `PSC=47 / ARR=999` 提供精确 1kHz（1ms）时钟**；SysTick 归 FreeRTOS。任务内一律使用 `vTaskDelay` / `vTaskDelayUntil`，`delay_ms` 仅限调度器启动前。
- **通信协议红线**：NRF 参数、32 字节帧格式与 `RC_THR_SCALE = 0.20f` 必须与飞控端严格兼容，修改前务必阅读 `docs/02_遥控协议说明.md`。
