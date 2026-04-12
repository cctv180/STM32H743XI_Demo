/**
 * @file bsp_sdio_sd.c
 * @author cctv180
 * @brief SD卡驱动，使用SDIO接口
 * @version 0.1
 * @date 2026-01-12
 *
 * @copyright Copyright (c) 2026
 *
 */

/* Includes ------------------------------------------------------------------*/
#include "bsp.h"
#include "bsp_sdio_sd.h"

/* 卡插入引脚 : PG12 */
#define SD_DETECT_GPIO_CLK_ENABLE() __HAL_RCC_GPIOG_CLK_ENABLE()
#define SD_DETECT_GPIO_PORT GPIOG
#define SD_DETECT_PIN GPIO_PIN_12
/* 0 表示卡插入 */
#define SD_IS_INSERTED() ((SD_DETECT_GPIO_PORT->IDR & SD_DETECT_PIN) == 0)

SD_HandleTypeDef hsd1;

/* 重新定义超时时间为 5000ms */
#undef SD_DATATIMEOUT
#define SD_DATATIMEOUT 5000

/* 静态缓冲区，用于处理非对齐读写 */
static uint8_t g_sd_buf[512];

/**
 * @brief  等待SD卡准备就绪（进入传输状态）
 */
static uint8_t SD_WaitReady(uint32_t Timeout)
{
    uint32_t start = HAL_GetTick();
    while (HAL_GetTick() - start < Timeout)
    {
        if (HAL_SD_GetCardState(&hsd1) == HAL_SD_CARD_TRANSFER)
        {
            return MSD_OK;
        }
    }
    return MSD_ERROR;
}

/**
 * @brief SDMMC1 Initialization Function
 * @param None
 * @retval None
 */
static void MX_SDMMC1_SD_Init(void)
{
    hsd1.Instance = SDMMC1;
    hsd1.Init.ClockEdge = SDMMC_CLOCK_EDGE_RISING;
    hsd1.Init.ClockPowerSave = SDMMC_CLOCK_POWER_SAVE_DISABLE;
    hsd1.Init.BusWide = SDMMC_BUS_WIDE_4B;
    hsd1.Init.HardwareFlowControl = SDMMC_HARDWARE_FLOW_CONTROL_DISABLE;
    hsd1.Init.ClockDiv = 2;
    if (HAL_SD_Init(&hsd1) != HAL_OK)
    {
        ERROR_HANDLER();
    }
}

/**
 * @brief SD MSP Initialization
 */
void HAL_SD_MspInit(SD_HandleTypeDef *hsd)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};
    if (hsd->Instance == SDMMC1)
    {
        PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_SDMMC;
        PeriphClkInitStruct.SdmmcClockSelection = RCC_SDMMCCLKSOURCE_PLL;
        if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
        {
            ERROR_HANDLER();
        }

        __HAL_RCC_SDMMC1_CLK_ENABLE();
        __HAL_RCC_GPIOC_CLK_ENABLE();
        __HAL_RCC_GPIOD_CLK_ENABLE();

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
    }
}

/**
 * @brief SD MSP De-Initialization
 */
void HAL_SD_MspDeInit(SD_HandleTypeDef *hsd)
{
    if (hsd->Instance == SDMMC1)
    {
        __HAL_RCC_SDMMC1_CLK_DISABLE();
        HAL_GPIO_DeInit(GPIOC, GPIO_PIN_10 | GPIO_PIN_11 | GPIO_PIN_12 | GPIO_PIN_8 | GPIO_PIN_9);
        HAL_GPIO_DeInit(GPIOD, GPIO_PIN_2);
    }
}

/**
 * @brief  Initializes the SD card device.
 */
uint8_t bsp_Init_SD(void)
{
    GPIO_InitTypeDef gpio_init;
    SD_DETECT_GPIO_CLK_ENABLE();
    gpio_init.Mode = GPIO_MODE_INPUT;
    gpio_init.Pull = GPIO_NOPULL;
    gpio_init.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio_init.Pin = SD_DETECT_PIN;
    HAL_GPIO_Init(SD_DETECT_GPIO_PORT, &gpio_init);

    if (bsp_SD_IsDetected() != SD_PRESENT)
    {
        return MSD_ERROR_SD_NOT_PRESENT;
    }

    MX_SDMMC1_SD_Init();
    return MSD_OK;
}

/**
 * @brief  DeInitializes the SD card device.
 */
uint8_t bsp_DeInit_SD(void)
{
    hsd1.Instance = SDMMC1;
    if (HAL_SD_DeInit(&hsd1) != HAL_OK)
    {
        return MSD_ERROR;
    }
    return MSD_OK;
}

uint8_t bsp_SD_IsDetected(void)
{
    return SD_IS_INSERTED() ? SD_PRESENT : SD_NOT_PRESENT;
}

/**
 * @brief  Polling Block Read
 */
uint8_t bsp_SD_ReadBlocks(uint32_t *pData, uint32_t ReadAddr, uint32_t NumOfBlocks, uint32_t Timeout)
{
    if (HAL_SD_ReadBlocks(&hsd1, (uint8_t *)pData, ReadAddr, NumOfBlocks, Timeout) == HAL_OK)
    {
        return MSD_OK;
    }
    return MSD_ERROR;
}

/**
 * @brief  Polling Block Write
 */
uint8_t bsp_SD_WriteBlocks(uint32_t *pData, uint32_t WriteAddr, uint32_t NumOfBlocks, uint32_t Timeout)
{
    if (HAL_SD_WriteBlocks(&hsd1, (uint8_t *)pData, WriteAddr, NumOfBlocks, Timeout) == HAL_OK)
    {
        return MSD_OK;
    }
    return MSD_ERROR;
}

uint8_t bsp_SD_ReadBlocks_DMA(uint32_t *pData, uint32_t ReadAddr, uint32_t NumOfBlocks)
{
    if (HAL_SD_ReadBlocks_DMA(&hsd1, (uint8_t *)pData, ReadAddr, NumOfBlocks) == HAL_OK)
    {
        return MSD_OK;
    }
    return MSD_ERROR;
}

uint8_t bsp_SD_WriteBlocks_DMA(uint32_t *pData, uint32_t WriteAddr, uint32_t NumOfBlocks)
{
    if (HAL_SD_WriteBlocks_DMA(&hsd1, (uint8_t *)pData, WriteAddr, NumOfBlocks) == HAL_OK)
    {
        return MSD_OK;
    }
    return MSD_ERROR;
}

uint8_t bsp_SD_Erase(uint32_t StartAddr, uint32_t EndAddr)
{
    if (HAL_SD_Erase(&hsd1, StartAddr, EndAddr) == HAL_OK)
    {
        return MSD_OK;
    }
    return MSD_ERROR;
}

uint8_t bsp_SD_GetCardState(void)
{
    return ((HAL_SD_GetCardState(&hsd1) == HAL_SD_CARD_TRANSFER) ? SD_TRANSFER_OK : SD_TRANSFER_BUSY);
}

void bsp_SD_GetCardInfo(HAL_SD_CardInfoTypeDef *CardInfo)
{
    HAL_SD_GetCardInfo(&hsd1, CardInfo);
}

void SDMMC1_IRQHandler(void)
{
    HAL_SD_IRQHandler(&hsd1);
}

/**
 * @brief  Reads data from SD card with byte alignment support.
 */
uint8_t bsp_SD_Read(uint8_t *pBuffer, uint32_t ReadAddr, uint32_t Size)
{
    uint32_t block_addr, offset, count;

    while (Size > 0)
    {
        if (SD_WaitReady(SD_DATATIMEOUT) != MSD_OK)
            return MSD_ERROR;

        block_addr = ReadAddr / 512;
        offset = ReadAddr % 512;
        count = (512 - offset > Size) ? Size : (512 - offset);

        if (offset == 0 && count == 512)
        {
            if (bsp_SD_ReadBlocks((uint32_t *)pBuffer, block_addr, 1, SD_DATATIMEOUT) != MSD_OK)
                return MSD_ERROR;
        }
        else
        {
            if (bsp_SD_ReadBlocks((uint32_t *)g_sd_buf, block_addr, 1, SD_DATATIMEOUT) != MSD_OK)
                return MSD_ERROR;
            memcpy(pBuffer, g_sd_buf + offset, count);
        }

        pBuffer += count;
        ReadAddr += count;
        Size -= count;
    }
    return MSD_OK;
}

/**
 * @brief  Writes data to SD card with byte alignment support.
 */
uint8_t bsp_SD_Write(const uint8_t *pBuffer, uint32_t WriteAddr, uint32_t Size)
{
    uint32_t block_addr, offset, count;

    while (Size > 0)
    {
        if (SD_WaitReady(SD_DATATIMEOUT) != MSD_OK)
            return MSD_ERROR;

        block_addr = WriteAddr / 512;
        offset = WriteAddr % 512;
        count = (512 - offset > Size) ? Size : (512 - offset);

        if (offset == 0 && count == 512)
        {
            if (bsp_SD_WriteBlocks((uint32_t *)pBuffer, block_addr, 1, SD_DATATIMEOUT) != MSD_OK)
            {
                HAL_SD_Abort(&hsd1);
                return MSD_ERROR;
            }
        }
        else
        {
            /* Read-Modify-Write */
            if (bsp_SD_ReadBlocks((uint32_t *)g_sd_buf, block_addr, 1, SD_DATATIMEOUT) != MSD_OK)
                return MSD_ERROR;

            memcpy(g_sd_buf + offset, pBuffer, count);

            if (bsp_SD_WriteBlocks((uint32_t *)g_sd_buf, block_addr, 1, SD_DATATIMEOUT) != MSD_OK)
            {
                HAL_SD_Abort(&hsd1);
                return MSD_ERROR;
            }
        }

        pBuffer += count;
        WriteAddr += count;
        Size -= count;
    }
    return MSD_OK;
}

#if defined(__SHELL_H__) && defined(DEBUG_MODE)
#include "dump_hex.h"

static void sd_cmd_help(void)
{
    printf("Usage: sd <command> [args]\r\n");
    printf("Commands:\r\n");
    printf("  detect                    - Detect SD card presence\r\n");
    printf("  init                      - Initialize SD card\r\n");
    printf("  info                      - Get SD card information\r\n");
    printf("  read <addr> <size>        - Read data from SD (byte-aligned, max 2KB)\r\n");
    printf("  write <addr> <string>     - Write string to SD (byte-aligned)\r\n");
    printf("  erase <blk_start> <blk_end>- Erase blocks\r\n");
    printf("  bench <total_size> [step] - Benchmark read/write speed\r\n");
}

static int _sd_cmd(int argc, char *argv[])
{
    if (argc < 2)
    {
        sd_cmd_help();
        return -1;
    }

    const char *subcmd = argv[1];

    if (strcmp(subcmd, "detect") == 0)
    {
        printf("SD card %s detected.\r\n", (bsp_SD_IsDetected() == SD_PRESENT) ? "" : "NOT");
    }
    else if (strcmp(subcmd, "init") == 0)
    {
        uint8_t res = bsp_Init_SD();
        printf("SD card initialization %s (code: %u).\r\n", (res == MSD_OK) ? "success" : "failed", res);
    }
    else if (strcmp(subcmd, "info") == 0)
    {
        printf("\r\n");
        printf("SD Card BlockNbr    : %lX\r\n", (unsigned long)hsd1.SdCard.BlockNbr);
        printf("SD Card BlockSize   : %lX\r\n", (unsigned long)hsd1.SdCard.BlockSize);
        printf("SD Card CardSpeed   : %lX\r\n", (unsigned long)hsd1.SdCard.CardSpeed);
        printf("SD Card CardType    : %lX\r\n", (unsigned long)hsd1.SdCard.CardType);
        printf("SD Card CardVersion : %lX\r\n", (unsigned long)hsd1.SdCard.CardVersion);
        printf("SD Card Class       : %lX\r\n", (unsigned long)hsd1.SdCard.Class);
        printf("SD Card LogBlockNbr : %lX\r\n", (unsigned long)hsd1.SdCard.LogBlockNbr);
        printf("SD Card LogBlockSize: %lX\r\n", (unsigned long)hsd1.SdCard.LogBlockSize);
        printf("SD Card RelCardAdd  : %lX\r\n", (unsigned long)hsd1.SdCard.RelCardAdd);

        HAL_SD_CardInfoTypeDef cardInfo;
        HAL_StatusTypeDef res = HAL_SD_GetCardInfo(&hsd1, &cardInfo);
        if (res != HAL_OK)
        {
            printf("HAL_SD_GetCardInfo() error\r\n");
            return -1;
        }
        printf("\r\n*** HAL_SD_GetCardInfo() info ***\r\n");
        printf("Card Type= %lu\r\n", (unsigned long)cardInfo.CardType);
        printf("Card Version= %lu\r\n", (unsigned long)cardInfo.CardVersion);
        printf("Card Class= %lu\r\n", (unsigned long)cardInfo.Class);
        printf("Relative Card Address= %lu\r\n", (unsigned long)cardInfo.RelCardAdd);
        printf("Block Count= %lu\r\n", (unsigned long)cardInfo.BlockNbr);
        printf("Block Size(Bytes)= %lu\r\n", (unsigned long)cardInfo.BlockSize);
        printf("LogiBlockCount= %lu\r\n", (unsigned long)cardInfo.LogBlockNbr);
        printf("LogiBlockSize(Bytes)= %lu\r\n", (unsigned long)cardInfo.LogBlockSize);
        printf("SD Card Capacity(MB)= %lu\r\n", (unsigned long)(cardInfo.BlockNbr >> 1U >> 10U));

        HAL_SD_CardStatusTypeDef cardStatus;
        res = HAL_SD_GetCardStatus(&hsd1, &cardStatus);
        if (res != HAL_OK)
        {
            printf("HAL_SD_GetCardStatus() error\r\n");
            return -1;
        }
        printf("\r\n*** HAL_SD_GetCardStatus() info ***\r\n");
        printf("DataBusWidth= %u\r\n", (unsigned)cardStatus.DataBusWidth);
        printf("CardType= %u\r\n", (unsigned)cardStatus.CardType);
        printf("SpeedClass= %u\r\n", (unsigned)cardStatus.SpeedClass);
        printf("AllocationUnitSize= %u\r\n", (unsigned)cardStatus.AllocationUnitSize);
        printf("EraseSize= %u\r\n", (unsigned)cardStatus.EraseSize);
        printf("EraseTimeout= %u\r\n", (unsigned)cardStatus.EraseTimeout);
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

        if (size == 0 || size > 2048)
        {
            printf("Error: Size must be 1-2048 bytes for dump.\r\n");
            return -1;
        }

        uint8_t *buf = malloc(size);
        if (!buf)
            return -1;

        if (bsp_SD_Read(buf, addr, size) == MSD_OK)
        {
            printf("Read %u bytes from 0x%08X successfully.\r\n", size, addr);
            dump_hex(buf, size, 16);
        }
        else
            printf("Error: Read failed.\r\n");
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

        if (bsp_SD_Write((const uint8_t *)data, addr, size) == MSD_OK)
        {
            printf("Wrote %u bytes to 0x%08X successfully.\r\n", size, addr);
        }
        else
            printf("Error: Write failed.\r\n");
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

        if (bsp_SD_Erase(start, end) == MSD_OK)
        {
            printf("Erase blocks from %u to %u success.\r\n", start, end);
        }
        else
            printf("Error: Erase failed.\r\n");
    }
    else if (strcmp(subcmd, "bench") == 0)
    {
        if (argc < 3)
        {
            printf("Usage: sd bench <total_size> [step]\r\n");
            return -1;
        }
        uint32_t total_size = strtoul(argv[2], NULL, 0);
        uint32_t step = (argc >= 4) ? strtoul(argv[3], NULL, 0) : 4096;

        uint8_t *buf = malloc(step);
        if (!buf)
            return -1;

        printf("Benchmarking SD (Total: %u, Step: %u)...\r\n", total_size, step);

        // Write test
        memset(buf, 0x55, step);
        uint32_t start_time = HAL_GetTick();
        for (uint32_t addr = 0; addr < total_size; addr += step)
        {
            uint32_t cur_size = (addr + step > total_size) ? (total_size - addr) : step;
            if (bsp_SD_Write(buf, addr, cur_size) != MSD_OK)
            {
                printf("Error: Write failed at 0x%08X\r\n", addr);
                free(buf);
                return -1;
            }
        }
        uint32_t cost = HAL_GetTick() - start_time;
        printf("Write: %u bytes in %u ms, Speed: %.2f KB/s\r\n", total_size, cost,
               (cost > 0) ? (float)total_size / cost : 0);

        // Read test
        start_time = HAL_GetTick();
        for (uint32_t addr = 0; addr < total_size; addr += step)
        {
            uint32_t cur_size = (addr + step > total_size) ? (total_size - addr) : step;
            if (bsp_SD_Read(buf, addr, cur_size) != MSD_OK)
            {
                printf("Error: Read failed at 0x%08X\r\n", addr);
                free(buf);
                return -1;
            }
        }
        cost = HAL_GetTick() - start_time;
        printf("Read : %u bytes in %u ms, Speed: %.2f KB/s\r\n", total_size, cost,
               (cost > 0) ? (float)total_size / cost : 0);

        free(buf);
    }
    else
        sd_cmd_help();

    return 0;
}

SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), sd, _sd_cmd, sd card debug tool);
#endif
