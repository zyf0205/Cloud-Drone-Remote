# Cloud-Drone-Remote 遥控器

基于 STM32F103C8T6 + FreeRTOS V9.0.0 的四轴飞行器**遥控器端**(发送端),
与 [Cloud-Drone](../Cloud-Drone)(飞控端)通过 NRF24L01+ 2.4GHz 单向链路通信。

```
RCTask(唯一任务, 10ms 周期)
  ├─ 摇杆 ADC 读取(DMA 循环采集,后台自动刷新)
  ├─ 通道映射(1000~2000 + EEPROM 微调) → 一阶低通 → 钳位
  ├─ fault_beep() 启动自检故障蜂鸣提醒
  ├─ key() 按键状态机(AUX 开关/摇杆微调)
  └─ NRF_SEND() 组帧(帧头+8通道大端+校验和) → 无线发送
```

单任务架构:采集→滤波→组帧→发送是一条纯顺序数据流,
ADC 由 DMA 后台刷新,一个任务内串行完成全部工作,天然无竞态。

## 硬件与外设(详见 `docs/01_硬件平台说明.md`)

- 时钟:**HSI 内部时钟 48MHz**(无外部晶振,8MHz/2 × 12);
  1ms 毫秒时钟由 TIM3 提供,SysTick 归 FreeRTOS
- 摇杆:ADC1 DMA 循环采集 4 通道(yaw/thr/pitch/roll)
- 按键:AUX1~4(PA15/PB11/PC14/PC15)、微调前后左右(PB0/PB1/PA6/PA7),
  均低电平有效
- NRF24L01+:软件 SPI,1Mbps/0dBm/RF_CH=1,单向无 ACK
- AT24C02 EEPROM:软件 I2C,微调值持久化
- 蜂鸣器 PA12:开机提示/按键反馈/故障报警

## 可靠性设计

| 机制 | 说明 |
|---|---|
| IWDG 看门狗 | 任务启动时开狗(约 2.56s),每 10ms 喂一次;挂死 2.6s 内自动复位重启恢复发送,快于飞控端 5s 失联门限,空中链路可自动恢复 |
| 启动自检超时 | NRF 在位检测最多 20 次(×50ms)、EEPROM 自检最多 20 次,失败不再死等卡死开机 |
| 故障蜂鸣报警 | NRF 故障=每秒长鸣 0.4s(跳过发送);EEPROM 故障=每秒两短鸣(微调清零,仍可飞行) |
| 栈溢出/内存失败钩子 | 蜂鸣器长鸣提示 |

## 按键操作

**组合进入**:8 个按键全部按住约 30ms(蜂鸣器静音)→ 松开其余、保留目标键 → 执行对应功能(蜂鸣器短鸣确认)。

| 按键 | 功能 |
|---|---|
| AUX1 | 飞控定高开关 |
| AUX2 | 飞控光流定点开关(需先开定高) |
| AUX3 | 飞控陀螺校准触发(未解锁+油门最低时) |
| AUX4 | 预留 |
| 左/右微调 | Roll 中位 ∓5(限 ±250),即时写 EEPROM |
| 前/后微调 | Pitch 中位 ∓5,同上 |

微调用于补偿摇杆机械中位偏差:悬停往一边漂时微调对应方向,断电不丢。

## 发送帧格式(与飞控端严格一致)

```
字节:  0    1    2    3    4..19             20..30   31
      0xAA 0xAF 0x03 0x1B  8 通道大端(2B/个)  保留 0   前 31 字节求和校验
通道顺序: roll pitch thr yaw AUX1 AUX2 AUX3 AUX4(量程 1000~2000)
```

10ms 一帧(100Hz),单向无 ACK,链路可靠性由飞控端 5s 失联保护兜底。
协议参数任何改动必须两端同步,详见 `docs/02_遥控协议说明.md`。

## 目录结构

```
Cloud-Drone-Remote/
├── src/
│   ├── app/       FreeRTOS 任务层(遥控任务、错误钩子)
│   ├── bsp/       NRF24L01、AT24C02 EEPROM
│   ├── control/   遥控核心逻辑(采集/滤波/按键/组帧)
│   ├── core/      main、初始化、毫秒时钟、蜂鸣器
│   ├── driver/    软件外设驱动(ADC DMA、I2C、SPI、TIM)
│   ├── include/   全局类型与声明
│   ├── FreeRTOS/  V9.0.0 内核(RVDS/ARM_CM3 移植)
│   ├── Lib/       STM32 标准外设库
│   └── StartUp/   启动文件与 CMSIS
├── docs/          硬件平台说明、遥控协议说明
└── remote.uvprojx
```

## 编译与烧录

```bash
D:\Keil_v5\UV4\UV4.exe -j0 -b "D:\_study\projects\_MyProject\Drone\Cloud-Drone-Remote\remote.uvprojx" -o "D:\_study\projects\_MyProject\Drone\Cloud-Drone-Remote\build\logs\build.log"
```

成功标志:日志末尾 `0 Error(s), 0 Warning(s)`;产物 `build/keil/remote.hex`。
代码风格统一 `.clang-format`(与飞控端一致)。

## 常见问题

| 现象 | 处理 |
|---|---|
| 上电后每秒长鸣 | NRF 模块不在位/自检失败,检查供电与接线 |
| 上电后每秒两短鸣 | EEPROM 异常,微调不可用(中位回出厂状态),仍可飞行 |
| 飞控收不到信号 | 两端 RF 参数必须一致(RF_CH=1);查 NRF 天线与 3.3V 供电 |
| 微调不生效 | 微调的是摇杆中位,飞控端需重新解锁后应用到期望角 |
| 摇杆某通道跳动 | ADC 供电/接地干扰;代码已含一阶低通+钳位 |
