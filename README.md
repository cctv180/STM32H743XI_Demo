# STM32H743XI_Demo

基于 **STM32H743XIH6**（TFBGA240）的演示 / 开发工程，使用 **STM32 HAL** 与 **STM32CubeMX**（`project.ioc`），应用与板级代码参考安富莱 **STM32-V7** BSP 风格。

## 硬件

- MCU：STM32H743XIH6
- 外设概要：LTDC 显示、FMC SDRAM、QSPI、SDMMC SD 卡、多路 USART（DMA）、定时器等（详见 `project.ioc` 与 `User/bsp`）

## 开发与编译

- **IDE**：Keil µVision
- **工程文件**：`MDK-ARM/project.uvprojx`
- **目标芯片**：STM32H743XIHx（ARM Compiler 6）

构建成功后可根据工程配置生成 hex（例如 `output(mdk).hex`）。

## 代码结构（简要）

| 路径 | 说明 |
|------|------|
| `User/main.c` | 应用入口：`System_Init()`、`bsp_Init()`，主循环 Shell / MultiTimer / 按键测试 |
| `User/bsp/` | 板级支持包，详见下文「User/bsp（板级支持包）」一节 |
| `Src/` | CubeMX 生成代码（如 `main.c`、`stm32h7xx_hal_msp.c`）；当前 Keil 工程中部分文件默认**不参与编译**，以 `MDK-ARM/project.uvprojx` 为准 |
| `Drivers/` | STM32 HAL、CMSIS |
| `OpenLib/` | 第三方/开源组件：letter_shell、MultiTimer、perf_counter、xxtea 等 |
| `Inc/`、`MDK-ARM/` | 头文件与 Keil 工程、启动文件 |

## User/bsp（板级支持包）

`User/bsp` 把开发板相关硬件封装成可复用模块；`bsp.h` 为总入口（版本宏、调试打印 `BSP_Printf` / `BSP_INFO`、并包含各子模块头文件与 `OpenLib` 中 Shell、MultiTimer 等）。

### 系统初始化分工

| 入口 | 文件 | 作用 |
|------|------|------|
| `System_Init()` | `bsp.c` | MPU、`I/D Cache`、`HAL_Init()`、系统时钟（约 400 MHz SYSCLK / 200 MHz HCLK，HSE 25 MHz）、可选 Event Recorder |
| `bsp_Init()` | `bsp.c` | 按顺序初始化外设与上层组件（见下表） |

应用侧只需在 `User/main.c` 中先 `System_Init()` 再 `bsp_Init()` 一次即可。

### `bsp_Init()` 中的典型顺序（与依赖关系）

1. **SDRAM**（`bsp_fmc_sdram`）— 扩展内存，常为显存、大缓冲等提供空间
2. **QSPI**（`bsp_qspi`）— 外部串行 Flash 等
3. **perf_counter**（`init_cycle_counter`）— 周期计数 / 与定时基准配合
4. **按键**（`bsp_key`）— 需在滴答扫描前完成初始化
5. **DMA**（`bsp_dma`）— 与串口等外设的 DMA 通道
6. **串口**（`bsp_uart`）— 多路 UART 及 `printf` 重定向等
7. **Shell**（`userInitShell`，见 `OpenLib/letter_shell`）— 命令行交互
8. **FMC 扩展 IO**（`bsp_fmc_io`）— 通过 FMC 挂接的 74HC574 类扩展 IO，**须在 LED 初始化之前**
9. **LED**（`bsp_led`）、**蜂鸣器**（`bsp_beep`）
10. **MultiTimer**（`userInitMultiTime`）— 软件定时器调度
11. **LCD**（`bsp_tft_h7`）— LTDC / 屏相关初始化
12. **SD 卡**（`bsp_sdio_sd`）— SDMMC，初始化后会打印卡信息（容量、块大小等）

实际顺序以 `User/bsp/bsp.c` 中 `bsp_Init()` 为准；若改管脚或时钟，需同时对照 `project.ioc` 与对应 `bsp_*.c`。

### 子模块与源文件对应关系

| 模块 | 主要文件 | 说明 |
|------|----------|------|
| 核心 | `bsp.h`、`bsp.c` | 总入口、`System_Init` / `bsp_Init`、`bsp_Idle`、时钟与错误处理等 |
| 通用工具 | `bsp_user_lib.h`、`bsp_user_lib.c` | 板级通用小工具函数 |
| SDRAM | `bsp_fmc_sdram.h`、`bsp_fmc_sdram.c` | FMC 外部 SDRAM 初始化与使用 |
| FMC 扩展 IO | `bsp_fmc_io.h`、`bsp_fmc_io.c` | FMC 总线扩展数字 IO（如 74HC574），驱动 LED 位选等前需先初始化 |
| QSPI | `bsp_qspi.h`、`bsp_qspi.c` | QSPI 外设与外部 Flash 访问 |
| DMA | `bsp_dma.h`、`bsp_dma.c` | USART 等外设 DMA 配置，与 HAL 句柄配合 |
| UART | `bsp_uart.h`、`bsp_uart.c` | 多路串口初始化、收发与调试输出 |
| 按键 | `bsp_key.h`、`bsp_key.c` | 按键扫描，常与 `OpenLib/MultiButton` 配合 |
| LED | `bsp_led.h`、`bsp_led.c` | LED GPIO / 位控制 |
| 蜂鸣器 | `bsp_beep.h`、`bsp_beep.c` | 蜂鸣器硬件初始化与控制 |
| 定时 / PWM | `bsp_tim_pwm.h`、`bsp_tim_pwm.c` | 定时器、PWM（含与系统节拍、外设相关的配置） |
| TFT / LCD | `bsp_tft_h7.h`、`bsp_tft_h7.c` | H7 板 TFT、与 LTDC 相关的 BSP 层 |
| SD 卡 | `bsp_sdio_sd.h`、`bsp_sdio_sd.c` | SDMMC 访问 SD 卡：初始化、读写、信息查询等 |

头文件在 `User/bsp/inc/`，实现在 `User/bsp/src/`；`bsp.h` / `bsp.c` 位于 `User/bsp/` 根目录。`bsp.h` 中还预留了大量未启用的驱动包含行（SPI、I2C、触摸等），可按需取消注释并加入工程。

## CubeMX

修改时钟、引脚或外设时，用 **STM32CubeMX** 打开 `project.ioc` 重新生成代码，并注意与 `User/` 下 BSP 的管脚与初始化是否一致。

## 许可

HAL/CMSIS 等以 ST 及各自组件许可证为准；BSP 头部注释含安富莱版权声明，使用时请遵守相应条款。
