# Cloud-Drone-Remote 项目编译指南

## 编译方法

### WSL2 环境编译（推荐）
**关键：Keil是Windows程序，必须直接使用Windows路径，不能用 `wslpath` 转换。**

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
- 输出文件：`build/keil/remote.axf`、`build/keil/remote.hex`（Keil 输出目录与中间产物 `.o/.crf/.d/.map/.lst` 同在此目录）
- EIDE 构建产物（若用）：`build/eide/remote/`

## 项目结构
```
Cloud-Drone-Remote/           # 遥控器（发送端）工程，结构与 Cloud-Drone 飞控端一致
├── src/                      # 全部源码
│   ├── driver/               # 底层外设驱动（ADC DMA, 软件I2C, 软件SPI, TIM, USART）
│   ├── bsp/                  # 板级设备驱动（NRF24L01, AT24C02 EEPROM）
│   ├── control/              # 遥控应用逻辑（摇杆采集/滤波/按键/组帧发送）
│   ├── app/                  # FreeRTOS 任务层（遥控任务, 错误钩子）
│   ├── core/                 # 系统核心（main, 初始化, 延时, 毫秒时钟）
│   ├── include/              # 全局类型定义（all_data.h, all_define.h）
│   ├── FreeRTOS/             # FreeRTOS V9.0.0 内核（含 FreeRTOSConfig.h, RVDS/ARM_CM3 移植）
│   ├── Lib/                  # STM32标准外设库
│   └── StartUp/              # 启动文件（CMSIS, 中断向量表）
├── build/                    # 编译产物与日志（git 忽略）
│   ├── keil/                 # Keil 输出（axf/hex/obj/crf/map/lst）
│   ├── eide/                 # EIDE 输出
│   └── logs/                 # 编译日志
├── docs/                     # 设计参考文档（硬件/遥控协议）
├── remote.uvprojx            # Keil 工程文件
└── remote.uvoptx             # Keil 用户选项
```

## 重要说明
- FreeRTOS 版本：V9.0.0（RVDS/ARM_CM3 移植，Keil AC5 编译），与飞控端一致
- **看门狗（IWDG）**：LSI 标称 40kHz，分频 128、重载 800 → 超时约 2.56s。
  在 RCTask 启动时开狗（task_rc.c），**每 10ms 周期喂一次**；任务任何环节
  挂死 → 约 2.6s 内复位重启恢复发送（快于飞控端 5s 失联门限）。
  main.c 配置了 DBGMCU 断点冻结 IWDG，调试不受影响。
  **修改 RCTask 循环时注意：新增的阻塞调用总时长不得超过 ~2s，否则会被复位**
- 系统时钟：**HSI 内部时钟 48MHz**（8MHz/2 × 12，遥控器无外部晶振）；
  `system_stm32f10x.c` 已定义 `SYSCLK_FREQ_48MHz`，`SystemCoreClock` 为真实主频
- NVIC 优先级分组：Group4；TIM3=6（1ms 毫秒时钟，不调 API）、SysTick/PendSV=15（内核）
- 毫秒时钟：SysTick 归 FreeRTOS，由 **TIM3**（1kHz，PSC=23/ARR=999）提供，
  `delay_ms`/`SysTick_count` 接口不变，调度器启动前后均可用
- 通信协议与飞控端严格兼容（NRF 参数、32 字节帧格式），修改前务必阅读 `docs/`
