/**
 * @file    bsp_fmc_sdram.h
 * @brief   外部 SDRAM (IS42S32800G-6BLI) 驱动模块头文件
 */

#ifndef _BSP_FMC_SDRAM_H
#define _BSP_FMC_SDRAM_H

#define EXT_SDRAM_ADDR ((uint32_t)0xC0000000)
#define EXT_SDRAM_SIZE ((uint32_t)(32 * 1024 * 1024))

/* LCD显存,第1页, 分配2M字节 */
#define SDRAM_LCD_BUF1 EXT_SDRAM_ADDR

/* LCD显存,第2页, 分配2M字节 */
#define SDRAM_LCD_BUF2 (EXT_SDRAM_ADDR + SDRAM_LCD_SIZE)

#define SDRAM_LCD_SIZE (2 * 1024 * 1024) /* 每层2M */
#define SDRAM_LCD_LAYER 2                /* 2层 */

/* 剩下的12M字节，提供给应用程序使用 */
#define SDRAM_APP_BUF (EXT_SDRAM_ADDR + SDRAM_LCD_SIZE * SDRAM_LCD_LAYER)
#define SDRAM_APP_SIZE (EXT_SDRAM_SIZE - SDRAM_LCD_SIZE * SDRAM_LCD_LAYER)

void bsp_InitExtSDRAM(void);
uint32_t bsp_TestExtSDRAM1(void);
uint32_t bsp_TestExtSDRAM2(void);

#endif

/* end of file */
