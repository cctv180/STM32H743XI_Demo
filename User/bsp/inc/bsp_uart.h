/**
 *********************************************************************************************************
 * @file    bsp_uart.h
 * @brief   多串口 (USART/UART) 收发 BSP 驱动
 *
 * 设计要点:
 *  - 收发均走 DMA + Ring Buffer (kfifo) , 应用层无阻塞写入 / 非阻塞读取.
 *  - RX 走 idle-line + 循环 DMA (HAL_UARTEx_ReceiveToIdle_DMA), 由
 *    HAL_UARTEx_RxEventCallback 推进 ring buffer 的 write_index.
 *  - TX 走单次 DMA (HAL_UART_Transmit_DMA), 在 TxCplt 回调中链式
 *    投递 ring buffer 的剩余数据.
 *  - 为方便从 STM32CubeMX 迁移, 文件内 HAL_UART_MspInit / HAL_UART_MspDeInit
 *    保持 CubeMX 生成的结构 (instance 级 if/else), 直接覆盖 HAL 弱符号.
 *  - 编译期通过 UARTx_FIFO_EN 开关启用具体端口, 未启用的端口不分配内存.
 *
 * 公共 API:
 *  - bsp_InitUart()      初始化已使能的串口 (变量初始化 + 硬件配置 + 启动 RX DMA)
 *  - comSendBuf/Char     非阻塞发送
 *  - comGetChar/Buf      非阻塞读取 (内部处理 D-Cache 一致性)
 *  - comGetLen           读取缓冲中已经积累的字节数
 *  - comClearTx/RxFifo   清空缓冲区
 *  - comSetBaud          运行期切换波特率 (沿用 HAL UART_SetConfig 的逻辑)
 *  - RS485_*             基于 USART3 的 RS485 收发壳
 *
 * Copyright (C) Project Contributors. All rights reserved.
 *********************************************************************************************************
 */
#ifndef BSP_UART_H
#define BSP_UART_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "bsp.h"

/*
 *********************************************************************************************************
 *                                            端口分配 (板级)
 *********************************************************************************************************
 *  USART1  PA9 (TX)  / PA10 (RX)            打印调试口 / RS232
 *  USART2  PA2 (TX)  / PA3  (RX)            (TX 与以太网共用, 一般仅做 RX 接 GPS)
 *  USART3  PB10 (TX) / PB11 (RX) / PB2 TXEN RS485
 *  UART4   PC10 / PC11                      (默认与 SD 卡复用, 不开)
 *  UART5   PC12 / PD2                       (默认与 SD 卡复用, 不开)
 *  USART6  PG14 / PC7                       GPRS / WIFI(ESP8266)
 *  UART7   PB4  / PB3                       (默认与 SPI3 复用, 不开)
 *  UART8   PJ8  / PJ9                       (默认与 LTDC 复用, 不开)
 */

/* 编译期端口使能开关. 未启用的端口不分配 buffer / handle / DMA / IRQ 代码. */
#ifndef UART1_FIFO_EN
#define UART1_FIFO_EN 1
#endif /* UART1_FIFO_EN */

#ifndef UART2_FIFO_EN
#define UART2_FIFO_EN 0
#endif /* UART2_FIFO_EN */

#ifndef UART3_FIFO_EN
#define UART3_FIFO_EN 1
#endif /* UART3_FIFO_EN */

#ifndef UART4_FIFO_EN
#define UART4_FIFO_EN 0
#endif /* UART4_FIFO_EN */

#ifndef UART5_FIFO_EN
#define UART5_FIFO_EN 0
#endif /* UART5_FIFO_EN */

#ifndef UART6_FIFO_EN
#define UART6_FIFO_EN 0
#endif /* UART6_FIFO_EN */

#ifndef UART7_FIFO_EN
#define UART7_FIFO_EN 0
#endif /* UART7_FIFO_EN */

#ifndef UART8_FIFO_EN
#define UART8_FIFO_EN 0
#endif /* UART8_FIFO_EN */

/* PB2 RS485 发送使能 (TXEN) 控制脚. 使能 USART3 时生效. */
#define RS485_TXEN_GPIO_CLK_ENABLE() __HAL_RCC_GPIOB_CLK_ENABLE()
#define RS485_TXEN_GPIO_PORT GPIOB
#define RS485_TXEN_PIN GPIO_PIN_2

/* 通过 BSRR 单周期设置, 避免读改写, 也方便在中断里调用. */
#define RS485_RX_EN() (RS485_TXEN_GPIO_PORT->BSRR = (uint32_t)RS485_TXEN_PIN << 16U)
#define RS485_TX_EN() (RS485_TXEN_GPIO_PORT->BSRR = RS485_TXEN_PIN)

    /**
     * @brief 端口编号. 与硬件 USART/UART 一一对应, 1 起始.
     */
    typedef enum
    {
        COM1 = 1, /*!< USART1 */
        COM2,     /*!< USART2 */
        COM3,     /*!< USART3 */
        COM4,     /*!< UART4  */
        COM5,     /*!< UART5  */
        COM6,     /*!< USART6 */
        COM7,     /*!< UART7  */
        COM8      /*!< UART8  */
    } COM_PORT_E;

    /* ---------------- 各端口波特率与缓冲区大小 ----------------
     *
     * 注意:
     *  - RX 缓冲区大小会被 find_PowerOf2(_, 0) 向下取整到 2 的幂.
     *    实际生效大小 = find_PowerOf2(UARTx_RX_BUF_SIZE, 0), 因此原始值
     *    建议直接配置成 2 的幂, 否则末尾不足一个 2 的幂的部分不会被 DMA 使用.
     *  - 缓冲区强制 32 字节对齐, 适配 STM32H7 D-Cache line.
     */

#if UART1_FIFO_EN == 1
#ifndef UART1_BAUD
#define UART1_BAUD 115200
#endif /* UART1_BAUD */

#ifndef UART1_TX_BUF_SIZE
#define UART1_TX_BUF_SIZE (8 * 1024)
#endif /* UART1_TX_BUF_SIZE */

#ifndef UART1_RX_BUF_SIZE
#define UART1_RX_BUF_SIZE (1 * 1024)
#endif /* UART1_RX_BUF_SIZE */

    extern UART_HandleTypeDef huart1;
    extern DMA_HandleTypeDef hdma_usart1_tx;
    extern DMA_HandleTypeDef hdma_usart1_rx;
#endif /* UART1_FIFO_EN == 1 */

#if UART2_FIFO_EN == 1
#ifndef UART2_BAUD
#define UART2_BAUD 115200
#endif /* UART2_BAUD */

#ifndef UART2_TX_BUF_SIZE
#define UART2_TX_BUF_SIZE (1 * 1024)
#endif /* UART2_TX_BUF_SIZE */

#ifndef UART2_RX_BUF_SIZE
#define UART2_RX_BUF_SIZE (1 * 1024)
#endif /* UART2_RX_BUF_SIZE */

    extern UART_HandleTypeDef huart2;
    extern DMA_HandleTypeDef hdma_usart2_tx;
    extern DMA_HandleTypeDef hdma_usart2_rx;
#endif /* UART2_FIFO_EN == 1 */

#if UART3_FIFO_EN == 1
#ifndef UART3_BAUD
#define UART3_BAUD 115200
#endif /* UART3_BAUD */

#ifndef UART3_TX_BUF_SIZE
#define UART3_TX_BUF_SIZE (1 * 1024)
#endif /* UART3_TX_BUF_SIZE */

#ifndef UART3_RX_BUF_SIZE
#define UART3_RX_BUF_SIZE (1 * 1024)
#endif /* UART3_RX_BUF_SIZE */

    extern UART_HandleTypeDef huart3;
    extern DMA_HandleTypeDef hdma_usart3_tx;
    extern DMA_HandleTypeDef hdma_usart3_rx;
#endif /* UART3_FIFO_EN == 1 */

#if UART4_FIFO_EN == 1
#ifndef UART4_BAUD
#define UART4_BAUD 115200
#endif /* UART4_BAUD */

#ifndef UART4_TX_BUF_SIZE
#define UART4_TX_BUF_SIZE (1 * 1024)
#endif /* UART4_TX_BUF_SIZE */

#ifndef UART4_RX_BUF_SIZE
#define UART4_RX_BUF_SIZE (1 * 1024)
#endif /* UART4_RX_BUF_SIZE */

    extern UART_HandleTypeDef huart4;
    extern DMA_HandleTypeDef hdma_usart4_tx;
    extern DMA_HandleTypeDef hdma_usart4_rx;
#endif /* UART4_FIFO_EN == 1 */

#if UART5_FIFO_EN == 1
#ifndef UART5_BAUD
#define UART5_BAUD 115200
#endif /* UART5_BAUD */

#ifndef UART5_TX_BUF_SIZE
#define UART5_TX_BUF_SIZE (1 * 1024)
#endif /* UART5_TX_BUF_SIZE */

#ifndef UART5_RX_BUF_SIZE
#define UART5_RX_BUF_SIZE (1 * 1024)
#endif /* UART5_RX_BUF_SIZE */

    extern UART_HandleTypeDef huart5;
    extern DMA_HandleTypeDef hdma_usart5_tx;
    extern DMA_HandleTypeDef hdma_usart5_rx;
#endif /* UART5_FIFO_EN == 1 */

#if UART6_FIFO_EN == 1
#ifndef UART6_BAUD
#define UART6_BAUD 115200
#endif /* UART6_BAUD */

#ifndef UART6_TX_BUF_SIZE
#define UART6_TX_BUF_SIZE (1 * 1024)
#endif /* UART6_TX_BUF_SIZE */

#ifndef UART6_RX_BUF_SIZE
#define UART6_RX_BUF_SIZE (1 * 1024)
#endif /* UART6_RX_BUF_SIZE */

    extern UART_HandleTypeDef huart6;
    extern DMA_HandleTypeDef hdma_usart6_tx;
    extern DMA_HandleTypeDef hdma_usart6_rx;
#endif /* UART6_FIFO_EN == 1 */

#if UART7_FIFO_EN == 1
#ifndef UART7_BAUD
#define UART7_BAUD 115200
#endif /* UART7_BAUD */

#ifndef UART7_TX_BUF_SIZE
#define UART7_TX_BUF_SIZE (1 * 1024)
#endif /* UART7_TX_BUF_SIZE */

#ifndef UART7_RX_BUF_SIZE
#define UART7_RX_BUF_SIZE (1 * 1024)
#endif /* UART7_RX_BUF_SIZE */

    extern UART_HandleTypeDef huart7;
    extern DMA_HandleTypeDef hdma_usart7_tx;
    extern DMA_HandleTypeDef hdma_usart7_rx;
#endif /* UART7_FIFO_EN == 1 */

#if UART8_FIFO_EN == 1
#ifndef UART8_BAUD
#define UART8_BAUD 115200
#endif /* UART8_BAUD */

#ifndef UART8_TX_BUF_SIZE
#define UART8_TX_BUF_SIZE (1 * 1024)
#endif /* UART8_TX_BUF_SIZE */

#ifndef UART8_RX_BUF_SIZE
#define UART8_RX_BUF_SIZE (1 * 1024)
#endif /* UART8_RX_BUF_SIZE */

    extern UART_HandleTypeDef huart8;
    extern DMA_HandleTypeDef hdma_usart8_tx;
    extern DMA_HandleTypeDef hdma_usart8_rx;
#endif /* UART8_FIFO_EN == 1 */

    /**
     * @brief 串口设备运行时上下文.
     *
     *  - tx_kfifo / rx_kfifo : DMA 直接读写的 ring buffer.
     *  - SendBefor / SendOver: TX 钩子 (典型用于 RS485 收发方向切换).
     *  - ReciveNew           : RX 钩子, 每次 idle-line 事件后回调, 参数为本次新收字节数.
     *  - Sending             : 当前是否有 TX DMA 正在进行 (volatile, 跨 ISR/线程).
     */
    typedef struct
    {
        UART_HandleTypeDef *huart;         /*!< 关联的 HAL UART handle 指针 */
        void (*SendBefor)(void);           /*!< 发送前回调, 可选 */
        void (*SendOver)(void);            /*!< 发送完成回调, 可选 */
        void (*ReciveNew)(uint8_t length); /*!< 收到新数据后回调, 可选 */
        RINGBUFF_T tx_kfifo;               /*!< 发送 ring buffer */
        RINGBUFF_T rx_kfifo;               /*!< 接收 ring buffer */
        volatile uint8_t Sending;          /*!< TX DMA 是否在传输中 (1=忙, 0=空闲) */
    } UART_T;

    /* -------------------------------- 公共 API -------------------------------- */

    /**
     * @brief  初始化全部启用的串口 (变量 + 硬件 + 启动 RX DMA + RS485 GPIO).
     * @note   只需在 bsp_Init 中调用一次. 调用前需先初始化 DMA 时钟 (bsp_Init_dma).
     */
    void bsp_InitUart(void);

    /**
     * @brief  释放全部启用的串口的硬件资源 (DMA / GPIO / NVIC / 外设时钟).
     * @note   仅当确实需要切换电源域 / 重新配置硬件时调用.
     */
    void bsp_DeInitUart(void);

    /**
     * @brief  非阻塞发送一段缓冲区到指定串口.
     * @note   数据被拷贝到 tx ring buffer 后立即返回, 实际由 DMA 后台搬运.
     *         若 ring buffer 空间不足, 多余字节会被丢弃 (返回值反映被接收的字节数).
     * @param  port  端口号 (COM1..COM8)
     * @param  buf   待发送数据
     * @param  len   字节数
     * @retval 实际入队的字节数; port 无效时返回 0.
     */
    uint16_t comSendBuf(COM_PORT_E port, const uint8_t *buf, uint16_t len);

    /**
     * @brief  发送单字节到指定串口.
     * @note   底层走 ring buffer + DMA 回调链.
     */
    uint16_t comSendChar(COM_PORT_E port, uint8_t ch);

    /**
     * @brief  从 RX ring buffer 中取一个字节, 非阻塞.
     * @param  port  端口号
     * @param  out   输出字节地址
     * @retval 1 = 取到一个字节; 0 = 缓冲区空 / 端口无效.
     */
    uint8_t comGetChar(COM_PORT_E port, uint8_t *out);

    /**
     * @brief  从 RX ring buffer 中批量取数据, 非阻塞.
     * @retval 实际拷贝的字节数 (可能小于 len).
     */
    uint16_t comGetBuf(COM_PORT_E port, uint8_t *out, uint16_t len);

    /**
     * @brief  清空 TX ring buffer (尚未送出的数据将被丢弃).
     */
    void comClearTxFifo(COM_PORT_E port);

    /**
     * @brief  清空 RX ring buffer.
     */
    void comClearRxFifo(COM_PORT_E port);

    /**
     * @brief  动态切换波特率, 仅修改 BRR 寄存器, 不重新初始化外设.
     * @param  port  端口号
     * @param  baud  新波特率 (Hz)
     * @retval 0 成功, 非 0 失败.
     */
    int comSetBaud(COM_PORT_E port, uint32_t baud);

    /**
     * @brief  返回 RX ring buffer 内已积累的字节数.
     */
    uint16_t comGetLen(COM_PORT_E port);

    /**
     * @brief  通过 RS485 (USART3) 发送一段数据.
     */
    void RS485_SendBuf(const uint8_t *buf, uint16_t len);

    /**
     * @brief  通过 RS485 (USART3) 发送 0 结尾的字符串.
     */
    void RS485_SendStr(const char *str);

    /**
     * @brief  设置 RS485 (USART3) 的波特率.
     */
    void RS485_SetBaud(uint32_t baud);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
/******************** End of file ********************/
