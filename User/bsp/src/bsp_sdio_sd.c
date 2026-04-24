/**
 *********************************************************************************************************
 * @file    bsp_sdio_sd.c
 * @author  cctv180
 * @brief   STM32H7 SDMMC1 (SD 卡) 底层驱动实现
 *********************************************************************************************************
 *
 *  模块划分（自上而下）：
 *   1. 板级常量 / 状态 / 静态变量
 *   2. HAL MSP (init / deinit) —— 注意本文件提供 MSP，Src/stm32h7xx_hal_msp.c 中同名实现已被排除
 *   3. HAL 中断与 DMA 完成回调
 *   4. 插拔检测（multi_button）
 *   5. 生命周期（bsp_Init_SD / bsp_DeInit_SD / bsp_SD_HardwareInit）
 *   6. 状态查询（bsp_SD_GetStatus / GetCardState / IsHwReady / GetCardInfo）
 *   7. 块级读写（IDMA：bsp_SD_ReadBlocks / bsp_SD_WriteBlocks / bsp_SD_Erase）
 *   8. 字节级读写（bsp_SD_Read / bsp_SD_Write）
 *   9. Shell 调试命令（含 bench / verify）
 *
 *  时钟链（默认配置）：
 *    PLL1Q (≈200 MHz)  →  SDMMCCLK  →  SDCK = SDMMCCLK / (2 * CLKDIV)
 *    HAL 会根据卡类型自动把 CLKDIV 钳到 50 MHz (High Speed)；
 *    若开启 SD_OVERCLOCK_100MHZ 则在 HAL_SD_Init 后覆盖 CLKCR 为 CLKDIV=1 (≈100 MHz)。
 *
 *  D-Cache / IDMA 约束：
 *    - 块读：传输前 invalidate、传输后再 invalidate（避免脏行污染 & 确保 CPU 读到新值）
 *    - 块写：传输前 clean（确保 CPU 新数据已刷到内存供 IDMA 读取）
 *    - Buffer 要求 32 字节对齐，否则邻近 cache 行可能被误伤；本驱动对非对齐 buffer
 *      自动走 g_sd_buf 逐块中转保证安全（性能会下降）。
 *
 *********************************************************************************************************
 */

/* Includes --------------------------------------------------------------- */
#include "bsp.h"
#include "bsp_sdio_sd.h"
#include <stdint.h>

/* ======================================================================= */
/* 1. 板级常量 / 状态 / 静态变量                                           */
/* ======================================================================= */

/* 卡插入检测引脚：PG12，低电平 = 已插入 */
#define SD_DETECT_GPIO_CLK_ENABLE() __HAL_RCC_GPIOG_CLK_ENABLE()
#define SD_DETECT_GPIO_PORT GPIOG
#define SD_DETECT_PIN GPIO_PIN_12
#define SD_IS_INSERTED() ((SD_DETECT_GPIO_PORT->IDR & SD_DETECT_PIN) == 0)

/* HAL 轮询 / 等卡就绪的统一超时（毫秒） */
#define SD_POLL_TIMEOUT_MS 5000u

/**
 * Overclock 开关：绕过 HAL 对 High Speed 50 MHz 的钳制，尝试 100 MHz SDCK。
 *   0 = 标准 50 MHz HS（默认、最稳、符合规范）
 *   1 = 约 100 MHz SDCK（非标准 3.3V 下 overclock；实测多数现代 TF 卡可稳，
 *       但换卡 / 温度变化后请用 `sd bench ... verify` 复测。出现 CRC / Timeout 请退回 0）
 */
#define SD_OVERCLOCK_100MHZ 1

/* DMA 完成标志：0 = 空闲，1 = 传输完成，2 = 错误 / Abort */
enum
{
    SD_DMA_BUSY = 0,
    SD_DMA_DONE = 1,
    SD_DMA_ERROR = 2,
};

/* 全局 handle */
SD_HandleTypeDef hsd1;

/* multi_button 插拔检测句柄 */
static Button s_sdDetectBtn;

/* 内部状态机 */
static bsp_SD_Status_t s_sd_status = BSP_SD_STATUS_ABSENT;

/* DMA 完成标志（ISR 写入，主循环轮询） */
static volatile uint8_t s_sd_dma_cplt;

/* 非对齐首尾 / 非对齐 buffer 的中转缓冲；32 字节对齐满足 D-Cache / IDMA 要求 */
ALIGN_32BYTES(static uint8_t g_sd_buf[SD_BLOCK_SIZE]);

/* 前向声明 */
static uint8_t SdDetectPinLevel(uint8_t id);
static void sd_detect_btn_cb(void *button);
static bsp_SD_Status_t SD_WaitReady(uint32_t Timeout);
static bsp_SD_Status_t sd_dma_wait(uint32_t Timeout);

/* ======================================================================= */
/* 2. HAL MSP                                                              */
/* ======================================================================= */

/**
 * @brief  SDMMC1 初始化（取代 CubeMX 生成版本，可返回错误）。
 * @retval HAL_OK 成功；其他表示 HAL_SD_Init 失败。
 */
static HAL_StatusTypeDef MX_SDMMC1_SD_Init(void)
{
    hsd1.Instance = SDMMC1;
    hsd1.Init.ClockEdge = SDMMC_CLOCK_EDGE_RISING;
    hsd1.Init.ClockPowerSave = SDMMC_CLOCK_POWER_SAVE_DISABLE;
    hsd1.Init.BusWide = SDMMC_BUS_WIDE_4B;
    hsd1.Init.HardwareFlowControl = SDMMC_HARDWARE_FLOW_CONTROL_DISABLE;
    hsd1.Init.ClockDiv = 2u;

    HAL_StatusTypeDef st = HAL_SD_Init(&hsd1);
    if (st != HAL_OK)
    {
        return st;
    }

#if SD_OVERCLOCK_100MHZ
    /* HAL 识别到 HS 卡后会把 CLKDIV 钳到 2 (SDMMCCLK 200MHz / (2*2) = 50MHz)。
     * 这里直接覆盖为 CLKDIV=1 → SDCK ≈ 100 MHz。超出 3.3V HS 规范，换卡后请
     * 用 `sd bench ... verify` 复测，出现 CRC / Timeout 立即退回 0。 */
    MODIFY_REG(hsd1.Instance->CLKCR, SDMMC_CLKCR_CLKDIV, 1u);
#endif
    return HAL_OK;
}

/**
 * @brief SD MSP Initialization
 *        This function configures the hardware resources used in this example
 * @param hsd: SD handle pointer
 * @retval None
 *
 * @note  本函数保留 CubeMX 生成的结构 / 注释 / USER CODE 标记，方便日后重新生成
 *        后与 Src/stm32h7xx_hal_msp.c 做 diff 合并。自定义项（如 NVIC 优先级、
 *        额外 GPIO 配置）应只放在 USER CODE BEGIN / END 之间的区块中。
 *        同名函数在 Src/stm32h7xx_hal_msp.c 已由 MDK project.uvprojx 置
 *        IncludeInBuild=0 排除，避免符号冲突。
 */
void HAL_SD_MspInit(SD_HandleTypeDef *hsd)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};
    if (hsd->Instance == SDMMC1)
    {
        /* USER CODE BEGIN SDMMC1_MspInit 0 */

        /* USER CODE END SDMMC1_MspInit 0 */

        /** Initializes the peripherals clock
         */
        PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_SDMMC;
        PeriphClkInitStruct.SdmmcClockSelection = RCC_SDMMCCLKSOURCE_PLL;
        if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
        {
            ERROR_HANDLER();
        }

        /* Peripheral clock enable */
        __HAL_RCC_SDMMC1_CLK_ENABLE();

        __HAL_RCC_GPIOC_CLK_ENABLE();
        __HAL_RCC_GPIOD_CLK_ENABLE();
        /**SDMMC1 GPIO Configuration
        PC10     ------> SDMMC1_D2
        PC11     ------> SDMMC1_D3
        PC12     ------> SDMMC1_CK
        PD2     ------> SDMMC1_CMD
        PC8     ------> SDMMC1_D0
        PC9     ------> SDMMC1_D1
        */
        GPIO_InitStruct.Pin = GPIO_PIN_10 | GPIO_PIN_11 | GPIO_PIN_12 | GPIO_PIN_8 | GPIO_PIN_9;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        GPIO_InitStruct.Alternate = GPIO_AF12_SDIO1;
        HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

        GPIO_InitStruct.Pin = GPIO_PIN_2;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        GPIO_InitStruct.Alternate = GPIO_AF12_SDIO1;
        HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

        /* SDMMC1 interrupt Init */
        HAL_NVIC_SetPriority(SDMMC1_IRQn, 0, 0);
        HAL_NVIC_EnableIRQ(SDMMC1_IRQn);
        /* USER CODE BEGIN SDMMC1_MspInit 1 */
        /* 项目优先级策略：让出 0..4 给更高优先级中断，SDMMC 统一使用 5 */
        HAL_NVIC_SetPriority(SDMMC1_IRQn, 5, 0);
        /* USER CODE END SDMMC1_MspInit 1 */
    }
}

/**
 * @brief SD MSP De-Initialization
 *        This function freeze the hardware resources used in this example
 * @param hsd: SD handle pointer
 * @retval None
 */
void HAL_SD_MspDeInit(SD_HandleTypeDef *hsd)
{
    if (hsd->Instance == SDMMC1)
    {
        /* USER CODE BEGIN SDMMC1_MspDeInit 0 */

        /* USER CODE END SDMMC1_MspDeInit 0 */
        /* Peripheral clock disable */
        __HAL_RCC_SDMMC1_CLK_DISABLE();

        /**SDMMC1 GPIO Configuration
        PC10     ------> SDMMC1_D2
        PC11     ------> SDMMC1_D3
        PC12     ------> SDMMC1_CK
        PD2     ------> SDMMC1_CMD
        PC8     ------> SDMMC1_D0
        PC9     ------> SDMMC1_D1
        */
        HAL_GPIO_DeInit(GPIOC, GPIO_PIN_10 | GPIO_PIN_11 | GPIO_PIN_12 | GPIO_PIN_8 | GPIO_PIN_9);

        HAL_GPIO_DeInit(GPIOD, GPIO_PIN_2);

        /* SDMMC1 interrupt DeInit */
        HAL_NVIC_DisableIRQ(SDMMC1_IRQn);
        /* USER CODE BEGIN SDMMC1_MspDeInit 1 */

        /* USER CODE END SDMMC1_MspDeInit 1 */
    }
}

/* ======================================================================= */
/* 3. 中断与 DMA 完成回调                                                   */
/* ======================================================================= */

void SDMMC1_IRQHandler(void)
{
    HAL_SD_IRQHandler(&hsd1);
}

void HAL_SD_TxCpltCallback(SD_HandleTypeDef *hsd)
{
    (void)hsd;
    s_sd_dma_cplt = SD_DMA_DONE;
}

void HAL_SD_RxCpltCallback(SD_HandleTypeDef *hsd)
{
    (void)hsd;
    s_sd_dma_cplt = SD_DMA_DONE;
}

void HAL_SD_ErrorCallback(SD_HandleTypeDef *hsd)
{
    (void)hsd;
    s_sd_dma_cplt = SD_DMA_ERROR;
}

void HAL_SD_AbortCallback(SD_HandleTypeDef *hsd)
{
    (void)hsd;
    s_sd_dma_cplt = SD_DMA_ERROR;
}

/* ======================================================================= */
/* 4. 插拔检测 (multi_button)                                               */
/* ======================================================================= */

/**
 * @brief  multi_button 读脚电平回调。
 * @retval 0 = 已插入（active_level=0，库视为“按下”），1 = 未插入。
 */
static uint8_t SdDetectPinLevel(uint8_t id)
{
    (void)id;
    return SD_IS_INSERTED() ? 0u : 1u;
}

/**
 * @brief  multi_button 事件回调。
 * @note   LONG_PRESS_START：卡稳定插入 ≥ 库内长按阈值（约 1s）后触发一次 → HardwareInit。
 *         PRESS_UP：检测脚由插入变为拔出 → DeInit，并根据之前状态置 EJECTED / ABSENT。
 */
static void sd_detect_btn_cb(void *button)
{
    struct Button *btn = (struct Button *)button;
    uint8_t ev = (uint8_t)get_button_event(btn);

    if (ev == LONG_PRESS_START)
    {
        (void)bsp_SD_HardwareInit();
    }
    else if (ev == PRESS_UP)
    {
        bsp_SD_Status_t prev = s_sd_status;
        if (prev == BSP_SD_STATUS_READY || prev == BSP_SD_STATUS_INIT_FAILED)
        {
            (void)bsp_DeInit_SD();
        }
        /* READY 才记“热拔出”给上层消费；其他情况直接回 ABSENT */
        s_sd_status = (prev == BSP_SD_STATUS_READY) ? BSP_SD_STATUS_EJECTED : BSP_SD_STATUS_ABSENT;
    }
}

/* ======================================================================= */
/* 5. 生命周期                                                              */
/* ======================================================================= */

/**
 * @brief  初始化 SD 检测 GPIO 并注册 multi_button。
 * @note   依赖工程中已周期调用 button_ticks()（通常 5ms tick）。
 */
bsp_SD_Status_t bsp_Init_SD(void)
{
    SD_DETECT_GPIO_CLK_ENABLE();

    GPIO_InitTypeDef gpio_init = {0};
    gpio_init.Mode = GPIO_MODE_INPUT;
    gpio_init.Pull = GPIO_NOPULL;
    gpio_init.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio_init.Pin = SD_DETECT_PIN;
    HAL_GPIO_Init(SD_DETECT_GPIO_PORT, &gpio_init);

    button_init(&s_sdDetectBtn, SdDetectPinLevel, 0u, 0u);
    button_attach(&s_sdDetectBtn, LONG_PRESS_START, sd_detect_btn_cb);
    button_attach(&s_sdDetectBtn, PRESS_UP, sd_detect_btn_cb);
    button_start(&s_sdDetectBtn);

    return bsp_SD_GetStatus();
}

/**
 * @brief  释放 SDMMC 外设资源。
 * @note   卡仍在位 → INSERTED_NOT_READY（可重新 init）；不在位 → ABSENT。
 */
bsp_SD_Status_t bsp_DeInit_SD(void)
{
    if (s_sd_status != BSP_SD_STATUS_READY && s_sd_status != BSP_SD_STATUS_INIT_FAILED)
    {
        return bsp_SD_GetStatus();
    }
    hsd1.Instance = SDMMC1;
    HAL_SD_DeInit(&hsd1);
    s_sd_status = SD_IS_INSERTED() ? BSP_SD_STATUS_INSERTED_NOT_READY : BSP_SD_STATUS_ABSENT;
    return s_sd_status;
}

/**
 * @brief  执行 HAL_SD_Init。已 READY 则直接返回成功。
 */
bsp_SD_Status_t bsp_SD_HardwareInit(void)
{
    if (s_sd_status == BSP_SD_STATUS_READY)
    {
        return BSP_SD_STATUS_READY;
    }
    if (!SD_IS_INSERTED())
    {
        s_sd_status = BSP_SD_STATUS_ABSENT;
        return BSP_SD_STATUS_ABSENT;
    }
    if (MX_SDMMC1_SD_Init() != HAL_OK)
    {
        s_sd_status = BSP_SD_STATUS_INIT_FAILED;
        return BSP_SD_STATUS_INIT_FAILED;
    }
    s_sd_status = BSP_SD_STATUS_READY;
    return BSP_SD_STATUS_READY;
}

/* ======================================================================= */
/* 6. 状态查询                                                              */
/* ======================================================================= */

/**
 * @brief  综合状态。
 * @note   EJECTED 为“一次性”状态：首次读到返回 EJECTED，随后自动降为 ABSENT，
 *         确保上层不会漏掉热拔出事件，也不会反复处理。
 */
bsp_SD_Status_t bsp_SD_GetStatus(void)
{
    /* 热拔出一次性消费 */
    if (s_sd_status == BSP_SD_STATUS_EJECTED)
    {
        s_sd_status = BSP_SD_STATUS_ABSENT;
        return BSP_SD_STATUS_EJECTED;
    }

    /* 物理引脚二次校验（应对中断抢占 / 竞态） */
    if (!SD_IS_INSERTED())
    {
        if (s_sd_status == BSP_SD_STATUS_READY)
        {
            s_sd_status = BSP_SD_STATUS_EJECTED;
            return BSP_SD_STATUS_EJECTED;
        }
        s_sd_status = BSP_SD_STATUS_ABSENT;
        return BSP_SD_STATUS_ABSENT;
    }

    return s_sd_status;
}

bsp_SD_Status_t bsp_SD_GetCardState(void)
{
    if (s_sd_status != BSP_SD_STATUS_READY)
    {
        return bsp_SD_GetStatus();
    }
    return (HAL_SD_GetCardState(&hsd1) == HAL_SD_CARD_TRANSFER)
               ? BSP_SD_STATUS_READY
               : BSP_SD_STATUS_INSERTED_NOT_READY;
}

uint8_t bsp_SD_IsHwReady(void)
{
    return (s_sd_status == BSP_SD_STATUS_READY) ? 1u : 0u;
}

void bsp_SD_GetCardInfo(HAL_SD_CardInfoTypeDef *CardInfo)
{
    if (s_sd_status != BSP_SD_STATUS_READY || CardInfo == NULL)
    {
        return;
    }
    HAL_SD_GetCardInfo(&hsd1, CardInfo);
}

/* ======================================================================= */
/* 7. 块级读写（IDMA）                                                      */
/* ======================================================================= */

/**
 * @brief  等 SD 卡回到 TRANSFER 状态（程序完成等）。
 */
static bsp_SD_Status_t SD_WaitReady(uint32_t Timeout)
{
    if (s_sd_status != BSP_SD_STATUS_READY)
    {
        return s_sd_status;
    }
    uint32_t t0 = HAL_GetTick();
    while ((HAL_GetTick() - t0) < Timeout)
    {
        if (HAL_SD_GetCardState(&hsd1) == HAL_SD_CARD_TRANSFER)
        {
            return BSP_SD_STATUS_READY;
        }
    }
    return BSP_SD_STATUS_INSERTED_NOT_READY;
}

/**
 * @brief  等 DMA 完成标志（由 TxCplt / RxCplt / Error / Abort 回调置位）。
 */
static bsp_SD_Status_t sd_dma_wait(uint32_t Timeout)
{
    uint32_t t0 = HAL_GetTick();
    while (s_sd_dma_cplt == SD_DMA_BUSY)
    {
        if ((HAL_GetTick() - t0) > Timeout)
        {
            return BSP_SD_STATUS_INSERTED_NOT_READY;
        }
    }
    return (s_sd_dma_cplt == SD_DMA_DONE) ? BSP_SD_STATUS_READY : bsp_SD_GetStatus();
}

bsp_SD_Status_t bsp_SD_ReadBlocks(uint32_t *pData, uint32_t BlockAddr, uint32_t NumOfBlocks)
{
    if (s_sd_status != BSP_SD_STATUS_READY || pData == NULL || NumOfBlocks == 0u)
    {
        return bsp_SD_GetStatus();
    }

    uint32_t bytes = NumOfBlocks * SD_BLOCK_SIZE;

    /* 传输前 invalidate：丢弃可能存在的脏行，避免 IDMA 写完后被 evict 覆盖新数据 */
    SCB_InvalidateDCache_by_Addr(pData, bytes);

    s_sd_dma_cplt = SD_DMA_BUSY;
    if (HAL_SD_ReadBlocks_DMA(&hsd1, (uint8_t *)pData, BlockAddr, NumOfBlocks) != HAL_OK)
    {
        return bsp_SD_GetStatus();
    }

    bsp_SD_Status_t r = sd_dma_wait(SD_POLL_TIMEOUT_MS);
    if (r != BSP_SD_STATUS_READY)
    {
        HAL_SD_Abort(&hsd1);
        return r;
    }

    /* 传输后 invalidate：确保 CPU 读时 cache miss，从内存重新加载 IDMA 写入的值 */
    SCB_InvalidateDCache_by_Addr(pData, bytes);

    return SD_WaitReady(SD_POLL_TIMEOUT_MS);
}

bsp_SD_Status_t bsp_SD_WriteBlocks(uint32_t *pData, uint32_t BlockAddr, uint32_t NumOfBlocks)
{
    if (s_sd_status != BSP_SD_STATUS_READY || pData == NULL || NumOfBlocks == 0u)
    {
        return bsp_SD_GetStatus();
    }

    uint32_t bytes = NumOfBlocks * SD_BLOCK_SIZE;

    /* 传输前 clean：CPU 最新数据刷到内存供 IDMA 读取 */
    SCB_CleanDCache_by_Addr(pData, bytes);

    s_sd_dma_cplt = SD_DMA_BUSY;
    if (HAL_SD_WriteBlocks_DMA(&hsd1, (uint8_t *)pData, BlockAddr, NumOfBlocks) != HAL_OK)
    {
        return bsp_SD_GetStatus();
    }

    bsp_SD_Status_t r = sd_dma_wait(SD_POLL_TIMEOUT_MS);
    if (r != BSP_SD_STATUS_READY)
    {
        HAL_SD_Abort(&hsd1);
        return r;
    }

    /* 等卡 program 完成（HAL TxCplt 只表示 SDMMC 发完数据） */
    return SD_WaitReady(SD_POLL_TIMEOUT_MS);
}

bsp_SD_Status_t bsp_SD_Erase(uint32_t BlockStart, uint32_t BlockEnd)
{
    if (s_sd_status != BSP_SD_STATUS_READY)
    {
        return bsp_SD_GetStatus();
    }
    return (HAL_SD_Erase(&hsd1, BlockStart, BlockEnd) == HAL_OK)
               ? BSP_SD_STATUS_READY
               : bsp_SD_GetStatus();
}

/* ======================================================================= */
/* 8. 字节级读写                                                            */
/* ======================================================================= */

/* pBuffer 是否 32 字节对齐（D-Cache 安全路径判据） */
#define IS_DCACHE_ALIGNED(p) (((uintptr_t)(p) & 0x1Fu) == 0u)

/**
 * @brief  任意地址 / 任意大小读取，内部分“头 / 中 / 尾”三段。
 * @note   中段若 pBuffer 未 32 字节对齐 → 逐块经 g_sd_buf 中转（D-Cache 安全）。
 */
bsp_SD_Status_t bsp_SD_Read(uint8_t *pBuffer, uint32_t ReadAddr, uint32_t Size)
{
    if (pBuffer == NULL || Size == 0u)
    {
        return bsp_SD_GetStatus();
    }

    /* 1. 头部非对齐字节（RMW 读一整块后拷出需要的部分） */
    uint32_t offset = ReadAddr & 0x1FFu;
    if (offset != 0u)
    {
        uint32_t block_addr = ReadAddr / SD_BLOCK_SIZE;
        uint32_t count = SD_BLOCK_SIZE - offset;
        if (count > Size)
        {
            count = Size;
        }

        if (bsp_SD_ReadBlocks((uint32_t *)g_sd_buf, block_addr, 1u) != BSP_SD_STATUS_READY)
        {
            return bsp_SD_GetStatus();
        }
        memcpy(pBuffer, g_sd_buf + offset, count);

        pBuffer += count;
        ReadAddr += count;
        Size -= count;
    }

    /* 2. 中段整块 */
    if (Size >= SD_BLOCK_SIZE)
    {
        uint32_t full_blocks = Size / SD_BLOCK_SIZE;
        uint32_t block_addr = ReadAddr / SD_BLOCK_SIZE;

        if (IS_DCACHE_ALIGNED(pBuffer))
        {
            if (bsp_SD_ReadBlocks((uint32_t *)pBuffer, block_addr, full_blocks) != BSP_SD_STATUS_READY)
            {
                return bsp_SD_GetStatus();
            }
        }
        else
        {
            for (uint32_t i = 0u; i < full_blocks; i++)
            {
                if (bsp_SD_ReadBlocks((uint32_t *)g_sd_buf, block_addr + i, 1u) != BSP_SD_STATUS_READY)
                {
                    return bsp_SD_GetStatus();
                }
                memcpy(pBuffer + i * SD_BLOCK_SIZE, g_sd_buf, SD_BLOCK_SIZE);
            }
        }

        uint32_t bytes = full_blocks * SD_BLOCK_SIZE;
        pBuffer += bytes;
        ReadAddr += bytes;
        Size -= bytes;
    }

    /* 3. 尾部不足一整块 */
    if (Size != 0u)
    {
        uint32_t block_addr = ReadAddr / SD_BLOCK_SIZE;
        if (bsp_SD_ReadBlocks((uint32_t *)g_sd_buf, block_addr, 1u) != BSP_SD_STATUS_READY)
        {
            return bsp_SD_GetStatus();
        }
        memcpy(pBuffer, g_sd_buf, Size);
    }

    return BSP_SD_STATUS_READY;
}

/**
 * @brief  任意地址 / 任意大小写入。
 * @note   首尾非 512 对齐部分走 Read-Modify-Write；
 *         中段若 pBuffer 未 32 字节对齐 → 逐块经 g_sd_buf 中转。
 */
bsp_SD_Status_t bsp_SD_Write(const uint8_t *pBuffer, uint32_t WriteAddr, uint32_t Size)
{
    if (pBuffer == NULL || Size == 0u)
    {
        return bsp_SD_GetStatus();
    }

    /* 1. 头部非对齐字节（RMW） */
    uint32_t offset = WriteAddr & 0x1FFu;
    if (offset != 0u)
    {
        uint32_t block_addr = WriteAddr / SD_BLOCK_SIZE;
        uint32_t count = SD_BLOCK_SIZE - offset;
        if (count > Size)
        {
            count = Size;
        }

        if (bsp_SD_ReadBlocks((uint32_t *)g_sd_buf, block_addr, 1u) != BSP_SD_STATUS_READY)
        {
            return bsp_SD_GetStatus();
        }
        memcpy(g_sd_buf + offset, pBuffer, count);
        if (bsp_SD_WriteBlocks((uint32_t *)g_sd_buf, block_addr, 1u) != BSP_SD_STATUS_READY)
        {
            return bsp_SD_GetStatus();
        }

        pBuffer += count;
        WriteAddr += count;
        Size -= count;
    }

    /* 2. 中段整块 */
    if (Size >= SD_BLOCK_SIZE)
    {
        uint32_t full_blocks = Size / SD_BLOCK_SIZE;
        uint32_t block_addr = WriteAddr / SD_BLOCK_SIZE;

        if (IS_DCACHE_ALIGNED(pBuffer))
        {
            if (bsp_SD_WriteBlocks((uint32_t *)pBuffer, block_addr, full_blocks) != BSP_SD_STATUS_READY)
            {
                return bsp_SD_GetStatus();
            }
        }
        else
        {
            for (uint32_t i = 0u; i < full_blocks; i++)
            {
                memcpy(g_sd_buf, pBuffer + i * SD_BLOCK_SIZE, SD_BLOCK_SIZE);
                if (bsp_SD_WriteBlocks((uint32_t *)g_sd_buf, block_addr + i, 1u) != BSP_SD_STATUS_READY)
                {
                    return bsp_SD_GetStatus();
                }
            }
        }

        uint32_t bytes = full_blocks * SD_BLOCK_SIZE;
        pBuffer += bytes;
        WriteAddr += bytes;
        Size -= bytes;
    }

    /* 3. 尾部不足一整块（RMW） */
    if (Size != 0u)
    {
        uint32_t block_addr = WriteAddr / SD_BLOCK_SIZE;
        if (bsp_SD_ReadBlocks((uint32_t *)g_sd_buf, block_addr, 1u) != BSP_SD_STATUS_READY)
        {
            return bsp_SD_GetStatus();
        }
        memcpy(g_sd_buf, pBuffer, Size);
        if (bsp_SD_WriteBlocks((uint32_t *)g_sd_buf, block_addr, 1u) != BSP_SD_STATUS_READY)
        {
            return bsp_SD_GetStatus();
        }
    }

    return BSP_SD_STATUS_READY;
}

/* ======================================================================= */
/* 9. Shell 调试命令                                                        */
/* ======================================================================= */

#if defined(__SHELL_H__) && defined(DEBUG_MODE)
#include "dump_hex.h"

static const char *sd_status_str(bsp_SD_Status_t st)
{
    switch (st)
    {
    case BSP_SD_STATUS_ABSENT:
        return "ABSENT";
    case BSP_SD_STATUS_INSERTED_NOT_READY:
        return "INSERTED_NOT_READY";
    case BSP_SD_STATUS_READY:
        return "READY";
    case BSP_SD_STATUS_INIT_FAILED:
        return "INIT_FAILED";
    case BSP_SD_STATUS_EJECTED:
        return "EJECTED";
    default:
        return "UNKNOWN";
    }
}

static void sd_cmd_help(void)
{
    printf("Usage: sd <command> [args]\r\n");
    printf("Commands:\r\n");
    printf("  status                         - Show SD card status\r\n");
    printf("  init                           - Initialize SD card\r\n");
    printf("  deinit                         - Release SDMMC peripheral\r\n");
    printf("  info                           - Dump card info & capacity\r\n");
    printf("  read  <addr> <size>            - Read bytes (max 2KB, hex dump)\r\n");
    printf("  write <addr> <string>          - Write string (byte-aligned)\r\n");
    printf("  erase <blk_start> <blk_end>    - Erase block range\r\n");
    printf("  bench <total> [step] [base] [verify]\r\n");
    printf("       Bench IDMA R/W.  step default 32768 (大 step 更贴近卡上限)\r\n");
    printf("       base default 0x01000000 (跳过 MBR/元数据，避免首扇区 busy)\r\n");
    printf("       verify: 额外传 'verify' 时，每块写入递增 pattern 再读回校验 (验 overclock)\r\n");
}

/* ---- bench：pattern 生成 / 校验 --------------------------------------- */

static void bench_fill_pattern(uint32_t *buf, uint32_t block_addr, uint32_t bytes)
{
    uint32_t words = bytes / 4u;
    for (uint32_t i = 0u; i < words; i++)
    {
        buf[i] = block_addr + i;
    }
}

/* 返回：0 = 全部匹配；非 0 = 首个失败的 word index + 1 */
static uint32_t bench_check_pattern(const uint32_t *buf, uint32_t block_addr, uint32_t bytes,
                                    uint32_t *bad_expected, uint32_t *bad_got)
{
    uint32_t words = bytes / 4u;
    for (uint32_t i = 0u; i < words; i++)
    {
        uint32_t expected = block_addr + i;
        if (buf[i] != expected)
        {
            if (bad_expected)
                *bad_expected = expected;
            if (bad_got)
                *bad_got = buf[i];
            return i + 1u;
        }
    }
    return 0u;
}

/* ---- sd info 打印抽离 -------------------------------------------------- */

static int sd_cmd_info(void)
{
    if (!bsp_SD_IsHwReady())
    {
        printf("SD hardware not initialized (insert long-press, or run: sd init).\r\n");
        return -1;
    }

    printf("\r\n*** hsd1.SdCard ***\r\n");
    printf("BlockNbr     : %lX\r\n", (unsigned long)hsd1.SdCard.BlockNbr);
    printf("BlockSize    : %lX\r\n", (unsigned long)hsd1.SdCard.BlockSize);
    printf("CardSpeed    : %lX\r\n", (unsigned long)hsd1.SdCard.CardSpeed);
    printf("CardType     : %lX\r\n", (unsigned long)hsd1.SdCard.CardType);
    printf("CardVersion  : %lX\r\n", (unsigned long)hsd1.SdCard.CardVersion);
    printf("Class        : %lX\r\n", (unsigned long)hsd1.SdCard.Class);
    printf("LogBlockNbr  : %lX\r\n", (unsigned long)hsd1.SdCard.LogBlockNbr);
    printf("LogBlockSize : %lX\r\n", (unsigned long)hsd1.SdCard.LogBlockSize);
    printf("RelCardAdd   : %lX\r\n", (unsigned long)hsd1.SdCard.RelCardAdd);

    HAL_SD_CardInfoTypeDef info;
    if (HAL_SD_GetCardInfo(&hsd1, &info) != HAL_OK)
    {
        printf("HAL_SD_GetCardInfo() error\r\n");
        return -1;
    }
    printf("\r\n*** HAL_SD_GetCardInfo ***\r\n");
    printf("Capacity(MB) : %lu\r\n", (unsigned long)(info.BlockNbr >> 1U >> 10U));
    printf("BlockCount   : %lu\r\n", (unsigned long)info.BlockNbr);
    printf("BlockSize    : %lu\r\n", (unsigned long)info.BlockSize);

    HAL_SD_CardStatusTypeDef status;
    if (HAL_SD_GetCardStatus(&hsd1, &status) != HAL_OK)
    {
        printf("HAL_SD_GetCardStatus() error\r\n");
        return -1;
    }
    printf("\r\n*** HAL_SD_GetCardStatus ***\r\n");
    printf("DataBusWidth       : %u\r\n", (unsigned)status.DataBusWidth);
    printf("SpeedClass         : %u\r\n", (unsigned)status.SpeedClass);
    printf("AllocationUnitSize : %u\r\n", (unsigned)status.AllocationUnitSize);
    printf("EraseSize          : %u\r\n", (unsigned)status.EraseSize);
    printf("EraseTimeout       : %u\r\n", (unsigned)status.EraseTimeout);
    return 0;
}

/* ---- sd bench ---------------------------------------------------------- */

static int sd_cmd_bench(int argc, char *argv[])
{
    if (argc < 3)
    {
        printf("Usage: sd bench <total_size> [step] [byte_start] [verify]\r\n");
        printf("  step       default 32768 (大 step 减少 CMD25/CMD18 次数，更贴近卡上限)\r\n");
        printf("  byte_start default 0x1000000 (跳过 MBR/元数据，避免首扇区 busy)\r\n");
        printf("  verify     传 'verify' 会写入递增 pattern 并读回校验，验 overclock 稳定性\r\n");
        return -1;
    }

    uint32_t total_size = strtoul(argv[2], NULL, 0);
    uint32_t step = (argc >= 4) ? strtoul(argv[3], NULL, 0) : 32768u;
    uint32_t base = (argc >= 5) ? strtoul(argv[4], NULL, 0) : 0x01000000u;

    /* verify 可以出现在第 5 或第 6 个参数位 */
    int do_verify = 0;
    for (int i = 3; i < argc; i++)
    {
        if (strcmp(argv[i], "verify") == 0)
        {
            do_verify = 1;
            break;
        }
    }

    if (total_size == 0u || step == 0u)
    {
        printf("Error: total_size and step must be non-zero.\r\n");
        return -1;
    }
    if ((base & 0x1FFu) != 0u)
    {
        printf("Error: byte_start must be 512B aligned.\r\n");
        return -1;
    }

    /* malloc 未必 32 字节对齐；对齐后再当 DMA buffer 使用 */
    uint8_t *raw = malloc(step + 32u);
    if (!raw)
    {
        printf("Error: malloc(%u) failed.\r\n", (unsigned)(step + 32u));
        return -1;
    }
    uint8_t *buf = (uint8_t *)(((uintptr_t)raw + 31u) & ~(uintptr_t)31u);

    uint32_t nchk = (total_size + step - 1u) / step;

    printf("Benchmarking SD (Total: %u, Step: %u, Base: 0x%08X, Verify: %d) buf=%p\r\n",
           (unsigned)total_size, (unsigned)step, (unsigned)base, do_verify, (void *)buf);

    /* ---- Write pass ---- */
    if (!do_verify)
    {
        memset(buf, 0x55, step);
    }
    uint32_t t0 = HAL_GetTick();
    for (uint32_t off = 0u, ci = 0u; off < total_size; off += step, ci++)
    {
        uint32_t cur_size = (off + step > total_size) ? (total_size - off) : step;
        uint32_t block_addr = (base + off) / SD_BLOCK_SIZE;

        if (do_verify)
        {
            bench_fill_pattern((uint32_t *)buf, block_addr, cur_size & ~0x1FFu);
        }

        if ((ci & 7u) == 0u)
        {
            printf("  write %u/%u, off+base=0x%08X\r\n", (unsigned)(ci + 1u), (unsigned)nchk, (unsigned)(base + off));
        }

        if (bsp_SD_Write(buf, base + off, cur_size) != BSP_SD_STATUS_READY)
        {
            printf("Error: Write failed at 0x%08X\r\n", (unsigned)(base + off));
            free(raw);
            return -1;
        }
    }
    uint32_t cost = HAL_GetTick() - t0;
    printf("Write: %u bytes in %u ms, Speed: %.2f KB/s\r\n",
           (unsigned)total_size, (unsigned)cost,
           (cost > 0u) ? ((float)total_size / 1024.0f) / ((float)cost / 1000.0f) : 0.0f);

    /* ---- Read pass ---- */
    printf("Read pass...\r\n");
    t0 = HAL_GetTick();
    for (uint32_t off = 0u, ci = 0u; off < total_size; off += step, ci++)
    {
        uint32_t cur_size = (off + step > total_size) ? (total_size - off) : step;
        uint32_t block_addr = (base + off) / SD_BLOCK_SIZE;

        if ((ci & 7u) == 0u)
        {
            printf("  read %u/%u, off+base=0x%08X\r\n", (unsigned)(ci + 1u), (unsigned)nchk, (unsigned)(base + off));
        }

        if (bsp_SD_Read(buf, base + off, cur_size) != BSP_SD_STATUS_READY)
        {
            printf("Error: Read failed at 0x%08X\r\n", (unsigned)(base + off));
            free(raw);
            return -1;
        }

        if (do_verify)
        {
            uint32_t bad_exp = 0u, bad_got = 0u;
            uint32_t fail_idx = bench_check_pattern((const uint32_t *)buf, block_addr,
                                                    cur_size & ~0x1FFu, &bad_exp, &bad_got);
            if (fail_idx != 0u)
            {
                printf("Verify FAIL at 0x%08X word[%u]: got=0x%08X expected=0x%08X\r\n",
                       (unsigned)(base + off), (unsigned)(fail_idx - 1u),
                       (unsigned)bad_got, (unsigned)bad_exp);
                free(raw);
                return -1;
            }
        }
    }
    cost = HAL_GetTick() - t0;
    printf("Read : %u bytes in %u ms, Speed: %.2f KB/s\r\n",
           (unsigned)total_size, (unsigned)cost,
           (cost > 0u) ? ((float)total_size / 1024.0f) / ((float)cost / 1000.0f) : 0.0f);

    if (do_verify)
    {
        printf("Verify: OK (all %u bytes match pattern)\r\n", (unsigned)total_size);
    }

    free(raw);
    return 0;
}

/* ---- 总入口 ------------------------------------------------------------ */

static int _sd_cmd(int argc, char *argv[])
{
    if (argc < 2)
    {
        sd_cmd_help();
        return -1;
    }

    const char *subcmd = argv[1];

    if (strcmp(subcmd, "status") == 0)
    {
        bsp_SD_Status_t st = bsp_SD_GetStatus();
        printf("SD status: %s (%u)\r\n", sd_status_str(st), (unsigned)st);
    }
    else if (strcmp(subcmd, "init") == 0)
    {
        bsp_SD_Status_t res = bsp_SD_HardwareInit();
        printf("SD init: %s\r\n", sd_status_str(res));
    }
    else if (strcmp(subcmd, "deinit") == 0)
    {
        bsp_SD_Status_t res = bsp_DeInit_SD();
        printf("SD deinit: %s\r\n", sd_status_str(res));
    }
    else if (strcmp(subcmd, "info") == 0)
    {
        return sd_cmd_info();
    }
    else if (strcmp(subcmd, "read") == 0)
    {
        if (argc < 4)
        {
            printf("Usage: sd read <addr> <size>\r\n");
            return -1;
        }
        uint32_t addr = strtoul(argv[2], NULL, 0);
        uint32_t size = strtoul(argv[3], NULL, 0);

        if (size == 0u || size > 2048u)
        {
            printf("Error: Size must be 1-2048 bytes for hex dump.\r\n");
            return -1;
        }

        uint8_t *buf = malloc(size);
        if (!buf)
            return -1;

        if (bsp_SD_Read(buf, addr, size) == BSP_SD_STATUS_READY)
        {
            printf("Read %u bytes from 0x%08X successfully.\r\n", (unsigned)size, (unsigned)addr);
            dump_hex(buf, size, 16);
        }
        else
        {
            printf("Error: Read failed.\r\n");
        }
        free(buf);
    }
    else if (strcmp(subcmd, "write") == 0)
    {
        if (argc < 4)
        {
            printf("Usage: sd write <addr> <string>\r\n");
            return -1;
        }
        uint32_t addr = strtoul(argv[2], NULL, 0);
        const char *data = argv[3];
        uint32_t size = strlen(data);

        if (bsp_SD_Write((const uint8_t *)data, addr, size) == BSP_SD_STATUS_READY)
        {
            printf("Wrote %u bytes to 0x%08X successfully.\r\n", (unsigned)size, (unsigned)addr);
        }
        else
        {
            printf("Error: Write failed.\r\n");
        }
    }
    else if (strcmp(subcmd, "erase") == 0)
    {
        if (argc < 4)
        {
            printf("Usage: sd erase <blk_start> <blk_end>\r\n");
            return -1;
        }
        uint32_t start = strtoul(argv[2], NULL, 0);
        uint32_t end = strtoul(argv[3], NULL, 0);

        if (bsp_SD_Erase(start, end) == BSP_SD_STATUS_READY)
        {
            printf("Erase blocks [%u, %u] OK.\r\n", (unsigned)start, (unsigned)end);
        }
        else
        {
            printf("Error: Erase failed.\r\n");
        }
    }
    else if (strcmp(subcmd, "bench") == 0)
    {
        return sd_cmd_bench(argc, argv);
    }
    else
    {
        sd_cmd_help();
    }

    return 0;
}

SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), sd, _sd_cmd, sd card debug tool);
#endif /* __SHELL_H__ && DEBUG_MODE */

/************************ (C) COPYRIGHT cctv180 *****END OF FILE****/
