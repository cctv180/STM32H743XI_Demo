/*
*********************************************************************************************************
*
*    模块名称 : W25Q256 QSPI驱动模块
*    文件名称 : bsp_qspi_w25q256.h
*
*    Copyright (C),  2020-2030. 安富莱电子 www.armfly.com
*
*********************************************************************************************************
*/

#ifndef _BSP_QSPI_H
#define _BSP_QSPI_H

/* W25Q64JV基本信息 */
#define QSPI_FLASH_SIZES 8 * 1024 * 1024                          /* Flash大小，8MB*/
#define QSPI_FLASH_SIZE (32 - POSITION_VAL(QSPI_FLASH_SIZES - 1)) /* Flash大小，2^23 = 8MB*/
#define QSPI_SECTOR_SIZE (4 * 1024)                               /* 扇区大小，4KB */
#define QSPI_PAGE_SIZE 256                                        /* 页大小，256字节 */
#define QSPI_END_ADDR (QSPI_FLASH_SIZES - 1)                      /* 末尾地址 */

/* W25Q64JV相关命令 */
#define QSPI_READ_JEDEC_ID 0x9F     /* 读取JEDEC ID命令 */
#define QSPI_READ_STATU_REG_1 0x05  /* 读取状态寄存器1 */
#define QSPI_READ_STATU_REG_2 0x35  /* 读取状态寄存器2 */
#define QSPI_READ_STATU_REG_3 0x15  /* 读取状态寄存器3 */
#define QSPI_WRITE_STATU_REG_1 0x01 /* 写入状态寄存器1 */
#define QSPI_WRITE_STATU_REG_2 0x31 /* 写入状态寄存器2 */
#define QSPI_WRITE_STATU_REG_3 0x11 /* 写入状态寄存器3 */

#define QSPI_WRITE_ENABLE_CMD 0x06     /* 写使能指令 */
#define QSPI_WRITE_ENABLE_REG_CMD 0x50 /* 写易失STATUS寄存器使能指令 */
#define QSPI_WRITE_DISABLE_CMD 0x04    /* 写失能指令 */
#define QSPI_SECTOR_ERASE_4K_CMD 0x20  /* 擦除4K扇区,地址4K对齐 */
#define QSPI_BULK_ERASE_CMD 0xC7       /* 整个芯片擦除命令 */
#define QSPI_PAGE_PROG_CMD 0x02        /* 页编程命令 */
#define QSPI_FAST_READ_4_CMD 0xEB      /* 24bit地址的4线快速读取命令 */

/* 供外部调用的变量声明 */
extern QSPI_HandleTypeDef hqspi;

/* 供外部调用的函数声明 */

void bsp_InitQspi(void);
uint8_t QSPI_ReadSR(uint8_t _reg);
uint32_t QSPI_ReadID(void);
void QSPI_WaitBusy(void);
uint8_t QSPI_WriteSR(uint8_t _reg, uint8_t _value);
void QSPI_EraseSector(uint32_t address);
void QSPI_EraseChip(void);
void QSPI_ReadBuffer(uint8_t *_pBuf, uint32_t _uiReadAddr, uint32_t _uiSize);
void QSPI_WriteBuffer(uint8_t *_pBuf, uint32_t _uiWriteAddr, uint16_t _usWriteSize);
void QSPI_MemoryMapped(void);

#endif

/***************************** 安富莱电子 www.armfly.com (END OF FILE) *********************************/
