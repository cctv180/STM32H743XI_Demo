/**
 * @file    bsp.h
 * @brief   板级支持包（BSP）主头文件，包含所有外设驱动模块
 */
#ifndef _BSP_H_
#define _BSP_H_

/* 公共头文件 */
#include "stm32h7xx_hal.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* 定义 BSP 版本号 */
#define STM32_BOARD "STM32V7"
#define STM32_BSP_VERSION "V1.02"

/* 开启打印数据 */
#define BSP_INFO_EN

/* 开启调试打印 */
#define DEBUG_MODE 1

/*
 * 开启 Event Recorder组件 1开启 0关闭
 * 启用 Event Recorder 需要从RTE勾选
 * 1. Compiler 项目勾选 Event Recorder
 * 2. Debug (printf) Viewer 重定向 需要同时设置STDOUT EVR模式
 *    a. 启用Use MicroLIB库
 *    b. 不要重定向fpuc和fgetc 即删除串口打印重定向
 */
#define Enable_EventRecorder 0

/* RTOS_RTX开启  1开启 0关闭 */
#define USE_RTX 0

/* 获取系统时间 */
// #define get_system_ms() (get_system_ticks() / (SystemCoreClock / 1000ul)) //新版本已经集成
// #define get_system_us() (get_system_ticks() / (SystemCoreClock / 1000000ul))

/* CPU空闲时执行的函数 */
#define CPU_IDLE() bsp_Idle()
#define ERROR_HANDLER() Error_Handler(__FILE__, __LINE__);
/* 开关全局中断的宏 */
#define ENABLE_INT() __set_PRIMASK(0)  /* 使能全局中断 */
#define DISABLE_INT() __set_PRIMASK(1) /* 禁止全局中断 */

typedef enum
{
    BSP_ERR_NULL = 0,
    BSP_ERR_01,
    BSP_ERR_02,
    BSP_ERR_03,
    BSP_ERR_04,
    BSP_ERR_05,
} BSP_ERR_E;

/*
*********************************************************************************************************
* 以下宏自动处理与提示
*********************************************************************************************************
*/
#ifdef DEBUG_MODE
// #define BSP_Printf(...) bsp_log_debug(__FILE__, __LINE__, __VA_ARGS__)
#define BSP_Printf(...)                                 \
    do                                                  \
    {                                                   \
        printf("[D/SYS] (%s:%d) ", __FILE__, __LINE__); \
        printf(__VA_ARGS__);                            \
        printf("\r\n");                                 \
    } while (0)

#else
#define BSP_Printf(...)
#endif /* DEBUG_MODE END */

#ifdef BSP_INFO_EN
// #define BSP_INFO(...)  bsp_log_info(__VA_ARGS__)
#define BSP_INFO(...)        \
    do                       \
    {                        \
        printf("[I/SYS] ");  \
        printf(__VA_ARGS__); \
        printf("\r\n");      \
    } while (0)

#else
#define BSP_INFO(...)
#endif

#if Enable_EventRecorder == 1
#include "EventRecorder.h"
#endif // #if Enable_EventRecorder == 1

#if USE_RTX == 1

#ifndef RTE_CMSIS_RTOS2
/*  ARM::CMSIS:RTOS2:Keil RTX5:Source:5.5.2 */
#define RTE_CMSIS_RTOS2             /* CMSIS-RTOS2 */
#define RTE_CMSIS_RTOS2_RTX5        /* CMSIS-RTOS2 Keil RTX5 */
#define RTE_CMSIS_RTOS2_RTX5_SOURCE /* CMSIS-RTOS2 Keil RTX5 Source */
#endif

#endif // #if USE_RTX == 1

/* 检查是否定义了开发板型号 */
#if !defined(STM32_BOARD)
#error "Please define the board model : STM32_BOARD"
#endif

/* 这个宏仅用于调试阶段排错printf */
#if Enable_EventRecorder == 1
#include "EventRecorder.h"
#endif

/* printf 二进制格式输出 宏 */
#define BYTE_TO_BINARY_PATTERN "0b%c%c%c%c%c%c%c%c"
/* printf 二进制格式输出 宏 */
#define BYTE_TO_BINARY(byte)       \
    (byte & 0x80 ? '1' : '0'),     \
        (byte & 0x40 ? '1' : '0'), \
        (byte & 0x20 ? '1' : '0'), \
        (byte & 0x10 ? '1' : '0'), \
        (byte & 0x08 ? '1' : '0'), \
        (byte & 0x04 ? '1' : '0'), \
        (byte & 0x02 ? '1' : '0'), \
        (byte & 0x01 ? '1' : '0')

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

#ifndef NULL
#define NULL 0
#endif

/* Open Software Library */
#include "perf_counter.h"
#include "ring_buffer.h"
#include "multi_button.h"
#include "shell.h"
#include "shell_port.h"
#include "MultiTimer.h"
#include "timer_port.h"
#include "utils_lib.h"

/* 底层驱动模块（当前工程已启用） */
#include "bsp_dma.h"
#include "bsp_led.h"
#include "bsp_key.h"
#include "bsp_uart.h"
#include "bsp_qspi.h"
#include "bsp_fmc_sdram.h"
#include "bsp_fmc_io.h"
#include "bsp_tft_h7.h"
#include "bsp_beep.h"
#include "bsp_tim_pwm.h"
#include "bsp_sdio_sd.h"

/* 提供给其他C文件调用的函数 */
void bsp_Init(void);
void bsp_Idle(void);
void System_Init(void);
void Error_Handler(char *file, uint32_t line);

void bsp_RunPer1ms(void);
void bsp_RunPer10ms(void);

#endif
/* end of file */
