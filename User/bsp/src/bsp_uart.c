/*
*********************************************************************************************************
*
*   模块名称 : 串口中断+FIFO驱动模块
*   文件名称 : bsp_uart_fifo.c
*   版    本 : V1.8
*   说    明 : 采用串口中断+FIFO模式实现多个串口的同时访问
*   修改记录 :
*       版本号  日期       作者    说明
*       V1.0    2013-02-01 armfly  正式发布
*       V1.1    2013-06-09 armfly  FiFo结构增加TxCount成员变量，方便判断缓冲区满; 增加 清FiFo的函数
*       V1.2    2014-09-29 armfly  增加RS485 MODBUS接口。接收到新字节后，直接执行回调函数。
*       V1.3    2015-07-23 armfly  增加 UART_T 结构的读写指针几个成员变量必须增加 __IO 修饰,否则优化后
*                   会导致串口发送函数死机。
*       V1.4    2015-08-04 armfly  解决UART4配置bug  GPIO_PinAFConfig(GPIOC, GPIO_PinSource11, GPIO_AF_USART1);
*       V1.5    2015-10-08 armfly  增加修改波特率的接口函数
*       V1.6    2018-09-07 armfly  移植到STM32H7平台
*       V1.7    2018-10-01 armfly  增加 Sending 标志，表示正在发送中
*       V1.8    2018-11-26 armfly  增加UART8，第8个串口
*
*   Copyright (C), 2015-2030, 安富莱电子 www.armfly.com
*
*********************************************************************************************************
*/
#include "bsp.h"
#include "bsp_uart.h"
#include "ring_buffer.h"

/* Private variables ---------------------------------------------------------*/

/* External variables --------------------------------------------------------*/

#define DMA_TX_BUF_SIZE 16

static void UartVarInit(void);
static UART_T *ComToUart(COM_PORT_E _ucPort);
static UART_T *BaseToUart(USART_TypeDef *_pBase);
static void UartSend(UART_T *_pUart, uint8_t *_ucaBuf, uint16_t _usLen);

#if UART1_FIFO_EN == 1
UART_T g_tUart1 = {0};
uint8_t s_dma_buf1[DMA_TX_BUF_SIZE];  /* DMA发送缓冲区 */
uint8_t s_tx_buf1[UART1_TX_BUF_SIZE]; /* 发送缓冲区 */
uint8_t s_rx_buf1[UART1_RX_BUF_SIZE]; /* 接收缓冲区 */
UART_HandleTypeDef huart1;
DMA_HandleTypeDef hdma_usart1_tx;
DMA_HandleTypeDef hdma_usart1_rx;
#endif

#if UART2_FIFO_EN == 1
UART_T g_tUart2 = {0};
uint8_t s_dma_buf2[DMA_TX_BUF_SIZE];  /* DMA发送缓冲区 */
uint8_t s_tx_buf2[UART2_TX_BUF_SIZE]; /* 发送缓冲区 */
uint8_t s_rx_buf2[UART2_RX_BUF_SIZE]; /* 接收缓冲区 */
UART_HandleTypeDef huart2;
DMA_HandleTypeDef hdma_usart2_tx;
DMA_HandleTypeDef hdma_usart2_rx;
#endif

#if UART3_FIFO_EN == 1
UART_T g_tUart3 = {0};
uint8_t s_dma_buf3[DMA_TX_BUF_SIZE];  /* DMA发送缓冲区 */
uint8_t s_tx_buf3[UART3_TX_BUF_SIZE]; /* 发送缓冲区 */
uint8_t s_rx_buf3[UART3_RX_BUF_SIZE]; /* 接收缓冲区 */
UART_HandleTypeDef huart3;
DMA_HandleTypeDef hdma_usart3_tx;
DMA_HandleTypeDef hdma_usart3_rx;
#endif

#if UART4_FIFO_EN == 1
UART_T g_tUart4 = {0};
uint8_t s_dma_buf4[DMA_TX_BUF_SIZE];  /* DMA发送缓冲区 */
uint8_t s_tx_buf4[UART4_TX_BUF_SIZE]; /* 发送缓冲区 */
uint8_t s_rx_buf4[UART4_RX_BUF_SIZE]; /* 接收缓冲区 */
UART_HandleTypeDef huart4;
DMA_HandleTypeDef hdma_usart4_tx;
DMA_HandleTypeDef hdma_usart4_rx;
#endif

#if UART5_FIFO_EN == 1
UART_T g_tUart5 = {0};
uint8_t s_dma_buf5[DMA_TX_BUF_SIZE];  /* DMA发送缓冲区 */
uint8_t s_tx_buf5[UART5_TX_BUF_SIZE]; /* 发送缓冲区 */
uint8_t s_rx_buf5[UART5_RX_BUF_SIZE]; /* 接收缓冲区 */
UART_HandleTypeDef huart5;
DMA_HandleTypeDef hdma_usart5_tx;
DMA_HandleTypeDef hdma_usart5_rx;
#endif

#if UART6_FIFO_EN == 1
UART_T g_tUart6 = {0};
uint8_t s_dma_buf6[DMA_TX_BUF_SIZE];  /* DMA发送缓冲区 */
uint8_t s_tx_buf6[UART6_TX_BUF_SIZE]; /* 发送缓冲区 */
uint8_t s_rx_buf6[UART6_RX_BUF_SIZE]; /* 接收缓冲区 */
UART_HandleTypeDef huart6;
DMA_HandleTypeDef hdma_usart6_tx;
DMA_HandleTypeDef hdma_usart6_rx;
#endif

#if UART7_FIFO_EN == 1
UART_T g_tUart7 = {0};
uint8_t s_dma_buf7[DMA_TX_BUF_SIZE];  /* DMA发送缓冲区 */
uint8_t s_tx_buf7[UART7_TX_BUF_SIZE]; /* 发送缓冲区 */
uint8_t s_rx_buf7[UART7_RX_BUF_SIZE]; /* 接收缓冲区 */
UART_HandleTypeDef huart7;
DMA_HandleTypeDef hdma_usart7_tx;
DMA_HandleTypeDef hdma_usart7_rx;
#endif

#if UART8_FIFO_EN == 1
UART_T g_tUart8 = {0};
uint8_t s_dma_buf8[DMA_TX_BUF_SIZE];  /* DMA发送缓冲区 */
uint8_t s_tx_buf8[UART8_TX_BUF_SIZE]; /* 发送缓冲区 */
uint8_t s_rx_buf8[UART8_RX_BUF_SIZE]; /* 接收缓冲区 */
UART_HandleTypeDef huart8;
DMA_HandleTypeDef hdma_usart8_tx;
DMA_HandleTypeDef hdma_usart8_rx;
#endif

/**
 * @brief USART1 Initialization Function
 * @param None
 * @retval None
 */
#if UART1_FIFO_EN == 1
static void MX_USART1_UART_Init(void)
{
    huart1.Instance = USART1;
    huart1.Init.BaudRate = 115200;
    huart1.Init.WordLength = UART_WORDLENGTH_8B;
    huart1.Init.StopBits = UART_STOPBITS_1;
    huart1.Init.Parity = UART_PARITY_NONE;
    huart1.Init.Mode = UART_MODE_TX_RX;
    huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart1.Init.OverSampling = UART_OVERSAMPLING_16;
    huart1.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
    huart1.Init.ClockPrescaler = UART_PRESCALER_DIV1;
    huart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
    if (HAL_UART_Init(&huart1) != HAL_OK)
    {
        ERROR_HANDLER();
    }
    if (HAL_UARTEx_SetTxFifoThreshold(&huart1, UART_TXFIFO_THRESHOLD_1_8) != HAL_OK)
    {
        ERROR_HANDLER();
    }
    if (HAL_UARTEx_SetRxFifoThreshold(&huart1, UART_RXFIFO_THRESHOLD_1_8) != HAL_OK)
    {
        ERROR_HANDLER();
    }
    if (HAL_UARTEx_DisableFifoMode(&huart1) != HAL_OK)
    {
        ERROR_HANDLER();
    }
}
#endif

/**
 * @brief USART3 Initialization Function
 * @param None
 * @retval None
 */
#if UART3_FIFO_EN == 1
static void MX_USART3_UART_Init(void)
{
    huart3.Instance = USART3;
    huart3.Init.BaudRate = 115200;
    huart3.Init.WordLength = UART_WORDLENGTH_8B;
    huart3.Init.StopBits = UART_STOPBITS_1;
    huart3.Init.Parity = UART_PARITY_NONE;
    huart3.Init.Mode = UART_MODE_TX_RX;
    huart3.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart3.Init.OverSampling = UART_OVERSAMPLING_16;
    huart3.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
    huart3.Init.ClockPrescaler = UART_PRESCALER_DIV1;
    huart3.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
    if (HAL_UART_Init(&huart3) != HAL_OK)
    {
        ERROR_HANDLER();
    }
    if (HAL_UARTEx_SetTxFifoThreshold(&huart3, UART_TXFIFO_THRESHOLD_1_8) != HAL_OK)
    {
        ERROR_HANDLER();
    }
    if (HAL_UARTEx_SetRxFifoThreshold(&huart3, UART_RXFIFO_THRESHOLD_1_8) != HAL_OK)
    {
        ERROR_HANDLER();
    }
    if (HAL_UARTEx_DisableFifoMode(&huart3) != HAL_OK)
    {
        ERROR_HANDLER();
    }
}
#endif

/**
 * @brief USART6 Initialization Function
 * @param None
 * @retval None
 */
#if UART6_FIFO_EN == 1
static void MX_USART6_UART_Init(void)
{
    huart6.Instance = USART6;
    huart6.Init.BaudRate = 115200;
    huart6.Init.WordLength = UART_WORDLENGTH_8B;
    huart6.Init.StopBits = UART_STOPBITS_1;
    huart6.Init.Parity = UART_PARITY_NONE;
    huart6.Init.Mode = UART_MODE_TX_RX;
    huart6.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart6.Init.OverSampling = UART_OVERSAMPLING_16;
    huart6.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
    huart6.Init.ClockPrescaler = UART_PRESCALER_DIV1;
    huart6.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
    if (HAL_UART_Init(&huart6) != HAL_OK)
    {
        ERROR_HANDLER();
    }
    if (HAL_UARTEx_SetTxFifoThreshold(&huart6, UART_TXFIFO_THRESHOLD_1_8) != HAL_OK)
    {
        ERROR_HANDLER();
    }
    if (HAL_UARTEx_SetRxFifoThreshold(&huart6, UART_RXFIFO_THRESHOLD_1_8) != HAL_OK)
    {
        ERROR_HANDLER();
    }
    if (HAL_UARTEx_DisableFifoMode(&huart6) != HAL_OK)
    {
        ERROR_HANDLER();
    }
}
#endif

/**
 * @brief UART MSP Initialization
 * This function configures the hardware resources used in this example
 * @param huart: UART handle pointer
 * @retval None
 */
void HAL_UART_MspInit(UART_HandleTypeDef *huart)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};
    if (huart->Instance == USART1)
    {
#if UART1_FIFO_EN == 1
        PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_USART1;
        PeriphClkInitStruct.Usart16ClockSelection = RCC_USART16CLKSOURCE_D2PCLK2;
        if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
        {
            ERROR_HANDLER();
        }

        /* Peripheral clock enable */
        __HAL_RCC_USART1_CLK_ENABLE();

        __HAL_RCC_GPIOA_CLK_ENABLE();
        /**USART1 GPIO Configuration
        PA10     ------> USART1_RX
        PA9     ------> USART1_TX
        */
        GPIO_InitStruct.Pin = GPIO_PIN_10 | GPIO_PIN_9;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
        GPIO_InitStruct.Alternate = GPIO_AF7_USART1;
        HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

        /* USART1 DMA Init */
        /* USART1_RX Init */
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

        /* USART1_TX Init */
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

        /* USART1 interrupt Init */
        HAL_NVIC_SetPriority(USART1_IRQn, 0, 0);
        HAL_NVIC_EnableIRQ(USART1_IRQn);
#endif
    }
#if UART3_FIFO_EN == 1
    else if (huart->Instance == USART3)
    {
        /** Initializes the peripherals clock */
        PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_USART3;
        PeriphClkInitStruct.Usart234578ClockSelection = RCC_USART234578CLKSOURCE_D2PCLK1;
        if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
        {
            ERROR_HANDLER();
        }

        /* Peripheral clock enable */
        __HAL_RCC_USART3_CLK_ENABLE();

        __HAL_RCC_GPIOC_CLK_ENABLE();
        /**USART3 GPIO Configuration
        PC10     ------> USART3_TX
        PC11     ------> USART3_RX
        */
        GPIO_InitStruct.Pin = GPIO_PIN_10 | GPIO_PIN_11;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
        GPIO_InitStruct.Alternate = GPIO_AF7_USART3;
        HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

        /* USART3 DMA Init */
        /* USART3_RX Init */
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

        /* USART3_TX Init */
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

        /* USART3 interrupt Init */
        HAL_NVIC_SetPriority(USART3_IRQn, 0, 0);
        HAL_NVIC_EnableIRQ(USART3_IRQn);
    }
#endif
#if UART6_FIFO_EN == 1
    else if (huart->Instance == USART6)
    {
        /** Initializes the peripherals clock
         */
        PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_USART6;
        PeriphClkInitStruct.Usart16ClockSelection = RCC_USART16CLKSOURCE_D2PCLK2;
        if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
        {
            ERROR_HANDLER();
        }

        /* Peripheral clock enable */
        __HAL_RCC_USART6_CLK_ENABLE();

        __HAL_RCC_GPIOG_CLK_ENABLE();
        __HAL_RCC_GPIOC_CLK_ENABLE();
        /**USART6 GPIO Configuration
        PG14     ------> USART6_TX
        PC7     ------> USART6_RX
        */
        GPIO_InitStruct.Pin = GPIO_PIN_14;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
        GPIO_InitStruct.Alternate = GPIO_AF7_USART6;
        HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);

        GPIO_InitStruct.Pin = GPIO_PIN_7;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
        GPIO_InitStruct.Alternate = GPIO_AF7_USART6;
        HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

        /* USART6 DMA Init */
        /* USART6_RX Init */
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

        /* USART6_TX Init */
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

        /* USART6 interrupt Init */
        HAL_NVIC_SetPriority(USART6_IRQn, 0, 0);
        HAL_NVIC_EnableIRQ(USART6_IRQn);
    }
#endif
}

/**
 * @brief UART MSP De-Initialization
 * This function freeze the hardware resources used in this example
 * @param huart: UART handle pointer
 * @retval None
 */
void HAL_UART_MspDeInit(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1)
    {
        /* USER CODE BEGIN USART1_MspDeInit 0 */

        /* USER CODE END USART1_MspDeInit 0 */
        /* Peripheral clock disable */
        __HAL_RCC_USART1_CLK_DISABLE();

        /**USART1 GPIO Configuration
        PA10     ------> USART1_RX
        PA9     ------> USART1_TX
        */
        HAL_GPIO_DeInit(GPIOA, GPIO_PIN_10 | GPIO_PIN_9);

        /* USART1 DMA DeInit */
        HAL_DMA_DeInit(huart->hdmarx);
        HAL_DMA_DeInit(huart->hdmatx);

        /* USART1 interrupt DeInit */
        HAL_NVIC_DisableIRQ(USART1_IRQn);
        /* USER CODE BEGIN USART1_MspDeInit 1 */

        /* USER CODE END USART1_MspDeInit 1 */
    }
    else if (huart->Instance == USART3)
    {
        /* USER CODE BEGIN USART3_MspDeInit 0 */

        /* USER CODE END USART3_MspDeInit 0 */
        /* Peripheral clock disable */
        __HAL_RCC_USART3_CLK_DISABLE();

        /**USART3 GPIO Configuration
        PC10     ------> USART3_TX
        PC11     ------> USART3_RX
        */
        HAL_GPIO_DeInit(GPIOC, GPIO_PIN_10 | GPIO_PIN_11);

        /* USART3 DMA DeInit */
        HAL_DMA_DeInit(huart->hdmarx);
        HAL_DMA_DeInit(huart->hdmatx);

        /* USART3 interrupt DeInit */
        HAL_NVIC_DisableIRQ(USART3_IRQn);
        /* USER CODE BEGIN USART3_MspDeInit 1 */

        /* USER CODE END USART3_MspDeInit 1 */
    }
    else if (huart->Instance == USART6)
    {
        /* USER CODE BEGIN USART6_MspDeInit 0 */

        /* USER CODE END USART6_MspDeInit 0 */
        /* Peripheral clock disable */
        __HAL_RCC_USART6_CLK_DISABLE();

        /**USART6 GPIO Configuration
        PG14     ------> USART6_TX
        PC7     ------> USART6_RX
        */
        HAL_GPIO_DeInit(GPIOG, GPIO_PIN_14);

        HAL_GPIO_DeInit(GPIOC, GPIO_PIN_7);

        /* USART6 DMA DeInit */
        HAL_DMA_DeInit(huart->hdmarx);
        HAL_DMA_DeInit(huart->hdmatx);

        /* USART6 interrupt DeInit */
        HAL_NVIC_DisableIRQ(USART6_IRQn);
        /* USER CODE BEGIN USART6_MspDeInit 1 */

        /* USER CODE END USART6_MspDeInit 1 */
    }
}

/**
 * [HAL_UARTEx_RxEventCallback description]
 *
 * @return  void    [return description]
 */
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    UART_T *pUart = BaseToUart(huart->Instance);

    if (pUart != 0)
    {
        uint16_t space = ringbuffer_space_len(&pUart->rx_kfifo);                                         //剩余空间
        uint16_t index_new = Size & (pUart->rx_kfifo.buffer_size - 1);                                   //新写入指针
        uint16_t length = (index_new - pUart->rx_kfifo.write_index) & (pUart->rx_kfifo.buffer_size - 1); //新写入长度

        if (pUart->rx_kfifo.write_index > index_new) //满一圈
        {
            pUart->rx_kfifo.write_mirror = ~pUart->rx_kfifo.write_mirror;
            if (length > space)
            {
                pUart->rx_kfifo.read_mirror = ~pUart->rx_kfifo.read_mirror;
            }
        }
        if (length > space)
        {
            pUart->rx_kfifo.read_index = index_new;
        }
        pUart->rx_kfifo.write_index = index_new;
    }
}

/**
 * [HAL_UART_TxCpltCallback description]
 *
 * @return  void    [return description]
 */
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    UART_T *pUart = BaseToUart(huart->Instance);
    uint16_t len;
    if (pUart != 0)
    {
        len = ringbuffer_data_len(&pUart->tx_kfifo);
        if (len == 0)
        {
            /* 回调函数, 一般用来处理RS485通信，将RS485芯片设置为接收模式，避免抢占总线 */
            if (pUart->SendOver)
            {
                pUart->SendOver();
            }
            pUart->Sending = FALSE;
            return;
        }

        if (len > DMA_TX_BUF_SIZE)
        {
            len = DMA_TX_BUF_SIZE;
        }
        ringbuffer_get(&pUart->tx_kfifo, pUart->dma_buf, len);
        /* CPU访问前，将Cache对应的区域无效化 */
        SCB_CleanInvalidateDCache_by_Addr((uint32_t *)pUart->dma_buf, (int32_t)len);
        HAL_UART_Transmit_DMA(pUart->huart, pUart->dma_buf, len);
    }
}

/**
 * @brief This function handles USART1 global interrupt.
 */
#if UART1_FIFO_EN == 1
void USART1_IRQHandler(void)
{
    HAL_UART_IRQHandler(&huart1);
}
#endif

/**
 * @brief This function handles USART3 global interrupt.
 */
#if UART3_FIFO_EN == 1
void USART3_IRQHandler(void)
{
    HAL_UART_IRQHandler(&huart3);
}
#endif

/**
 * @brief This function handles USART6 global interrupt.
 */
#if UART6_FIFO_EN == 1
void USART6_IRQHandler(void)
{
    HAL_UART_IRQHandler(&huart6);
}
#endif

/*
*********************************************************************************************************
*   函 数 名: BaseToUart
*   功能说明: 将UART基地址转换为UART指针
*   形    参: _pBase: UART基地址(USART1 - USART8)
*   返 回 值: uart指针
*********************************************************************************************************
*/
static UART_T *BaseToUart(USART_TypeDef *_pBase)
{
    if (_pBase == USART1)
    {
#if UART1_FIFO_EN == 1
        return &g_tUart1;
#else
        return 0;
#endif
    }
#if UART2_FIFO_EN == 1
    else if (_pBase == USART2)
    {
        return &g_tUart2;
    }
#endif
#if UART3_FIFO_EN == 1
    else if (_pBase == USART3)
    {
        return &g_tUart3;
    }
#endif
#if UART4_FIFO_EN == 1
    else if (_pBase == UART4)
    {
        return &g_tUart4;
    }
#endif
#if UART5_FIFO_EN == 1
    else if (_pBase == UART5)
    {
        return &g_tUart5;
    }
#endif
#if UART6_FIFO_EN == 1
    else if (_pBase == USART6)
    {
        return &g_tUart6;
    }
#endif
#if UART7_FIFO_EN == 1
    else if (_pBase == UART7)
    {
        return &g_tUart7;
    }
#endif
#if UART8_FIFO_EN == 1
    else if (_pBase == UART8)
    {
        return &g_tUart8;
    }
#endif
    /* 不做任何处理 */
    return 0;
}

/*
*********************************************************************************************************
*   函 数 名: ComToUart
*   功能说明: 将COM端口号转换为UART指针
*   形    参: _ucPort: 端口号(COM1 - COM8)
*   返 回 值: uart指针
*********************************************************************************************************
*/
static UART_T *ComToUart(COM_PORT_E _ucPort)
{
    if (_ucPort == COM1)
    {
#if UART1_FIFO_EN == 1
        return &g_tUart1;
#else
        return 0;
#endif
    }
#if UART2_FIFO_EN == 1
    else if (_ucPort == COM2)
    {
        return &g_tUart2;
    }
#endif
#if UART3_FIFO_EN == 1
    else if (_ucPort == COM3)
    {
        return &g_tUart3;
    }
#endif
#if UART4_FIFO_EN == 1
    else if (_ucPort == COM4)
    {
        return &g_tUart4;
    }
#endif
#if UART5_FIFO_EN == 1
    else if (_ucPort == COM5)
    {
        return &g_tUart5;
    }
#endif
#if UART6_FIFO_EN == 1
    else if (_ucPort == COM6)
    {
        return &g_tUart6;
    }
#endif
#if UART7_FIFO_EN == 1
    else if (_ucPort == COM7)
    {
        return &g_tUart7;
    }
#endif
#if UART8_FIFO_EN == 1
    else if (_ucPort == COM8)
    {
        return &g_tUart8;
    }
#endif
    /* 不做任何处理 */
    return 0;
}

/*
*********************************************************************************************************
*   函 数 名: UartSend
*   功能说明: 填写数据到UART发送缓冲区,并启动发送中断。中断处理函数发送完毕后，自动关闭发送中断
*   形    参: 无
*   返 回 值: 无
*********************************************************************************************************
*/
static void UartSend(UART_T *_pUart, uint8_t *_ucaBuf, uint16_t _usLen)
{
    uint16_t len;

    len = ringbuffer_put(&_pUart->tx_kfifo, _ucaBuf, _usLen);

    /* DMA不忙 */
    // if (HAL_DMA_STATE_BUSY != HAL_DMA_GetState(_pUart->huart->hdmatx))
    if (_pUart->Sending != TRUE)
    {
        _pUart->Sending = TRUE;
        if (len > DMA_TX_BUF_SIZE)
        {
            ringbuffer_get(&_pUart->tx_kfifo, _pUart->dma_buf, DMA_TX_BUF_SIZE);
            /* CPU访问前，将Cache对应的区域无效化 */
            SCB_CleanInvalidateDCache_by_Addr((uint32_t *)_pUart->dma_buf, (int32_t)DMA_TX_BUF_SIZE);
            HAL_UART_Transmit_DMA(_pUart->huart, _pUart->dma_buf, DMA_TX_BUF_SIZE);
        }
        else
        {
            ringbuffer_get(&_pUart->tx_kfifo, _pUart->dma_buf, len);
            /* CPU访问前，将Cache对应的区域无效化 */
            SCB_CleanInvalidateDCache_by_Addr((uint32_t *)_pUart->dma_buf, (int32_t)len);
            HAL_UART_Transmit_DMA(_pUart->huart, _pUart->dma_buf, len);
        }
    }

    while (len < _usLen)
    {
        len += ringbuffer_put(&_pUart->tx_kfifo, _ucaBuf + len, _usLen - len);
    }
}

/*
*********************************************************************************************************
*   函 数 名: comSendBuf
*   功能说明: 向串口发送一组数据。数据放到发送缓冲区后立即返回，由中断服务程序在后台完成发送
*   形    参: _ucPort: 端口号(COM1 - COM8)
*             _ucaBuf: 待发送的数据缓冲区
*             _usLen : 数据长度
*   返 回 值: 无
*********************************************************************************************************
*/
void comSendBuf(COM_PORT_E _ucPort, uint8_t *_ucaBuf, uint16_t _usLen)
{
    UART_T *pUart;

    pUart = ComToUart(_ucPort);
    if (pUart == 0)
    {
        return;
    }

    if (pUart->SendBefor != 0)
    {
        pUart->SendBefor(); /* 如果是RS485通信，可以在这个函数中将RS485设置为发送模式 */
    }

    UartSend(pUart, _ucaBuf, _usLen);
}

/*
*********************************************************************************************************
*   函 数 名: comSendChar
*   功能说明: 向串口发送1个字节。数据放到发送缓冲区后立即返回，由中断服务程序在后台完成发送
*   形    参: _ucPort: 端口号(COM1 - COM8)
*             _ucByte: 待发送的数据
*   返 回 值: 无
*********************************************************************************************************
*/
void comSendChar(COM_PORT_E _ucPort, uint8_t _ucByte)
{
    comSendBuf(_ucPort, &_ucByte, 1);
}

/*
*********************************************************************************************************
*   函 数 名: comGetChar
*   功能说明: 从接收缓冲区读取1字节，非阻塞。无论有无数据均立即返回。
*   形    参: _ucPort: 端口号(COM1 - COM8)
*             _pByte: 接收到的数据存放在这个地址
*   返 回 值: 0 表示无数据, 1 表示读取到有效字节
*********************************************************************************************************
*/
uint8_t comGetChar(COM_PORT_E _ucPort, uint8_t *_pByte)
{
    UART_T *pUart;

    pUart = ComToUart(_ucPort);
    if (pUart == 0)
    {
        return 0;
    }
    /* CPU访问前，将Cache对应的区域无效化 */
    SCB_InvalidateDCache_by_Addr((uint32_t *)(pUart->rx_kfifo.buffer_ptr + pUart->rx_kfifo.read_index), (int32_t)1);
    return ringbuffer_getchar(&pUart->rx_kfifo, _pByte);
}

/*
*********************************************************************************************************
*   函 数 名: comGetBuf
*   功能说明: 从接收缓冲区读取指定长度，非阻塞。无论有无数据均立即返回。
*   形    参: _ucPort: 端口号(COM1 - COM8)
*             _pByte: 接收到的数据存放在这个地址
*   返 回 值: 0 表示无数据, x 表示读取到有效字节
*********************************************************************************************************
*/
uint16_t comGetBuf(COM_PORT_E _ucPort, uint8_t *_pByte, uint16_t _usLen)
{
    UART_T *pUart;

    pUart = ComToUart(_ucPort);
    if (pUart == 0)
    {
        return 0;
    }
    /* CPU访问前，将Cache对应的区域无效化 */
    SCB_InvalidateDCache_by_Addr((uint32_t *)pUart->rx_kfifo.buffer_ptr, (int32_t)pUart->rx_kfifo.buffer_size);
    return ringbuffer_get(&pUart->rx_kfifo, _pByte, _usLen);
}

/*
*********************************************************************************************************
*   函 数 名: comClearTxFifo
*   功能说明: 清零串口发送缓冲区
*   形    参: _ucPort: 端口号(COM1 - COM8)
*   返 回 值: 无
*********************************************************************************************************
*/
void comClearTxFifo(COM_PORT_E _ucPort)
{
    UART_T *pUart;

    pUart = ComToUart(_ucPort);
    if (pUart == 0)
    {
        return;
    }

    ringbuffer_reset(&pUart->tx_kfifo);
}

/*
*********************************************************************************************************
*   函 数 名: comClearRxFifo
*   功能说明: 清零串口接收缓冲区
*   形    参: _ucPort: 端口号(COM1 - COM8)
*   返 回 值: 无
*********************************************************************************************************
*/
void comClearRxFifo(COM_PORT_E _ucPort)
{
    UART_T *pUart;

    pUart = ComToUart(_ucPort);
    if (pUart == 0)
    {
        return;
    }
    pUart->rx_kfifo.read_index = pUart->rx_kfifo.write_index;
    pUart->rx_kfifo.read_mirror = 0;
    pUart->rx_kfifo.write_mirror = 0;
}

/*
*********************************************************************************************************
*   函 数 名: comSetBaud
*   功能说明: 设置串口的波特率. 本函数固定设置为无校验，收发都使能模式
*   形    参: _ucPort: 端口号(COM1 - COM8)
*             _BaudRate: 波特率，8倍过采样  波特率.0-12.5Mbps
*                               16倍过采样 波特率.0-6.25Mbps
*   返 回 值: 无
*********************************************************************************************************
*/
void comSetBaud(COM_PORT_E _ucPort, uint32_t _BaudRate)
{
    USART_TypeDef *USARTx;

    UART_T *pUart;

    pUart = ComToUart(_ucPort);
    if (pUart == 0)
    {
        return;
    }
    pUart->huart->Init.BaudRate = _BaudRate;
    if (HAL_UART_Init(pUart->huart) != HAL_OK)
    {
        ERROR_HANDLER();
    }
}

/*
*********************************************************************************************************
*   函 数 名: comGetLen
*   功能说明: 取出串口缓冲区个数。
*   形    参:  _pUart : 串口设备
*   返 回 值: 1为空。0为不空。
*********************************************************************************************************
*/
uint16_t comGetLen(COM_PORT_E _ucPort)
{
    UART_T *pUart;

    pUart = ComToUart(_ucPort);
    if (pUart == 0)
    {
        return 0;
    }
    return ringbuffer_data_len(&pUart->rx_kfifo);
}

/*
*********************************************************************************************************
*   函 数 名: UartVarInit
*   功能说明: 初始化串口相关的变量
*   形    参: 无
*   返 回 值: 无
*********************************************************************************************************
*/
static void UartVarInit(void)
{
#if UART1_FIFO_EN == 1
    g_tUart1.huart = &huart1;
    g_tUart1.dma_buf = s_dma_buf1; /* 发送DMA指针 */
    g_tUart1.SendBefor = 0;        /* 发送数据前的回调函数 */
    g_tUart1.SendOver = 0;         /* 发送完毕后的回调函数 */
    g_tUart1.ReciveNew = 0;        /* 接收到新数据后的回调函数 */
    g_tUart1.Sending = 0;          /* 正在发送中标志 */
#endif
#if UART2_FIFO_EN == 1
    g_tUart2.huart = &huart2;
    g_tUart2.dma_buf = s_dma_buf2; /* 发送DMA指针 */
    g_tUart2.SendBefor = 0;        /* 发送数据前的回调函数 */
    g_tUart2.SendOver = 0;         /* 发送完毕后的回调函数 */
    g_tUart2.ReciveNew = 0;        /* 接收到新数据后的回调函数 */
    g_tUart2.Sending = 0;          /* 正在发送中标志 */
#endif
#if UART3_FIFO_EN == 1
    g_tUart3.huart = &huart3;
    g_tUart3.dma_buf = s_dma_buf3; /* 发送DMA指针 */
    g_tUart3.SendBefor = 0;        /* 发送数据前的回调函数 */
    g_tUart3.SendOver = 0;         /* 发送完毕后的回调函数 */
    g_tUart3.ReciveNew = 0;        /* 接收到新数据后的回调函数 */
    g_tUart3.Sending = 0;          /* 正在发送中标志 */
#endif
#if UART4_FIFO_EN == 1
    g_tUart4.huart = &huart4;
    g_tUart4.dma_buf = s_dma_buf4; /* 发送DMA指针 */
    g_tUart4.SendBefor = 0;        /* 发送数据前的回调函数 */
    g_tUart4.SendOver = 0;         /* 发送完毕后的回调函数 */
    g_tUart4.ReciveNew = 0;        /* 接收到新数据后的回调函数 */
    g_tUart4.Sending = 0;          /* 正在发送中标志 */
#endif
#if UART5_FIFO_EN == 1
    g_tUart5.huart = &huart5;
    g_tUart5.dma_buf = s_dma_buf5; /* 发送DMA指针 */
    g_tUart5.SendBefor = 0;        /* 发送数据前的回调函数 */
    g_tUart5.SendOver = 0;         /* 发送完毕后的回调函数 */
    g_tUart5.ReciveNew = 0;        /* 接收到新数据后的回调函数 */
    g_tUart5.Sending = 0;          /* 正在发送中标志 */
#endif
#if UART6_FIFO_EN == 1
    g_tUart6.huart = &huart6;
    g_tUart6.dma_buf = s_dma_buf6; /* 发送DMA指针 */
    g_tUart6.SendBefor = 0;        /* 发送数据前的回调函数 */
    g_tUart6.SendOver = 0;         /* 发送完毕后的回调函数 */
    g_tUart6.ReciveNew = 0;        /* 接收到新数据后的回调函数 */
    g_tUart6.Sending = 0;          /* 正在发送中标志 */
#endif
#if UART7_FIFO_EN == 1
    g_tUart7.huart = &huart7;
    g_tUart7.dma_buf = s_dma_buf7; /* 发送DMA指针 */
    g_tUart7.SendBefor = 0;        /* 发送数据前的回调函数 */
    g_tUart7.SendOver = 0;         /* 发送完毕后的回调函数 */
    g_tUart7.ReciveNew = 0;        /* 接收到新数据后的回调函数 */
    g_tUart7.Sending = 0;          /* 正在发送中标志 */
#endif
}

/*
*********************************************************************************************************
*   函 数 名: bsp_InitUart
*   功能说明: 初始化串口硬件，并对全局变量赋初值.
*   形    参: 无
*   返 回 值: 无
*********************************************************************************************************
*/
void bsp_InitUart(void)
{
    UartVarInit(); /* 必须先初始化全局变量,再配置硬件 */
#if UART1_FIFO_EN == 1
    MX_USART1_UART_Init(); /* 初始化串口 */
    ringbuffer_init(&g_tUart1.tx_kfifo, s_tx_buf1, (UART1_TX_BUF_SIZE));
    ringbuffer_init(&g_tUart1.rx_kfifo, s_rx_buf1, roundup_pow_of_two(UART1_RX_BUF_SIZE));
    HAL_UARTEx_ReceiveToIdle_DMA(&huart1, s_rx_buf1, roundup_pow_of_two(UART1_RX_BUF_SIZE)); /* 启动DMA */
#endif
#if UART2_FIFO_EN == 1
    MX_USART2_UART_Init(); /* 初始化串口 */
    ringbuffer_init(&g_tUart2.tx_kfifo, s_tx_buf2, (UART2_TX_BUF_SIZE));
    ringbuffer_init(&g_tUart2.rx_kfifo, s_rx_buf2, roundup_pow_of_two(UART2_RX_BUF_SIZE));
    HAL_UARTEx_ReceiveToIdle_DMA(&huart2, s_rx_buf2, roundup_pow_of_two(UART2_RX_BUF_SIZE)); /* 启动DMA */
#endif
#if UART3_FIFO_EN == 1
    MX_USART3_UART_Init(); /* 初始化串口 */
    ringbuffer_init(&g_tUart3.tx_kfifo, s_tx_buf3, (UART3_TX_BUF_SIZE));
    ringbuffer_init(&g_tUart3.rx_kfifo, s_rx_buf3, roundup_pow_of_two(UART3_RX_BUF_SIZE));
    HAL_UARTEx_ReceiveToIdle_DMA(&huart3, s_rx_buf3, roundup_pow_of_two(UART3_RX_BUF_SIZE)); /* 启动DMA */
#endif
#if UART4_FIFO_EN == 1
    MX_USART4_UART_Init(); /* 初始化串口 */
    ringbuffer_init(&g_tUart4.tx_kfifo, s_tx_buf4, (UART4_TX_BUF_SIZE));
    ringbuffer_init(&g_tUart4.rx_kfifo, s_rx_buf4, roundup_pow_of_two(UART4_RX_BUF_SIZE));
    HAL_UARTEx_ReceiveToIdle_DMA(&huart4, s_rx_buf4, roundup_pow_of_two(UART4_RX_BUF_SIZE)); /* 启动DMA */
#endif
#if UART5_FIFO_EN == 1
    MX_USART5_UART_Init(); /* 初始化串口 */
    ringbuffer_init(&g_tUart5.tx_kfifo, s_tx_buf5, (UART5_TX_BUF_SIZE));
    ringbuffer_init(&g_tUart5.rx_kfifo, s_rx_buf5, roundup_pow_of_two(UART5_RX_BUF_SIZE));
    HAL_UARTEx_ReceiveToIdle_DMA(&huart5, s_rx_buf5, roundup_pow_of_two(UART5_RX_BUF_SIZE)); /* 启动DMA */
#endif
#if UART6_FIFO_EN == 1
    MX_USART6_UART_Init(); /* 初始化串口 */
    ringbuffer_init(&g_tUart6.tx_kfifo, s_tx_buf6, (UART6_TX_BUF_SIZE));
    ringbuffer_init(&g_tUart6.rx_kfifo, s_rx_buf6, roundup_pow_of_two(UART6_RX_BUF_SIZE));
    HAL_UARTEx_ReceiveToIdle_DMA(&huart6, s_rx_buf6, roundup_pow_of_two(UART6_RX_BUF_SIZE)); /* 启动DMA */
#endif
#if UART7_FIFO_EN == 1
    MX_USART7_UART_Init(); /* 初始化串口 */
    ringbuffer_init(&g_tUart7.tx_kfifo, s_tx_buf7, (UART7_TX_BUF_SIZE));
    ringbuffer_init(&g_tUart7.rx_kfifo, s_rx_buf7, roundup_pow_of_two(UART7_RX_BUF_SIZE));
    HAL_UARTEx_ReceiveToIdle_DMA(&huart7, s_rx_buf7, roundup_pow_of_two(UART7_RX_BUF_SIZE)); /* 启动DMA */
#endif
#if UART8_FIFO_EN == 1
    MX_USART8_UART_Init(); /* 初始化串口 */
    ringbuffer_init(&g_tUart8.tx_kfifo, s_tx_buf8, (UART8_TX_BUF_SIZE));
    ringbuffer_init(&g_tUart8.rx_kfifo, s_rx_buf8, roundup_pow_of_two(UART8_RX_BUF_SIZE));
    HAL_UARTEx_ReceiveToIdle_DMA(&huart8, s_rx_buf8, roundup_pow_of_two(UART8_RX_BUF_SIZE)); /* 启动DMA */
#endif
}

/*
*********************************************************************************************************
*   函 数 名: fputc
*   功能说明: 重定义putc函数，这样可以使用printf函数从串口1打印输出
*   形    参: 无
*   返 回 值: 无
*********************************************************************************************************
*/
int fputc(int ch, FILE *f)
{
#if 1 /* 将需要printf的字符通过串口中断FIFO发送出去，printf函数会立即返回 */
    comSendChar(COM1, ch);

    return ch;
#else /* 采用阻塞方式发送每个字符,等待数据发送完毕 */
    /* 写一个字节到USART1 */
    USART6->TDR = ch;

    /* 等待发送结束 */
    while ((USART6->ISR & USART_ISR_TC) == 0)
    {
    }

    return ch;
#endif
}

/*
*********************************************************************************************************
*   函 数 名: fgetc
*   功能说明: 重定义getc函数，这样可以使用getchar函数从串口1输入数据
*   形    参: 无
*   返 回 值: 无
*********************************************************************************************************
*/
int fgetc(FILE *f)
{

#if 1 /* 从串口接收FIFO中取1个数据, 只有取到数据才返回 */
    uint8_t ucData;

    while (comGetChar(COM1, &ucData) == 0)
        ;

    return ucData;
#else
    /* 等待接收到数据 */
    while ((USART1->ISR & USART_ISR_RXNE) == 0)
    {
    }

    return (int)USART1->RDR;
#endif
}

/***************************** 安富莱电子 www.armfly.com (END OF FILE) *********************************/
