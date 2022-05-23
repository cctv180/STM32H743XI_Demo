/*
*********************************************************************************************************
*
*    模块名称 : BSP模块(For STM32H7)
*    文件名称 : bsp.h
*    版    本 : V1.0
*    说    明 : 这是硬件底层驱动程序的主文件。每个c文件可以 #include "bsp.h" 来包含所有的外设驱动模块。
*               bsp = Borad surport packet 板级支持包
*    修改记录 :
*        版本号  日期         作者       说明
*        V1.0    2018-07-29  Eric2013   正式发布
*
*    Copyright (C), 2018-2030, 安富莱电子 www.armfly.com
*
*********************************************************************************************************
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
#define DEBUG_MODE

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
#define get_system_ms() (get_system_ticks() / (SystemCoreClock / 1000ul))
#define get_system_us() (get_system_ticks() / (SystemCoreClock / 1000000ul))

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
//#define BSP_Printf(...) bsp_log_debug(__FILE__, __LINE__, __VA_ARGS__)
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
//#define BSP_INFO(...)  bsp_log_info(__VA_ARGS__)
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

/* 通过取消注释或者添加注释的方式控制是否包含底层驱动模块 */
#include "bsp_dma.h"
//#include "bsp_msg.h"
#include "bsp_user_lib.h"
//#include "bsp_timer.h"
#include "bsp_led.h"
#include "bsp_key.h"
//#include "bsp_dwt.h"

//#include "bsp_cpu_rtc.h"
//#include "bsp_cpu_adc.h"
//#include "bsp_cpu_dac.h"
#include "bsp_uart.h"
//#include "bsp_uart_gps.h"
//#include "bsp_uart_esp8266.h"
//#include "bsp_uart_sim800.h"

//#include "bsp_spi_bus.h"
//#include "bsp_spi_ad9833.h"
//#include "bsp_spi_ads1256.h"
//#include "bsp_spi_dac8501.h"
//#include "bsp_spi_dac8562.h"
//#include "bsp_spi_flash.h"
//#include "bsp_spi_tm7705.h"
//#include "bsp_spi_vs1053b.h"

//#include "bsp_fmc_sdram.h"
//#include "bsp_fmc_nand_flash.h"
//#include "bsp_fmc_ad7606.h"
//#include "bsp_fmc_oled.h"
#include "bsp_fmc_io.h"

//#include "bsp_i2c_gpio.h"
//#include "bsp_i2c_bh1750.h"
//#include "bsp_i2c_bmp085.h"
//#include "bsp_i2c_eeprom_24xx.h"
//#include "bsp_i2c_hmc5883l.h"
//#include "bsp_i2c_mpu6050.h"
//#include "bsp_i2c_si4730.h"
//#include "bsp_i2c_wm8978.h"

//#include "bsp_tft_h7.h"
//#include "bsp_tft_429.h"
//#include "bsp_tft_lcd.h"
//#include "bsp_ts_touch.h"
//#include "bsp_ts_ft5x06.h"
//#include "bsp_ts_gt811.h"
//#include "bsp_ts_gt911.h"
//#include "bsp_ts_stmpe811.h"

#include "bsp_beep.h"
#include "bsp_tim_pwm.h"
//#include "bsp_sdio_sd.h"
//#include "bsp_dht11.h"
//#include "bsp_ds18b20.h"
//#include "bsp_ps2.h"
//#include "bsp_ir_decode.h"
//#include "bsp_camera.h"
//#include "bsp_rs485_led.h"
//#include "bsp_can.h"

/* 提供给其他C文件调用的函数 */
void bsp_Init(void);
void bsp_Idle(void);
void System_Init(void);
void Error_Handler(char *file, uint32_t line);

void bsp_RunPer1ms(void);
void bsp_RunPer10ms(void);

#endif
/***************************** 安富莱电子 www.armfly.com (END OF FILE) *********************************/
