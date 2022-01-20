/*
*********************************************************************************************************
*
*    模块名称 : 串口中断+FIFO驱动模块
*    文件名称 : bsp_uart_fifo.h
*    说    明 : 头文件
*
*    Copyright (C), 2015-2020, 安富莱电子 www.armfly.com
*
*********************************************************************************************************
*/
#ifndef _BSP_USART_H
#define _BSP_USART_H

#include "bsp.h"

/*
    【安富莱STM32-V7】串口分配：
    【串口1】 RS232 芯片第1路   打印调试口
        PA9/USART1_TX
        P10/USART1_RX

    【串口2】 PA2 管脚用于以太网； RX管脚用于接收GPS信号
        PA2/USART2_TX/ETH_MDIO  (用于以太网，不做串口发送用)
        PA3/USART2_RX           接GPS模块输出

    【串口3】 RS485 通信 - TTL 跳线 和 排针
        PB10/USART3_TX
        PB11/USART3_RX
        PB2 控制RS485芯片的发送使能

    【串口4】 --- 不做串口用 SD卡占用           PC10/UART4_TX   PC11/UART4_RX
    【串口5】 --- 不做串口用 SD卡占用           PC12/UART5_TX   PD2/UART5_RX

    【串口6】--- GPRS模块 WIFI模块(ESP8266)
        PG14/USART6_TX
        PC7/USART6_RX

    【串口7】 --- 不做串口用 SPI3占用           PB4/UART7_TX     PB3/UART7_RX
    【串口8】 --- 不做串口用 LTDC显示接口用     PJ8/UART8_TX     PJ9/UART8_RX
*/

#define UART1_FIFO_EN 1
#define UART2_FIFO_EN 0
#define UART3_FIFO_EN 1
#define UART4_FIFO_EN 0
#define UART5_FIFO_EN 0
#define UART6_FIFO_EN 0
#define UART7_FIFO_EN 0
#define UART8_FIFO_EN 0

/* PB2 控制RS485芯片的发送使能 */
#define RS485_TXEN_GPIO_CLK_ENABLE() __HAL_RCC_GPIOB_CLK_ENABLE()
#define RS485_TXEN_GPIO_PORT GPIOB
#define RS485_TXEN_PIN GPIO_PIN_2

#define RS485_RX_EN() RS485_TXEN_GPIO_PORT->BSRR = (uint32_t)RS485_TXEN_PIN << 16U //低电平 接收
#define RS485_TX_EN() RS485_TXEN_GPIO_PORT->BSRR = RS485_TXEN_PIN                  //高电平 发送

/* 定义端口号 */
typedef enum
{
    COM1 = 0, /* USART1 */
    COM2 = 1, /* USART2 */
    COM3 = 2, /* USART3 */
    COM4 = 3, /* UART4  */
    COM5 = 4, /* UART5  */
    COM6 = 5, /* USART6 */
    COM7 = 6, /* UART7  */
    COM8 = 7  /* UART8  */
} COM_PORT_E;

/* 定义串口波特率和FIFO缓冲区大小，分为发送缓冲区和接收缓冲区, 支持全双工 */
#if UART1_FIFO_EN == 1
#define UART1_BAUD 115200
#define UART1_TX_BUF_SIZE 1 * 1024
#define UART1_RX_BUF_SIZE 1 * 1024
extern UART_HandleTypeDef huart1;
extern DMA_HandleTypeDef hdma_usart1_tx;
extern DMA_HandleTypeDef hdma_usart1_rx;
#endif

#if UART2_FIFO_EN == 1
#define UART2_BAUD 115200
#define UART2_TX_BUF_SIZE 1 * 1024
#define UART2_RX_BUF_SIZE 1 * 1024
extern UART_HandleTypeDef huart2;
extern DMA_HandleTypeDef hdma_usart2_tx;
extern DMA_HandleTypeDef hdma_usart2_rx;
#endif

#if UART3_FIFO_EN == 1
#define UART3_BAUD 115200
#define UART3_TX_BUF_SIZE 1 * 1024
#define UART3_RX_BUF_SIZE 1 * 1024
extern UART_HandleTypeDef huart3;
extern DMA_HandleTypeDef hdma_usart3_tx;
extern DMA_HandleTypeDef hdma_usart3_rx;
#endif

#if UART4_FIFO_EN == 1
#define UART4_BAUD 115200
#define UART4_TX_BUF_SIZE 1 * 1024
#define UART4_RX_BUF_SIZE 1 * 1024
extern UART_HandleTypeDef huart4;
extern DMA_HandleTypeDef hdma_usart4_tx;
extern DMA_HandleTypeDef hdma_usart4_rx;
#endif

#if UART5_FIFO_EN == 1
#define UART5_BAUD 115200
#define UART5_TX_BUF_SIZE 1 * 1024
#define UART5_RX_BUF_SIZE 1 * 1024
extern UART_HandleTypeDef huart5;
extern DMA_HandleTypeDef hdma_usart5_tx;
extern DMA_HandleTypeDef hdma_usart5_rx;
#endif

#if UART6_FIFO_EN == 1
#define UART6_BAUD 115200
#define UART6_TX_BUF_SIZE 1 * 1024
#define UART6_RX_BUF_SIZE 1 * 1024
extern UART_HandleTypeDef huart6;
extern DMA_HandleTypeDef hdma_usart6_tx;
extern DMA_HandleTypeDef hdma_usart6_rx;
#endif

#if UART7_FIFO_EN == 1
#define UART7_BAUD 115200
#define UART7_TX_BUF_SIZE 1 * 1024
#define UART7_RX_BUF_SIZE 1 * 1024
extern UART_HandleTypeDef huart7;
extern DMA_HandleTypeDef hdma_usart7_tx;
extern DMA_HandleTypeDef hdma_usart7_rx;
#endif

#if UART8_FIFO_EN == 1
#define UART8_BAUD 115200
#define UART8_TX_BUF_SIZE 1 * 1024
#define UART8_RX_BUF_SIZE 1 * 1024
extern UART_HandleTypeDef huart8;
extern DMA_HandleTypeDef hdma_usart8_tx;
extern DMA_HandleTypeDef hdma_usart8_rx;
#endif

/* 串口设备结构体 */
typedef struct
{
    UART_HandleTypeDef *huart;        /* STM32内部串口设备指针 */
    void (*SendBefor)(void);          /* 开始发送之前的回调函数指针（主要用于RS485切换到发送模式） */
    void (*SendOver)(void);           /* 发送完毕的回调函数指针（主要用于RS485将发送模式切换为接收模式） */
    void (*ReciveNew)(uint8_t _byte); /* 串口收到数据的回调函数指针 */
    RINGBUFF_T tx_kfifo;
    RINGBUFF_T rx_kfifo;
    uint8_t Sending; /* 正在发送中 */
} UART_T;

/* 供外部调用的变量声明 */

/* 供外部调用的函数声明 */
void bsp_InitUart(void);
void comSendBuf(COM_PORT_E _ucPort, uint8_t *_ucaBuf, uint16_t _usLen);
void comSendChar(COM_PORT_E _ucPort, uint8_t _ucByte);
uint8_t comGetChar(COM_PORT_E _ucPort, uint8_t *_pByte);
uint16_t comGetBuf(COM_PORT_E _ucPort, uint8_t *_pByte, uint16_t _usLen);
void comClearTxFifo(COM_PORT_E _ucPort);
void comClearRxFifo(COM_PORT_E _ucPort);
int comSetBaud(COM_PORT_E _ucPort, uint32_t _BaudRate);
uint16_t comGetLen(COM_PORT_E _ucPort);

void RS485_SendBuf(uint8_t *_ucaBuf, uint16_t _usLen);
void RS485_SendStr(char *_pBuf);
void RS485_SetBaud(uint32_t _baud);

#endif
/***************************** 安富莱电子 www.armfly.com (END OF FILE) *********************************/
