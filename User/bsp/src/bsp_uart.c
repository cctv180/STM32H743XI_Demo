/**
 *********************************************************************************************************
 * @file    bsp_uart.c
 * @brief   多路 UART/USART + DMA + Ring Buffer 驱动
 *
 *  - HAL_UART_MspInit / HAL_UART_MspDeInit 保持 CubeMX 生成的结构 (per-instance
 *    if/else), 直接覆盖 HAL 弱符号. CubeMX 重新生成时, 只需把对应分支拷贝到
 *    本文件即可完成迁移. 原 CubeMX 文件 (Src/main.c, Src/stm32h7xx_hal_msp.c)
 *    在 MDK-ARM 工程中 IncludeInBuild=0, 仅作迁移参考.
 *
 *  - 各端口的 MX_USARTx_UART_Init 仅是薄壳, 都调用同一个 uart_basic_init 完成
 *    寄存器配置; 这样既保留了 CubeMX 的命名风格, 又消除了冗余拷贝.
 *
 *  - TX 路径: ringbuffer_put -> HAL_UART_Transmit_DMA -> TxCplt 中链式投递
 *    剩余数据. read_index 在启动 DMA 之前更新, 避免 DMA 极快完成时回调读到
 *    陈旧的 read_index 而重复发送.
 *
 *  - RX 路径: 启动 HAL_UARTEx_ReceiveToIdle_DMA (循环 DMA + idle 事件), 在
 *    HAL_UARTEx_RxEventCallback 中由当前 DMA 写指针推进 ring buffer write_index.
 *    应用层在 comGetChar / comGetBuf 中执行 D-Cache invalidate, 保证 CPU 读
 *    到 DMA 已经写入 SRAM 的最新数据.
 *
 *  - 缓冲区按 32 字节对齐 (D-Cache line). RX 缓冲区大小必须为 2 的幂
 *    (ringbuffer 借助 buffer_size-1 做掩码), 否则会越界. 配置宏建议直接写
 *    成 2^N (默认 1024 / 8192 已满足).
 *
 * Copyright (C) Project Contributors. All rights reserved.
 *********************************************************************************************************
 */
#include "bsp.h"
#include "bsp_uart.h"
#include "ring_buffer.h"

/* ========================================================================== */
/*                                Private defines                              */
/* ========================================================================== */

/* HAL UART_SetConfig 中的 BRR 边界, 复用以保证 comSetBaud 行为与 HAL 完全一致. */
#define LPUART_BRR_MIN 0x00000300U
#define LPUART_BRR_MAX 0x000FFFFFU
#define UART_BRR_MIN 0x00000010U
#define UART_BRR_MAX 0x0000FFFFU

/* RX DMA 缓冲对齐(32 字节, 适配 STM32H7 D-Cache line 长度). */
#define BSP_UART_BUF_ALIGN __attribute__((aligned(32)))

/* ========================================================================== */
/*                                 Forward decl                                */
/* ========================================================================== */

static UART_T *port_to_uart(COM_PORT_E port);
static UART_T *instance_to_uart(const USART_TypeDef *base);
static void uart_basic_init(UART_HandleTypeDef *huart, USART_TypeDef *instance, uint32_t baud);
static void uart_kickoff_tx(UART_T *p);

static void RS485_InitTXE(void);
static void RS485_SendBefor(void);
static void RS485_SendOver(void);
static void RS485_ReciveNew(uint8_t byte);

/* ========================================================================== */
/*                          Per-port state allocations                         */
/* ========================================================================== */

#if UART1_FIFO_EN == 1
UART_HandleTypeDef huart1;
DMA_HandleTypeDef hdma_usart1_tx;
DMA_HandleTypeDef hdma_usart1_rx;
static UART_T s_uart1;
BSP_UART_BUF_ALIGN static uint8_t s_tx_buf1[UART1_TX_BUF_SIZE];
BSP_UART_BUF_ALIGN static uint8_t s_rx_buf1[UART1_RX_BUF_SIZE];
#endif

#if UART2_FIFO_EN == 1
UART_HandleTypeDef huart2;
DMA_HandleTypeDef hdma_usart2_tx;
DMA_HandleTypeDef hdma_usart2_rx;
static UART_T s_uart2;
BSP_UART_BUF_ALIGN static uint8_t s_tx_buf2[UART2_TX_BUF_SIZE];
BSP_UART_BUF_ALIGN static uint8_t s_rx_buf2[UART2_RX_BUF_SIZE];
#endif

#if UART3_FIFO_EN == 1
UART_HandleTypeDef huart3;
DMA_HandleTypeDef hdma_usart3_tx;
DMA_HandleTypeDef hdma_usart3_rx;
static UART_T s_uart3;
BSP_UART_BUF_ALIGN static uint8_t s_tx_buf3[UART3_TX_BUF_SIZE];
BSP_UART_BUF_ALIGN static uint8_t s_rx_buf3[UART3_RX_BUF_SIZE];
#endif

#if UART4_FIFO_EN == 1
UART_HandleTypeDef huart4;
DMA_HandleTypeDef hdma_usart4_tx;
DMA_HandleTypeDef hdma_usart4_rx;
static UART_T s_uart4;
BSP_UART_BUF_ALIGN static uint8_t s_tx_buf4[UART4_TX_BUF_SIZE];
BSP_UART_BUF_ALIGN static uint8_t s_rx_buf4[UART4_RX_BUF_SIZE];
#endif

#if UART5_FIFO_EN == 1
UART_HandleTypeDef huart5;
DMA_HandleTypeDef hdma_usart5_tx;
DMA_HandleTypeDef hdma_usart5_rx;
static UART_T s_uart5;
BSP_UART_BUF_ALIGN static uint8_t s_tx_buf5[UART5_TX_BUF_SIZE];
BSP_UART_BUF_ALIGN static uint8_t s_rx_buf5[UART5_RX_BUF_SIZE];
#endif

#if UART6_FIFO_EN == 1
UART_HandleTypeDef huart6;
DMA_HandleTypeDef hdma_usart6_tx;
DMA_HandleTypeDef hdma_usart6_rx;
static UART_T s_uart6;
BSP_UART_BUF_ALIGN static uint8_t s_tx_buf6[UART6_TX_BUF_SIZE];
BSP_UART_BUF_ALIGN static uint8_t s_rx_buf6[UART6_RX_BUF_SIZE];
#endif

#if UART7_FIFO_EN == 1
UART_HandleTypeDef huart7;
DMA_HandleTypeDef hdma_usart7_tx;
DMA_HandleTypeDef hdma_usart7_rx;
static UART_T s_uart7;
BSP_UART_BUF_ALIGN static uint8_t s_tx_buf7[UART7_TX_BUF_SIZE];
BSP_UART_BUF_ALIGN static uint8_t s_rx_buf7[UART7_RX_BUF_SIZE];
#endif

#if UART8_FIFO_EN == 1
UART_HandleTypeDef huart8;
DMA_HandleTypeDef hdma_usart8_tx;
DMA_HandleTypeDef hdma_usart8_rx;
static UART_T s_uart8;
BSP_UART_BUF_ALIGN static uint8_t s_tx_buf8[UART8_TX_BUF_SIZE];
BSP_UART_BUF_ALIGN static uint8_t s_rx_buf8[UART8_RX_BUF_SIZE];
#endif

/* ========================================================================== */
/*                                 Lookup helpers                              */
/* ========================================================================== */

/**
 * @brief COM_PORT_E -> UART_T*. 返回 NULL 表示该端口未启用或无效.
 */
static UART_T *port_to_uart(COM_PORT_E port)
{
    switch (port)
    {
#if UART1_FIFO_EN == 1
    case COM1:
        return &s_uart1;
#endif
#if UART2_FIFO_EN == 1
    case COM2:
        return &s_uart2;
#endif
#if UART3_FIFO_EN == 1
    case COM3:
        return &s_uart3;
#endif
#if UART4_FIFO_EN == 1
    case COM4:
        return &s_uart4;
#endif
#if UART5_FIFO_EN == 1
    case COM5:
        return &s_uart5;
#endif
#if UART6_FIFO_EN == 1
    case COM6:
        return &s_uart6;
#endif
#if UART7_FIFO_EN == 1
    case COM7:
        return &s_uart7;
#endif
#if UART8_FIFO_EN == 1
    case COM8:
        return &s_uart8;
#endif
    default:
        return NULL;
    }
}

/**
 * @brief 由外设基地址定位 UART_T (用于 HAL 回调).
 */
static UART_T *instance_to_uart(const USART_TypeDef *base)
{
#if UART1_FIFO_EN == 1
    if (base == USART1)
        return &s_uart1;
#endif
#if UART2_FIFO_EN == 1
    if (base == USART2)
        return &s_uart2;
#endif
#if UART3_FIFO_EN == 1
    if (base == USART3)
        return &s_uart3;
#endif
#if UART4_FIFO_EN == 1
    if (base == UART4)
        return &s_uart4;
#endif
#if UART5_FIFO_EN == 1
    if (base == UART5)
        return &s_uart5;
#endif
#if UART6_FIFO_EN == 1
    if (base == USART6)
        return &s_uart6;
#endif
#if UART7_FIFO_EN == 1
    if (base == UART7)
        return &s_uart7;
#endif
#if UART8_FIFO_EN == 1
    if (base == UART8)
        return &s_uart8;
#endif
    (void)base;
    return NULL;
}

/* ========================================================================== */
/*                       Common HAL UART config (基础参数)                      */
/* ========================================================================== */

/**
 * @brief 用统一的默认参数初始化 UART_HandleTypeDef.
 *
 * CubeMX 默认生成的 MX_USARTx_UART_Init 在每个端口都重复同样的字段, 仅
 * Instance / BaudRate 不同. 这里把公共部分集中, 命名仍保留 MX_USARTx_UART_Init,
 * 让 CubeMX 重新生成代码时仍能按文件名定位差异.
 */
static void uart_basic_init(UART_HandleTypeDef *huart, USART_TypeDef *instance, uint32_t baud)
{
    huart->Instance = instance;
    huart->Init.BaudRate = baud;
    huart->Init.WordLength = UART_WORDLENGTH_8B;
    huart->Init.StopBits = UART_STOPBITS_1;
    huart->Init.Parity = UART_PARITY_NONE;
    huart->Init.Mode = UART_MODE_TX_RX;
    huart->Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart->Init.OverSampling = UART_OVERSAMPLING_16;
    huart->Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
    huart->Init.ClockPrescaler = UART_PRESCALER_DIV1;
    huart->AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;

    if (HAL_UART_Init(huart) != HAL_OK)
    {
        ERROR_HANDLER();
    }
    if (HAL_UARTEx_SetTxFifoThreshold(huart, UART_TXFIFO_THRESHOLD_1_8) != HAL_OK)
    {
        ERROR_HANDLER();
    }
    if (HAL_UARTEx_SetRxFifoThreshold(huart, UART_RXFIFO_THRESHOLD_1_8) != HAL_OK)
    {
        ERROR_HANDLER();
    }
    if (HAL_UARTEx_DisableFifoMode(huart) != HAL_OK)
    {
        ERROR_HANDLER();
    }
}

#if UART1_FIFO_EN == 1
static void MX_USART1_UART_Init(void) { uart_basic_init(&huart1, USART1, UART1_BAUD); }
#endif
#if UART3_FIFO_EN == 1
static void MX_USART3_UART_Init(void) { uart_basic_init(&huart3, USART3, UART3_BAUD); }
#endif
#if UART6_FIFO_EN == 1
static void MX_USART6_UART_Init(void) { uart_basic_init(&huart6, USART6, UART6_BAUD); }
#endif

/* ========================================================================== */
/*                  HAL_UART_MspInit (CubeMX 风格, 直接覆盖弱符号)                */
/* ========================================================================== */

/**
 * @brief Initializes the UART MSP.
 *
 * 与 STM32CubeMX 生成的 stm32h7xx_hal_msp.c 内容保持一致, 便于直接对照迁移.
 */
void HAL_UART_MspInit(UART_HandleTypeDef *huart)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};

#if UART1_FIFO_EN == 1
    if (huart->Instance == USART1)
    {
        PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_USART1;
        PeriphClkInitStruct.Usart16ClockSelection = RCC_USART16CLKSOURCE_D2PCLK2;
        if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
        {
            ERROR_HANDLER();
        }

        __HAL_RCC_USART1_CLK_ENABLE();
        __HAL_RCC_GPIOA_CLK_ENABLE();

        /* PA9 -> USART1_TX, PA10 -> USART1_RX */
        GPIO_InitStruct.Pin = GPIO_PIN_9 | GPIO_PIN_10;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
        GPIO_InitStruct.Alternate = GPIO_AF7_USART1;
        HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

        /* USART1_RX -> DMA1 Stream0 */
        hdma_usart1_rx.Instance = DMA1_Stream0;
        hdma_usart1_rx.Init.Request = DMA_REQUEST_USART1_RX;
        hdma_usart1_rx.Init.Direction = DMA_PERIPH_TO_MEMORY;
        hdma_usart1_rx.Init.PeriphInc = DMA_PINC_DISABLE;
        hdma_usart1_rx.Init.MemInc = DMA_MINC_ENABLE;
        hdma_usart1_rx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
        hdma_usart1_rx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
        hdma_usart1_rx.Init.Mode = DMA_CIRCULAR;
        hdma_usart1_rx.Init.Priority = DMA_PRIORITY_LOW;
        hdma_usart1_rx.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
        if (HAL_DMA_Init(&hdma_usart1_rx) != HAL_OK)
        {
            ERROR_HANDLER();
        }
        __HAL_LINKDMA(huart, hdmarx, hdma_usart1_rx);

        /* USART1_TX -> DMA1 Stream1 */
        hdma_usart1_tx.Instance = DMA1_Stream1;
        hdma_usart1_tx.Init.Request = DMA_REQUEST_USART1_TX;
        hdma_usart1_tx.Init.Direction = DMA_MEMORY_TO_PERIPH;
        hdma_usart1_tx.Init.PeriphInc = DMA_PINC_DISABLE;
        hdma_usart1_tx.Init.MemInc = DMA_MINC_ENABLE;
        hdma_usart1_tx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
        hdma_usart1_tx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
        hdma_usart1_tx.Init.Mode = DMA_NORMAL;
        hdma_usart1_tx.Init.Priority = DMA_PRIORITY_LOW;
        hdma_usart1_tx.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
        if (HAL_DMA_Init(&hdma_usart1_tx) != HAL_OK)
        {
            ERROR_HANDLER();
        }
        __HAL_LINKDMA(huart, hdmatx, hdma_usart1_tx);

        HAL_NVIC_SetPriority(USART1_IRQn, 0, 0);
        HAL_NVIC_EnableIRQ(USART1_IRQn);
        return;
    }
#endif

#if UART3_FIFO_EN == 1
    if (huart->Instance == USART3)
    {
        PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_USART3;
        PeriphClkInitStruct.Usart234578ClockSelection = RCC_USART234578CLKSOURCE_D2PCLK1;
        if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
        {
            ERROR_HANDLER();
        }

        __HAL_RCC_USART3_CLK_ENABLE();
        __HAL_RCC_GPIOB_CLK_ENABLE();

        /* PB10 -> USART3_TX, PB11 -> USART3_RX */
        GPIO_InitStruct.Pin = GPIO_PIN_10 | GPIO_PIN_11;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
        GPIO_InitStruct.Alternate = GPIO_AF7_USART3;
        HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

        /* USART3_RX -> DMA1 Stream2 */
        hdma_usart3_rx.Instance = DMA1_Stream2;
        hdma_usart3_rx.Init.Request = DMA_REQUEST_USART3_RX;
        hdma_usart3_rx.Init.Direction = DMA_PERIPH_TO_MEMORY;
        hdma_usart3_rx.Init.PeriphInc = DMA_PINC_DISABLE;
        hdma_usart3_rx.Init.MemInc = DMA_MINC_ENABLE;
        hdma_usart3_rx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
        hdma_usart3_rx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
        hdma_usart3_rx.Init.Mode = DMA_CIRCULAR;
        hdma_usart3_rx.Init.Priority = DMA_PRIORITY_LOW;
        hdma_usart3_rx.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
        if (HAL_DMA_Init(&hdma_usart3_rx) != HAL_OK)
        {
            ERROR_HANDLER();
        }
        __HAL_LINKDMA(huart, hdmarx, hdma_usart3_rx);

        /* USART3_TX -> DMA1 Stream3 */
        hdma_usart3_tx.Instance = DMA1_Stream3;
        hdma_usart3_tx.Init.Request = DMA_REQUEST_USART3_TX;
        hdma_usart3_tx.Init.Direction = DMA_MEMORY_TO_PERIPH;
        hdma_usart3_tx.Init.PeriphInc = DMA_PINC_DISABLE;
        hdma_usart3_tx.Init.MemInc = DMA_MINC_ENABLE;
        hdma_usart3_tx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
        hdma_usart3_tx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
        hdma_usart3_tx.Init.Mode = DMA_NORMAL;
        hdma_usart3_tx.Init.Priority = DMA_PRIORITY_LOW;
        hdma_usart3_tx.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
        if (HAL_DMA_Init(&hdma_usart3_tx) != HAL_OK)
        {
            ERROR_HANDLER();
        }
        __HAL_LINKDMA(huart, hdmatx, hdma_usart3_tx);

        HAL_NVIC_SetPriority(USART3_IRQn, 0, 0);
        HAL_NVIC_EnableIRQ(USART3_IRQn);
        return;
    }
#endif

#if UART6_FIFO_EN == 1
    if (huart->Instance == USART6)
    {
        PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_USART6;
        PeriphClkInitStruct.Usart16ClockSelection = RCC_USART16CLKSOURCE_D2PCLK2;
        if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
        {
            ERROR_HANDLER();
        }

        __HAL_RCC_USART6_CLK_ENABLE();
        __HAL_RCC_GPIOG_CLK_ENABLE();
        __HAL_RCC_GPIOC_CLK_ENABLE();

        /* PG14 -> USART6_TX */
        GPIO_InitStruct.Pin = GPIO_PIN_14;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
        GPIO_InitStruct.Alternate = GPIO_AF7_USART6;
        HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);

        /* PC7 -> USART6_RX */
        GPIO_InitStruct.Pin = GPIO_PIN_7;
        HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

        /* USART6_RX -> DMA1 Stream4 */
        hdma_usart6_rx.Instance = DMA1_Stream4;
        hdma_usart6_rx.Init.Request = DMA_REQUEST_USART6_RX;
        hdma_usart6_rx.Init.Direction = DMA_PERIPH_TO_MEMORY;
        hdma_usart6_rx.Init.PeriphInc = DMA_PINC_DISABLE;
        hdma_usart6_rx.Init.MemInc = DMA_MINC_ENABLE;
        hdma_usart6_rx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
        hdma_usart6_rx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
        hdma_usart6_rx.Init.Mode = DMA_CIRCULAR;
        hdma_usart6_rx.Init.Priority = DMA_PRIORITY_LOW;
        hdma_usart6_rx.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
        if (HAL_DMA_Init(&hdma_usart6_rx) != HAL_OK)
        {
            ERROR_HANDLER();
        }
        __HAL_LINKDMA(huart, hdmarx, hdma_usart6_rx);

        /* USART6_TX -> DMA1 Stream5 */
        hdma_usart6_tx.Instance = DMA1_Stream5;
        hdma_usart6_tx.Init.Request = DMA_REQUEST_USART6_TX;
        hdma_usart6_tx.Init.Direction = DMA_MEMORY_TO_PERIPH;
        hdma_usart6_tx.Init.PeriphInc = DMA_PINC_DISABLE;
        hdma_usart6_tx.Init.MemInc = DMA_MINC_ENABLE;
        hdma_usart6_tx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
        hdma_usart6_tx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
        hdma_usart6_tx.Init.Mode = DMA_NORMAL;
        hdma_usart6_tx.Init.Priority = DMA_PRIORITY_LOW;
        hdma_usart6_tx.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
        if (HAL_DMA_Init(&hdma_usart6_tx) != HAL_OK)
        {
            ERROR_HANDLER();
        }
        __HAL_LINKDMA(huart, hdmatx, hdma_usart6_tx);

        HAL_NVIC_SetPriority(USART6_IRQn, 0, 0);
        HAL_NVIC_EnableIRQ(USART6_IRQn);
        return;
    }
#endif
}

/**
 * @brief De-Initializes the UART MSP.
 */
void HAL_UART_MspDeInit(UART_HandleTypeDef *huart)
{
#if UART1_FIFO_EN == 1
    if (huart->Instance == USART1)
    {
        __HAL_RCC_USART1_CLK_DISABLE();
        HAL_GPIO_DeInit(GPIOA, GPIO_PIN_9 | GPIO_PIN_10);
        HAL_DMA_DeInit(huart->hdmarx);
        HAL_DMA_DeInit(huart->hdmatx);
        HAL_NVIC_DisableIRQ(USART1_IRQn);
        return;
    }
#endif
#if UART3_FIFO_EN == 1
    if (huart->Instance == USART3)
    {
        __HAL_RCC_USART3_CLK_DISABLE();
        HAL_GPIO_DeInit(GPIOB, GPIO_PIN_10 | GPIO_PIN_11);
        HAL_DMA_DeInit(huart->hdmarx);
        HAL_DMA_DeInit(huart->hdmatx);
        HAL_NVIC_DisableIRQ(USART3_IRQn);
        return;
    }
#endif
#if UART6_FIFO_EN == 1
    if (huart->Instance == USART6)
    {
        __HAL_RCC_USART6_CLK_DISABLE();
        HAL_GPIO_DeInit(GPIOG, GPIO_PIN_14);
        HAL_GPIO_DeInit(GPIOC, GPIO_PIN_7);
        HAL_DMA_DeInit(huart->hdmarx);
        HAL_DMA_DeInit(huart->hdmatx);
        HAL_NVIC_DisableIRQ(USART6_IRQn);
        return;
    }
#endif
}

/* ========================================================================== */
/*                                HAL callbacks                                */
/* ========================================================================== */

/**
 * @brief 由 HAL 在 RX idle / 半满 / 满 时调用. Size 是 DMA 当前在缓冲区中的写入位置 (0-based).
 *
 *  实际 DMA 是循环模式, ringbuffer 的 buffer_ptr 与 DMA target 是同一段内存.
 *  本函数只负责让 ring buffer 的 write_index 跟上 DMA 真实写指针, 并维护
 *  mirror bit / 必要时回退 read_index 以避免覆盖未取走的数据.
 */
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    UART_T *p = instance_to_uart(huart->Instance);
    if (p == NULL)
    {
        return;
    }

    const uint16_t mask = p->rx_kfifo.buffer_size - 1U;
    const uint16_t index_new = (uint16_t)(Size & mask);
    const uint16_t length = (uint16_t)((index_new - p->rx_kfifo.write_index) & mask);
    const uint16_t space = ringbuffer_space_len(&p->rx_kfifo);

    /* 是否跨过了一圈 (write_index 走过了 buffer 末尾) */
    if (p->rx_kfifo.write_index > index_new)
    {
        p->rx_kfifo.write_mirror = ~p->rx_kfifo.write_mirror;
        if (length > space)
        {
            p->rx_kfifo.read_mirror = ~p->rx_kfifo.read_mirror;
        }
    }
    /* 缓冲区满, 主动丢弃最早的数据 (read_index 跟随 write_index) */
    if (length > space)
    {
        p->rx_kfifo.read_index = index_new;
    }
    p->rx_kfifo.write_index = index_new;

    if (p->ReciveNew)
    {
        p->ReciveNew((uint8_t)length);
    }
}

/**
 * @brief TX DMA 单次完成回调. 在此处自动投递 ring buffer 中剩余的数据.
 */
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    UART_T *p = instance_to_uart(huart->Instance);
    if (p == NULL)
    {
        return;
    }

    if (ringbuffer_data_len(&p->tx_kfifo) == 0U)
    {
        if (p->SendOver)
        {
            p->SendOver(); /* 例: RS485 切回接收 */
        }
        p->Sending = FALSE;
        return;
    }

    uart_kickoff_tx(p);
}

/**
 * @brief UART 错误回调.
 *
 *  常见错误源:
 *    - ORE (Overrun): RX FIFO 来不及取走, 通常 idle-line DMA 模式下不会出现,
 *      仅当中断响应被长时间挡住才偶发.
 *    - PE / FE / NE  : 波特率失配 / 噪声 / 帧错误.
 *    - DMA error     : DMA 总线故障 (极少).
 *    - RTO           : Receiver Timeout (启用了 USART_CR2_RTOEN 才会触发).
 *
 *  HAL 在调用本回调之前已经清掉对应错误标志、停掉 RX DMA 并把 gState 置回
 *  READY (参见 stm32h7xx_hal_uart.c 的 UART_DMAError / UART_DMAReceiveCplt
 *  错误分支). 本函数只需把端口状态 (ring buffer / Sending) 复位, 重新装载
 *  循环 DMA 让通道自动恢复.
 */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    UART_T *p = instance_to_uart(huart->Instance);
    if (p == NULL)
    {
        return;
    }

    BSP_Printf("UART error, ErrorCode=0x%08lX", (unsigned long)HAL_UART_GetError(huart));

    /* TX 侧: 出错时 TxCplt 不会再来, 这里强制解锁 Sending, 否则下次
     * comSendBuf 会把数据放进 ring buffer 但永远等不到 DMA 启动. */
    p->Sending = FALSE;

    /* RX 侧: HAL 已停 DMA, 清空 ring buffer 后重新启动 idle-line DMA.
     * 失败也只是日志一下, 下一次 ErrorCallback 会再次尝试. */
    ringbuffer_reset(&p->rx_kfifo);
    if (HAL_UARTEx_ReceiveToIdle_DMA(huart,
                                     p->rx_kfifo.buffer_ptr,
                                     p->rx_kfifo.buffer_size) != HAL_OK)
    {
        BSP_Printf("UART rx-DMA restart failed");
    }
}

/* ========================================================================== */
/*                                IRQ handlers                                 */
/* ========================================================================== */

#if UART1_FIFO_EN == 1
void USART1_IRQHandler(void) { HAL_UART_IRQHandler(&huart1); }
#endif
#if UART3_FIFO_EN == 1
void USART3_IRQHandler(void) { HAL_UART_IRQHandler(&huart3); }
#endif
#if UART6_FIFO_EN == 1
void USART6_IRQHandler(void) { HAL_UART_IRQHandler(&huart6); }
#endif

/* ========================================================================== */
/*                              TX kick-off helper                             */
/* ========================================================================== */

/**
 * @brief 从 tx ring buffer 中取出连续一段, 启动一次 TX DMA.
 *
 *  顺序:
 *    1. 计算本次能连续传输的长度 (考虑 ring buffer 的回绕).
 *    2. 先更新 read_index / mirror 到"传输后"状态.
 *    3. 启动 DMA 读取那段缓冲区.
 *
 *  原版做法是先启动 DMA 再更新 read_index. 当 baud 极高 / len 极小时,
 *  TxCplt 回调可能先于 read_index 自增执行, 导致重复发送. 调换顺序即可
 *  完全规避该竞态.
 */
static void uart_kickoff_tx(UART_T *p)
{
    const uint16_t total = ringbuffer_data_len(&p->tx_kfifo);
    if (total == 0U)
    {
        return;
    }

    const uint16_t start = p->tx_kfifo.read_index;
    const uint16_t to_end = (uint16_t)(p->tx_kfifo.buffer_size - start);
    uint16_t take;

    if (to_end > total)
    {
        take = total;
        p->tx_kfifo.read_index = (uint16_t)(start + take);
    }
    else
    {
        take = to_end;
        p->tx_kfifo.read_mirror = ~p->tx_kfifo.read_mirror;
        p->tx_kfifo.read_index = 0U;
    }

    HAL_UART_Transmit_DMA(p->huart, &p->tx_kfifo.buffer_ptr[start], take);
}

/**
 * @brief 把 buf 中的数据写入 tx ring buffer, 并按需触发 TX DMA.
 * @retval 实际入队的字节数.
 */
static uint16_t uart_send(UART_T *p, const uint8_t *buf, uint16_t len)
{
    /* 注意 ringbuffer_put 接收 const uint8_t*. */
    const uint16_t put = (uint16_t)ringbuffer_put(&p->tx_kfifo, buf, len);

    /* DMA 即将读 tx 缓冲, 把 CPU 写的最新数据刷到 RAM. */
    SCB_CleanDCache_by_Addr((uint32_t *)p->tx_kfifo.buffer_ptr, p->tx_kfifo.buffer_size);

    /* 仅当当前没有 TX 在进行才启动新 DMA, 否则交给 TxCplt 回调链式投递. */
    DISABLE_INT();
    if (p->Sending == FALSE)
    {
        p->Sending = TRUE;
        ENABLE_INT();
        uart_kickoff_tx(p);
    }
    else
    {
        ENABLE_INT();
    }

    return put;
}

/* ========================================================================== */
/*                                  Public API                                 */
/* ========================================================================== */

uint16_t comSendBuf(COM_PORT_E port, const uint8_t *buf, uint16_t len)
{
    UART_T *p = port_to_uart(port);
    if (p == NULL || buf == NULL || len == 0U)
    {
        return 0U;
    }

    if (p->SendBefor)
    {
        p->SendBefor(); /* 例: RS485 切到发送 */
    }
    return uart_send(p, buf, len);
}

uint16_t comSendChar(COM_PORT_E port, uint8_t ch)
{
    return comSendBuf(port, &ch, 1U);
}

uint8_t comGetChar(COM_PORT_E port, uint8_t *out)
{
    UART_T *p = port_to_uart(port);
    if (p == NULL || out == NULL)
    {
        return 0U;
    }
    /* DMA 直接写整个 RX 缓冲, CPU 读取前必须把整段对应 cache 行设为 invalid. */
    SCB_InvalidateDCache_by_Addr((uint32_t *)p->rx_kfifo.buffer_ptr, p->rx_kfifo.buffer_size);
    return (uint8_t)ringbuffer_getchar(&p->rx_kfifo, out);
}

uint16_t comGetBuf(COM_PORT_E port, uint8_t *out, uint16_t len)
{
    UART_T *p = port_to_uart(port);
    if (p == NULL || out == NULL || len == 0U)
    {
        return 0U;
    }
    SCB_InvalidateDCache_by_Addr((uint32_t *)p->rx_kfifo.buffer_ptr, p->rx_kfifo.buffer_size);
    return (uint16_t)ringbuffer_get(&p->rx_kfifo, out, len);
}

void comClearTxFifo(COM_PORT_E port)
{
    UART_T *p = port_to_uart(port);
    if (p == NULL)
    {
        return;
    }
    ringbuffer_reset(&p->tx_kfifo);
}

void comClearRxFifo(COM_PORT_E port)
{
    UART_T *p = port_to_uart(port);
    if (p == NULL)
    {
        return;
    }
    /* 让 read 跟随当前 DMA 写入位置, 等价于"舍弃尚未读取的字节". */
    p->rx_kfifo.read_index = p->rx_kfifo.write_index;
    p->rx_kfifo.read_mirror = 0U;
    p->rx_kfifo.write_mirror = 0U;
}

uint16_t comGetLen(COM_PORT_E port)
{
    UART_T *p = port_to_uart(port);
    if (p == NULL)
    {
        return 0U;
    }
    return (uint16_t)ringbuffer_data_len(&p->rx_kfifo);
}

/* ========================================================================== */
/*                                 comSetBaud                                  */
/* ========================================================================== */

/**
 * @brief 仅修改 BRR 寄存器, 不调用 HAL_UART_Init, 避免重新初始化 GPIO/DMA.
 *
 * 算法直接镜像 stm32xx_hal_uart.c -> UART_SetConfig() 的 BRR 计算分支
 * (LPUART / 8x / 16x oversampling), 与 HAL 输出保持比特一致.
 */
int comSetBaud(COM_PORT_E port, uint32_t baud)
{
    UART_T *p = port_to_uart(port);
    UART_HandleTypeDef *huart;
    UART_ClockSourceTypeDef clocksource;
    PLL2_ClocksTypeDef pll2_clocks;
    PLL3_ClocksTypeDef pll3_clocks;
    uint32_t pclk = 0U;
    uint32_t usartdiv;
    HAL_StatusTypeDef ret = HAL_OK;

    if (p == NULL || baud == 0U)
    {
        return -1;
    }

    huart = p->huart;
    huart->Init.BaudRate = baud;
    UART_GETCLOCKSOURCE(huart, clocksource);

    /* ------------------ LPUART ------------------ */
    if (UART_INSTANCE_LOWPOWER(huart))
    {
        switch (clocksource)
        {
        case UART_CLOCKSOURCE_D3PCLK1:
            pclk = HAL_RCCEx_GetD3PCLK1Freq();
            break;
        case UART_CLOCKSOURCE_PLL2:
            HAL_RCCEx_GetPLL2ClockFreq(&pll2_clocks);
            pclk = pll2_clocks.PLL2_Q_Frequency;
            break;
        case UART_CLOCKSOURCE_PLL3:
            HAL_RCCEx_GetPLL3ClockFreq(&pll3_clocks);
            pclk = pll3_clocks.PLL3_Q_Frequency;
            break;
        case UART_CLOCKSOURCE_HSI:
            pclk = (__HAL_RCC_GET_FLAG(RCC_FLAG_HSIDIV) != 0U)
                       ? (uint32_t)(HSI_VALUE >> (__HAL_RCC_GET_HSI_DIVIDER() >> 3U))
                       : (uint32_t)HSI_VALUE;
            break;
        case UART_CLOCKSOURCE_CSI:
            pclk = (uint32_t)CSI_VALUE;
            break;
        case UART_CLOCKSOURCE_LSE:
            pclk = (uint32_t)LSE_VALUE;
            break;
        default:
            pclk = 0U;
            ret = HAL_ERROR;
            break;
        }

        if (pclk != 0U)
        {
            const uint32_t lpuart_ker = pclk / UARTPrescTable[huart->Init.ClockPrescaler];
            if (lpuart_ker < (3U * baud) || lpuart_ker > (4096U * baud))
            {
                ret = HAL_ERROR;
            }
            else
            {
                usartdiv = (uint32_t)UART_DIV_LPUART(pclk, baud, huart->Init.ClockPrescaler);
                if (usartdiv >= LPUART_BRR_MIN && usartdiv <= LPUART_BRR_MAX)
                {
                    huart->Instance->BRR = usartdiv;
                }
                else
                {
                    ret = HAL_ERROR;
                }
            }
        }
        return (ret == HAL_OK) ? 0 : -2;
    }

    /* ------------------ Standard U(S)ART ------------------ */
    switch (clocksource)
    {
    case UART_CLOCKSOURCE_D2PCLK1:
        pclk = HAL_RCC_GetPCLK1Freq();
        break;
    case UART_CLOCKSOURCE_D2PCLK2:
        pclk = HAL_RCC_GetPCLK2Freq();
        break;
    case UART_CLOCKSOURCE_PLL2:
        HAL_RCCEx_GetPLL2ClockFreq(&pll2_clocks);
        pclk = pll2_clocks.PLL2_Q_Frequency;
        break;
    case UART_CLOCKSOURCE_PLL3:
        HAL_RCCEx_GetPLL3ClockFreq(&pll3_clocks);
        pclk = pll3_clocks.PLL3_Q_Frequency;
        break;
    case UART_CLOCKSOURCE_HSI:
        pclk = (__HAL_RCC_GET_FLAG(RCC_FLAG_HSIDIV) != 0U)
                   ? (uint32_t)(HSI_VALUE >> (__HAL_RCC_GET_HSI_DIVIDER() >> 3U))
                   : (uint32_t)HSI_VALUE;
        break;
    case UART_CLOCKSOURCE_CSI:
        pclk = (uint32_t)CSI_VALUE;
        break;
    case UART_CLOCKSOURCE_LSE:
        pclk = (uint32_t)LSE_VALUE;
        break;
    default:
        pclk = 0U;
        ret = HAL_ERROR;
        break;
    }
    if (pclk == 0U)
    {
        return -3;
    }

    if (huart->Init.OverSampling == UART_OVERSAMPLING_8)
    {
        usartdiv = (uint32_t)UART_DIV_SAMPLING8(pclk, baud, huart->Init.ClockPrescaler);
        if (usartdiv >= UART_BRR_MIN && usartdiv <= UART_BRR_MAX)
        {
            uint16_t brr = (uint16_t)(usartdiv & 0xFFF0U);
            brr |= (uint16_t)((usartdiv & 0x000FU) >> 1U);
            huart->Instance->BRR = brr;
        }
        else
        {
            ret = HAL_ERROR;
        }
    }
    else
    {
        usartdiv = (uint32_t)UART_DIV_SAMPLING16(pclk, baud, huart->Init.ClockPrescaler);
        if (usartdiv >= UART_BRR_MIN && usartdiv <= UART_BRR_MAX)
        {
            huart->Instance->BRR = (uint16_t)usartdiv;
        }
        else
        {
            ret = HAL_ERROR;
        }
    }

    return (ret == HAL_OK) ? 0 : -4;
}

/* ========================================================================== */
/*                                  RS485 stub                                 */
/* ========================================================================== */

static void RS485_InitTXE(void)
{
    GPIO_InitTypeDef gpio_init = {0};

    RS485_TXEN_GPIO_CLK_ENABLE();

    gpio_init.Mode = GPIO_MODE_OUTPUT_PP;
    gpio_init.Pull = GPIO_NOPULL;
    gpio_init.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio_init.Pin = RS485_TXEN_PIN;
    HAL_GPIO_Init(RS485_TXEN_GPIO_PORT, &gpio_init);

    RS485_RX_EN(); /* 默认接收态, 上电不抢总线 */
}

void RS485_SetBaud(uint32_t baud) { (void)comSetBaud(COM3, baud); }

static void RS485_SendBefor(void) { RS485_TX_EN(); }
static void RS485_SendOver(void) { RS485_RX_EN(); }
static void RS485_ReciveNew(uint8_t byte)
{
    (void)byte; /* 留给 MODBUS / 上层协议挂接 */
}

void RS485_SendBuf(const uint8_t *buf, uint16_t len)
{
    (void)comSendBuf(COM3, buf, len);
}

void RS485_SendStr(const char *str)
{
    if (str == NULL)
    {
        return;
    }
    RS485_SendBuf((const uint8_t *)str, (uint16_t)strlen(str));
}

/* ========================================================================== */
/*                              Init / DeInit                                  */
/* ========================================================================== */

/**
 * @brief 初始化指定的 UART_T 上下文 + 启动 RX DMA.
 */
static void uart_port_setup(UART_T *p,
                            UART_HandleTypeDef *huart,
                            uint8_t *tx_buf, uint16_t tx_size,
                            uint8_t *rx_buf, uint16_t rx_size,
                            void (*before)(void),
                            void (*over)(void),
                            void (*recv_new)(uint8_t))
{
    p->huart = huart;
    p->SendBefor = before;
    p->SendOver = over;
    p->ReciveNew = recv_new;
    p->Sending = FALSE;

    ringbuffer_init(&p->tx_kfifo, tx_buf, tx_size);
    ringbuffer_init(&p->rx_kfifo, rx_buf, rx_size);

    if (HAL_UARTEx_ReceiveToIdle_DMA(huart, rx_buf, rx_size) != HAL_OK)
    {
        ERROR_HANDLER();
    }
}

void bsp_InitUart(void)
{
    /* RS485 控制脚先就绪, 避免 USART3 启用后短暂处于发送态. */
    RS485_InitTXE();

#if UART1_FIFO_EN == 1
    MX_USART1_UART_Init();
    uart_port_setup(&s_uart1, &huart1,
                    s_tx_buf1, (uint16_t)UART1_TX_BUF_SIZE,
                    s_rx_buf1, (uint16_t)find_PowerOf2(UART1_RX_BUF_SIZE, 0),
                    NULL, NULL, NULL);
#endif

#if UART2_FIFO_EN == 1
    /* 用户启用 USART2 时需自行提供 MX_USART2_UART_Init() 与 MSP/IRQ 分支. */
    MX_USART2_UART_Init();
    uart_port_setup(&s_uart2, &huart2,
                    s_tx_buf2, (uint16_t)UART2_TX_BUF_SIZE,
                    s_rx_buf2, (uint16_t)find_PowerOf2(UART2_RX_BUF_SIZE, 0),
                    NULL, NULL, NULL);
#endif

#if UART3_FIFO_EN == 1
    MX_USART3_UART_Init();
    uart_port_setup(&s_uart3, &huart3,
                    s_tx_buf3, (uint16_t)UART3_TX_BUF_SIZE,
                    s_rx_buf3, (uint16_t)find_PowerOf2(UART3_RX_BUF_SIZE, 0),
                    RS485_SendBefor, RS485_SendOver, RS485_ReciveNew);
#endif

#if UART4_FIFO_EN == 1
    /* 用户启用 UART4 时需自行提供 MX_USART4_UART_Init() 与 MSP/IRQ 分支. */
    MX_USART4_UART_Init();
    uart_port_setup(&s_uart4, &huart4,
                    s_tx_buf4, (uint16_t)UART4_TX_BUF_SIZE,
                    s_rx_buf4, (uint16_t)find_PowerOf2(UART4_RX_BUF_SIZE, 0),
                    NULL, NULL, NULL);
#endif

#if UART5_FIFO_EN == 1
    /* 用户启用 UART5 时需自行提供 MX_USART5_UART_Init() 与 MSP/IRQ 分支. */
    MX_USART5_UART_Init();
    uart_port_setup(&s_uart5, &huart5,
                    s_tx_buf5, (uint16_t)UART5_TX_BUF_SIZE,
                    s_rx_buf5, (uint16_t)find_PowerOf2(UART5_RX_BUF_SIZE, 0),
                    NULL, NULL, NULL);
#endif

#if UART6_FIFO_EN == 1
    MX_USART6_UART_Init();
    uart_port_setup(&s_uart6, &huart6,
                    s_tx_buf6, (uint16_t)UART6_TX_BUF_SIZE,
                    s_rx_buf6, (uint16_t)find_PowerOf2(UART6_RX_BUF_SIZE, 0),
                    NULL, NULL, NULL);
#endif

#if UART7_FIFO_EN == 1
    /* 用户启用 UART7 时需自行提供 MX_USART7_UART_Init() 与 MSP/IRQ 分支. */
    MX_USART7_UART_Init();
    uart_port_setup(&s_uart7, &huart7,
                    s_tx_buf7, (uint16_t)UART7_TX_BUF_SIZE,
                    s_rx_buf7, (uint16_t)find_PowerOf2(UART7_RX_BUF_SIZE, 0),
                    NULL, NULL, NULL);
#endif

#if UART8_FIFO_EN == 1
    /* 用户启用 UART8 时需自行提供 MX_USART8_UART_Init() 与 MSP/IRQ 分支. */
    MX_USART8_UART_Init();
    uart_port_setup(&s_uart8, &huart8,
                    s_tx_buf8, (uint16_t)UART8_TX_BUF_SIZE,
                    s_rx_buf8, (uint16_t)roundup_pow_of_two(UART8_RX_BUF_SIZE),
                    NULL, NULL, NULL);
#endif
}

void bsp_DeInitUart(void)
{
#if UART1_FIFO_EN == 1
    HAL_UART_DeInit(&huart1);
#endif
#if UART2_FIFO_EN == 1
    HAL_UART_DeInit(&huart2);
#endif
#if UART3_FIFO_EN == 1
    HAL_UART_DeInit(&huart3);
#endif
#if UART4_FIFO_EN == 1
    HAL_UART_DeInit(&huart4);
#endif
#if UART5_FIFO_EN == 1
    HAL_UART_DeInit(&huart5);
#endif
#if UART6_FIFO_EN == 1
    HAL_UART_DeInit(&huart6);
#endif
#if UART7_FIFO_EN == 1
    HAL_UART_DeInit(&huart7);
#endif
#if UART8_FIFO_EN == 1
    HAL_UART_DeInit(&huart8);
#endif
}

/* ========================================================================== */
/*                                  Shell cmd                                  */
/* ========================================================================== */

#if defined(__SHELL_H__) && defined(DEBUG_MODE)

static int _com_cmd(int argc, char *argv[])
{
    static int8_t s_com_num = 0;

    static const char *help_info[] = {
        "probe <1..8>     select active port",
        "read len         show RX buffer length",
        "read char        read 1 byte from RX",
        "read buff <n>    read up to n bytes (hex dump)",
        "write <text>     send a string",
        "clear            clear TX & RX FIFO",
        "baud <bps>       change baudrate",
    };

    if (argc < 2)
    {
        printf("Usage:\r\n");
        for (uint32_t i = 0; i < sizeof(help_info) / sizeof(help_info[0]); i++)
        {
            printf("  %s %s\r\n", argv[0], help_info[i]);
        }
        return 0;
    }

    /* probe ----------------------------------------------------- */
    if (!strcmp(argv[1], "probe"))
    {
        if (argc < 3)
        {
            printf("usage: %s probe <1..8>\r\n", argv[0]);
            return -1;
        }
        int8_t want = (int8_t)atoi(argv[2]);
        if (port_to_uart((COM_PORT_E)want) == NULL)
        {
            printf("COM%d not enabled. Available:", want);
            for (int8_t i = COM1; i <= COM8; i++)
            {
                if (port_to_uart((COM_PORT_E)i) != NULL)
                {
                    printf(" %d", i);
                }
            }
            printf("\r\n");
            return -1;
        }
        s_com_num = want;
        printf("COM%d selected\r\n", s_com_num);
        return 0;
    }

    if (s_com_num == 0)
    {
        printf("no port selected, try: %s probe <n>\r\n", argv[0]);
        return -1;
    }

    /* read ------------------------------------------------------ */
    if (!strcmp(argv[1], "read"))
    {
        if (argc < 3)
        {
            printf("usage: %s read len|char|buff\r\n", argv[0]);
            return -1;
        }
        if (!strcmp(argv[2], "len"))
        {
            printf("length = %u\r\n", comGetLen((COM_PORT_E)s_com_num));
            return 0;
        }
        if (!strcmp(argv[2], "char"))
        {
            uint8_t data;
            if (comGetChar((COM_PORT_E)s_com_num, &data))
            {
                printf("read = 0x%02X (%c)\r\n", data, data);
            }
            else
            {
                printf("RX empty\r\n");
            }
            return 0;
        }
        if (!strcmp(argv[2], "buff"))
        {
            if (argc < 4)
            {
                printf("usage: %s read buff <len>\r\n", argv[0]);
                return -1;
            }
            uint16_t length = (uint16_t)atoi(argv[3]);
            if (length == 0U)
            {
                return -1;
            }
            uint8_t *buff = (uint8_t *)malloc(length);
            if (buff == NULL)
            {
                printf("low memory\r\n");
                return -1;
            }
            uint16_t got = comGetBuf((COM_PORT_E)s_com_num, buff, length);
            if (got)
            {
                dump_hex(buff, got, 16);
            }
            else
            {
                printf("RX empty\r\n");
            }
            free(buff);
            return 0;
        }
        printf("unknown read sub-cmd: %s\r\n", argv[2]);
        return -1;
    }

    /* write ----------------------------------------------------- */
    if (!strcmp(argv[1], "write"))
    {
        if (argc < 3)
        {
            printf("usage: %s write <text>\r\n", argv[0]);
            return -1;
        }
        comSendBuf((COM_PORT_E)s_com_num, (const uint8_t *)argv[2], (uint16_t)strlen(argv[2]));
        return 0;
    }

    /* clear ----------------------------------------------------- */
    if (!strcmp(argv[1], "clear"))
    {
        comClearRxFifo((COM_PORT_E)s_com_num);
        comClearTxFifo((COM_PORT_E)s_com_num);
        printf("COM%d FIFO cleared\r\n", s_com_num);
        return 0;
    }

    /* baud ------------------------------------------------------ */
    if (!strcmp(argv[1], "baud"))
    {
        if (argc < 3)
        {
            printf("usage: %s baud <bps>\r\n", argv[0]);
            return -1;
        }
        uint32_t baud = (uint32_t)strtoul(argv[2], NULL, 0);
        if (baud == 0U)
        {
            printf("invalid baud\r\n");
            return -1;
        }
        int rc = comSetBaud((COM_PORT_E)s_com_num, baud);
        if (rc == 0)
        {
            printf("COM%d -> %u bps\r\n", s_com_num, (unsigned)baud);
        }
        else
        {
            printf("comSetBaud failed (%d)\r\n", rc);
        }
        return rc;
    }

    printf("unknown sub-command: %s\r\n", argv[1]);
    return -1;
}

SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN),
                 com, _com_cmd, com[probe read write clear baud]);
#endif /* __SHELL_H__ && DEBUG_MODE */

/* ========================================================================== */
/*                             stdio retarget (printf)                         */
/* ========================================================================== */

/**
 * @brief 把 printf 重定向到 COM1.
 */
int fputc(int ch, FILE *f)
{
    (void)f;
    comSendChar(COM1, (uint8_t)ch);
    return ch;
}

/**
 * @brief 把 getchar 重定向到 COM1 (阻塞读).
 */
int fgetc(FILE *f)
{
    (void)f;
    uint8_t ch;
    while (comGetChar(COM1, &ch) == 0U)
    {
    }
    return (int)ch;
}

/******************** End of file ********************/
