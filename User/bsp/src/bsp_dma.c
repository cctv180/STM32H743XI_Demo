/*
*********************************************************************************************************
*
*    模块名称 : dma驱动模块
*    文件名称 : bsp_dma.c
*    版    本 : V1.8
*    说    明 : 采用串口中断+FIFO模式实现多个串口的同时访问
*    修改记录 :
*        版本号  日期          作者    说明
*        V1.0    2013-02-01   散步去  正式发布
*
*    Copyright (C), 2015-2030, 安富莱电子 www.armfly.com
*
*********************************************************************************************************
*/
#include "bsp.h"
#include "bsp_dma.h"
#include "bsp_uart.h"

/* Private variables ---------------------------------------------------------*/

/* External variables --------------------------------------------------------*/
extern DMA_HandleTypeDef hdma_usart1_rx;
extern DMA_HandleTypeDef hdma_usart1_tx;
extern DMA_HandleTypeDef hdma_usart3_rx;
extern DMA_HandleTypeDef hdma_usart3_tx;
extern DMA_HandleTypeDef hdma_usart6_rx;
extern DMA_HandleTypeDef hdma_usart6_tx;

/**
* [bsp_Init_dma]
*
* @param   void  void  [void description]
*
* @return  void        [return description]
*/
void bsp_Init_dma(void)
{
    /* DMA controller clock enable */
    __HAL_RCC_DMA1_CLK_ENABLE();

    /* DMA interrupt init */
    /* DMA1_Stream0_IRQn interrupt configuration */
    HAL_NVIC_SetPriority(DMA1_Stream0_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(DMA1_Stream0_IRQn);
    /* DMA1_Stream1_IRQn interrupt configuration */
    HAL_NVIC_SetPriority(DMA1_Stream1_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(DMA1_Stream1_IRQn);
    /* DMA1_Stream2_IRQn interrupt configuration */
    HAL_NVIC_SetPriority(DMA1_Stream2_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(DMA1_Stream2_IRQn);
    /* DMA1_Stream3_IRQn interrupt configuration */
    HAL_NVIC_SetPriority(DMA1_Stream3_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(DMA1_Stream3_IRQn);
    /* DMA1_Stream4_IRQn interrupt configuration */
    HAL_NVIC_SetPriority(DMA1_Stream4_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(DMA1_Stream4_IRQn);
    /* DMA1_Stream5_IRQn interrupt configuration */
    HAL_NVIC_SetPriority(DMA1_Stream5_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(DMA1_Stream5_IRQn);
}

#if UART1_FIFO_EN == 1
/**
* @brief This function handles DMA1 stream0 global interrupt.
*/
void DMA1_Stream0_IRQHandler(void)
{
    /* USER CODE BEGIN DMA1_Stream0_IRQn 0 */

    /* USER CODE END DMA1_Stream0_IRQn 0 */
    HAL_DMA_IRQHandler(&hdma_usart1_rx);
    /* USER CODE BEGIN DMA1_Stream0_IRQn 1 */

    /* USER CODE END DMA1_Stream0_IRQn 1 */
}
#endif
#if UART1_FIFO_EN == 1
/**
 * @brief This function handles DMA1 stream1 global interrupt.
 */
void DMA1_Stream1_IRQHandler(void)
{
    /* USER CODE BEGIN DMA1_Stream1_IRQn 0 */

    /* USER CODE END DMA1_Stream1_IRQn 0 */
    HAL_DMA_IRQHandler(&hdma_usart1_tx);
    /* USER CODE BEGIN DMA1_Stream1_IRQn 1 */

    /* USER CODE END DMA1_Stream1_IRQn 1 */
}
#endif
#if UART3_FIFO_EN == 1
/**
 * @brief This function handles DMA1 stream2 global interrupt.
 */
void DMA1_Stream2_IRQHandler(void)
{
    /* USER CODE BEGIN DMA1_Stream2_IRQn 0 */

    /* USER CODE END DMA1_Stream2_IRQn 0 */
    HAL_DMA_IRQHandler(&hdma_usart3_rx);
    /* USER CODE BEGIN DMA1_Stream2_IRQn 1 */

    /* USER CODE END DMA1_Stream2_IRQn 1 */
}
#endif
#if UART3_FIFO_EN == 1
/**
 * @brief This function handles DMA1 stream3 global interrupt.
 */
void DMA1_Stream3_IRQHandler(void)
{
    /* USER CODE BEGIN DMA1_Stream3_IRQn 0 */

    /* USER CODE END DMA1_Stream3_IRQn 0 */
    HAL_DMA_IRQHandler(&hdma_usart3_tx);
    /* USER CODE BEGIN DMA1_Stream3_IRQn 1 */

    /* USER CODE END DMA1_Stream3_IRQn 1 */
}
#endif
#if UART6_FIFO_EN == 1
/**
 * @brief This function handles DMA1 stream4 global interrupt.
 */
void DMA1_Stream4_IRQHandler(void)
{
    /* USER CODE BEGIN DMA1_Stream4_IRQn 0 */

    /* USER CODE END DMA1_Stream4_IRQn 0 */
    HAL_DMA_IRQHandler(&hdma_usart6_rx);
    /* USER CODE BEGIN DMA1_Stream4_IRQn 1 */

    /* USER CODE END DMA1_Stream4_IRQn 1 */
}
#endif
#if UART6_FIFO_EN == 1
/**
 * @brief This function handles DMA1 stream5 global interrupt.
 */
void DMA1_Stream5_IRQHandler(void)
{
    /* USER CODE BEGIN DMA1_Stream5_IRQn 0 */

    /* USER CODE END DMA1_Stream5_IRQn 0 */
    HAL_DMA_IRQHandler(&hdma_usart6_tx);
    /* USER CODE BEGIN DMA1_Stream5_IRQn 1 */

    /* USER CODE END DMA1_Stream5_IRQn 1 */
}
#endif
/***************************** 安富莱电子 www.armfly.com (END OF FILE) *********************************/
