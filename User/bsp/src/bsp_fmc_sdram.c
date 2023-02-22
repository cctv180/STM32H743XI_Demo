/*
*********************************************************************************************************
*
*    模块名称 : 外部SDRAM驱动模块
*    文件名称 : bsp_fmc_sdram.c
*    版    本 : V2.4
*    说    明 : 安富莱STM32-V7开发板标配的SDRAM型号IS42S32800G-6BLI, 32位带宽, 容量32MB, 6ns速度(166MHz)
*
*    修改记录 :
*        版本号  日期        作者     说明
*        V1.0    2018-05-04 armfly  正式发布
*
*    Copyright (C), 2018-2030, 安富莱电子 www.armfly.com
*
*********************************************************************************************************
*/
#include "bsp.h"
#include "bsp_fmc_sdram.h"

/*
    SDRAM refresh period / Number of rows X SDRAM时钟速度 – 20
  = 64ms / 4096 *100MHz - 20
  = 1542.5 取值1543
*/
#define SDRAM_REFRESH_COUNT ((uint32_t)1543) /* SDRAM自刷新计数 */
#define SDRAM_TIMEOUT ((uint32_t)0xFFFF)

/* SDRAM的参数配置 */
#define SDRAM_MODEREG_BURST_LENGTH_1 ((uint16_t)0x0000)
#define SDRAM_MODEREG_BURST_LENGTH_2 ((uint16_t)0x0001)
#define SDRAM_MODEREG_BURST_LENGTH_4 ((uint16_t)0x0002)
#define SDRAM_MODEREG_BURST_LENGTH_8 ((uint16_t)0x0004)
#define SDRAM_MODEREG_BURST_TYPE_SEQUENTIAL ((uint16_t)0x0000)
#define SDRAM_MODEREG_BURST_TYPE_INTERLEAVED ((uint16_t)0x0008)
#define SDRAM_MODEREG_CAS_LATENCY_2 ((uint16_t)0x0020)
#define SDRAM_MODEREG_CAS_LATENCY_3 ((uint16_t)0x0030)
#define SDRAM_MODEREG_OPERATING_MODE_STANDARD ((uint16_t)0x0000)
#define SDRAM_MODEREG_WRITEBURST_MODE_PROGRAMMED ((uint16_t)0x0000)
#define SDRAM_MODEREG_WRITEBURST_MODE_SINGLE ((uint16_t)0x0200)

static uint8_t FMC_Initialized = 0;   // HAL_FMC_MspInit
static uint8_t FMC_DeInitialized = 0; // HAL_FMC_MspDeInit

static void SDRAM_Initialization_Sequence(SDRAM_HandleTypeDef *hsdram, FMC_SDRAM_CommandTypeDef *Command);

/* ### STM32CubeMX CODE BEGIN */
static void HAL_FMC_MspInit(void)
{
    /* USER CODE BEGIN FMC_MspInit 0 */

    /* USER CODE END FMC_MspInit 0 */
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    if (FMC_Initialized)
    {
        return;
    }
    FMC_Initialized = 1;
    RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};

    /** Initializes the peripherals clock
     */
    PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_FMC;
    PeriphClkInitStruct.FmcClockSelection = RCC_FMCCLKSOURCE_D1HCLK;
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
    {
        ERROR_HANDLER();
    }

    /* Peripheral clock enable */
    __HAL_RCC_FMC_CLK_ENABLE();

    /** FMC GPIO Configuration
    PI6   ------> FMC_D28
    PI5   ------> FMC_NBL3
    PI4   ------> FMC_NBL2
    PI1   ------> FMC_D25
    PI0   ------> FMC_D24
    PI7   ------> FMC_D29
    PE1   ------> FMC_NBL1
    PI2   ------> FMC_D26
    PH15   ------> FMC_D23
    PH14   ------> FMC_D22
    PE0   ------> FMC_NBL0
    PI3   ------> FMC_D27
    PG15   ------> FMC_SDNCAS
    PD0   ------> FMC_D2
    PH13   ------> FMC_D21
    PI9   ------> FMC_D30
    PD1   ------> FMC_D3
    PI10   ------> FMC_D31
    PG8   ------> FMC_SDCLK
    PF2   ------> FMC_A2
    PF1   ------> FMC_A1
    PF0   ------> FMC_A0
    PG5   ------> FMC_BA1
    PF3   ------> FMC_A3
    PG4   ------> FMC_BA0
    PF5   ------> FMC_A5
    PF4   ------> FMC_A4
    PH2   ------> FMC_SDCKE0
    PE10   ------> FMC_D7
    PH3   ------> FMC_SDNE0
    PH5   ------> FMC_SDNWE
    PF13   ------> FMC_A7
    PF14   ------> FMC_A8
    PE9   ------> FMC_D6
    PE11   ------> FMC_D8
    PH10   ------> FMC_D18
    PH11   ------> FMC_D19
    PD15   ------> FMC_D1
    PD14   ------> FMC_D0
    PF12   ------> FMC_A6
    PF15   ------> FMC_A9
    PE12   ------> FMC_D9
    PE15   ------> FMC_D12
    PH9   ------> FMC_D17
    PH12   ------> FMC_D20
    PF11   ------> FMC_SDNRAS
    PG0   ------> FMC_A10
    PE8   ------> FMC_D5
    PE13   ------> FMC_D10
    PH8   ------> FMC_D16
    PD10   ------> FMC_D15
    PD9   ------> FMC_D14
    PG1   ------> FMC_A11
    PE7   ------> FMC_D4
    PE14   ------> FMC_D11
    PD8   ------> FMC_D13
    */
    GPIO_InitStruct.Pin = GPIO_PIN_6 | GPIO_PIN_5 | GPIO_PIN_4 | GPIO_PIN_1 | GPIO_PIN_0 | GPIO_PIN_7 | GPIO_PIN_2 | GPIO_PIN_3 | GPIO_PIN_9 | GPIO_PIN_10;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF12_FMC;
    HAL_GPIO_Init(GPIOI, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_1 | GPIO_PIN_0 | GPIO_PIN_10 | GPIO_PIN_9 | GPIO_PIN_11 | GPIO_PIN_12 | GPIO_PIN_15 | GPIO_PIN_8 | GPIO_PIN_13 | GPIO_PIN_7 | GPIO_PIN_14;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF12_FMC;
    HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_15 | GPIO_PIN_14 | GPIO_PIN_13 | GPIO_PIN_2 | GPIO_PIN_3 | GPIO_PIN_5 | GPIO_PIN_10 | GPIO_PIN_11 | GPIO_PIN_9 | GPIO_PIN_12 | GPIO_PIN_8;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF12_FMC;
    HAL_GPIO_Init(GPIOH, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_15 | GPIO_PIN_8 | GPIO_PIN_5 | GPIO_PIN_4 | GPIO_PIN_0 | GPIO_PIN_1;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF12_FMC;
    HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_15 | GPIO_PIN_14 | GPIO_PIN_10 | GPIO_PIN_9 | GPIO_PIN_8;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF12_FMC;
    HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_2 | GPIO_PIN_1 | GPIO_PIN_0 | GPIO_PIN_3 | GPIO_PIN_5 | GPIO_PIN_4 | GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_12 | GPIO_PIN_15 | GPIO_PIN_11;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF12_FMC;
    HAL_GPIO_Init(GPIOF, &GPIO_InitStruct);

    /* USER CODE BEGIN FMC_MspInit 1 */

    /* USER CODE END FMC_MspInit 1 */
}

void HAL_SDRAM_MspInit(SDRAM_HandleTypeDef *hsdram)
{
    /* USER CODE BEGIN SDRAM_MspInit 0 */

    /* USER CODE END SDRAM_MspInit 0 */
    HAL_FMC_MspInit();
    /* USER CODE BEGIN SDRAM_MspInit 1 */

    /* USER CODE END SDRAM_MspInit 1 */
}

static void HAL_FMC_MspDeInit(void)
{
    /* USER CODE BEGIN FMC_MspDeInit 0 */

    /* USER CODE END FMC_MspDeInit 0 */
    if (FMC_DeInitialized)
    {
        return;
    }
    FMC_DeInitialized = 1;
    /* Peripheral clock enable */
    __HAL_RCC_FMC_CLK_DISABLE();

    /** FMC GPIO Configuration
    PI6   ------> FMC_D28
    PI5   ------> FMC_NBL3
    PI4   ------> FMC_NBL2
    PI1   ------> FMC_D25
    PI0   ------> FMC_D24
    PI7   ------> FMC_D29
    PE1   ------> FMC_NBL1
    PI2   ------> FMC_D26
    PH15   ------> FMC_D23
    PH14   ------> FMC_D22
    PE0   ------> FMC_NBL0
    PI3   ------> FMC_D27
    PG15   ------> FMC_SDNCAS
    PD0   ------> FMC_D2
    PH13   ------> FMC_D21
    PI9   ------> FMC_D30
    PD1   ------> FMC_D3
    PI10   ------> FMC_D31
    PG8   ------> FMC_SDCLK
    PF2   ------> FMC_A2
    PF1   ------> FMC_A1
    PF0   ------> FMC_A0
    PG5   ------> FMC_BA1
    PF3   ------> FMC_A3
    PG4   ------> FMC_BA0
    PF5   ------> FMC_A5
    PF4   ------> FMC_A4
    PH2   ------> FMC_SDCKE0
    PE10   ------> FMC_D7
    PH3   ------> FMC_SDNE0
    PH5   ------> FMC_SDNWE
    PF13   ------> FMC_A7
    PF14   ------> FMC_A8
    PE9   ------> FMC_D6
    PE11   ------> FMC_D8
    PH10   ------> FMC_D18
    PH11   ------> FMC_D19
    PD15   ------> FMC_D1
    PD14   ------> FMC_D0
    PF12   ------> FMC_A6
    PF15   ------> FMC_A9
    PE12   ------> FMC_D9
    PE15   ------> FMC_D12
    PH9   ------> FMC_D17
    PH12   ------> FMC_D20
    PF11   ------> FMC_SDNRAS
    PG0   ------> FMC_A10
    PE8   ------> FMC_D5
    PE13   ------> FMC_D10
    PH8   ------> FMC_D16
    PD10   ------> FMC_D15
    PD9   ------> FMC_D14
    PG1   ------> FMC_A11
    PE7   ------> FMC_D4
    PE14   ------> FMC_D11
    PD8   ------> FMC_D13
    */
    HAL_GPIO_DeInit(GPIOI, GPIO_PIN_6 | GPIO_PIN_5 | GPIO_PIN_4 | GPIO_PIN_1 | GPIO_PIN_0 | GPIO_PIN_7 | GPIO_PIN_2 | GPIO_PIN_3 | GPIO_PIN_9 | GPIO_PIN_10);

    HAL_GPIO_DeInit(GPIOE, GPIO_PIN_1 | GPIO_PIN_0 | GPIO_PIN_10 | GPIO_PIN_9 | GPIO_PIN_11 | GPIO_PIN_12 | GPIO_PIN_15 | GPIO_PIN_8 | GPIO_PIN_13 | GPIO_PIN_7 | GPIO_PIN_14);

    HAL_GPIO_DeInit(GPIOH, GPIO_PIN_15 | GPIO_PIN_14 | GPIO_PIN_13 | GPIO_PIN_2 | GPIO_PIN_3 | GPIO_PIN_5 | GPIO_PIN_10 | GPIO_PIN_11 | GPIO_PIN_9 | GPIO_PIN_12 | GPIO_PIN_8);

    HAL_GPIO_DeInit(GPIOG, GPIO_PIN_15 | GPIO_PIN_8 | GPIO_PIN_5 | GPIO_PIN_4 | GPIO_PIN_0 | GPIO_PIN_1);

    HAL_GPIO_DeInit(GPIOD, GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_15 | GPIO_PIN_14 | GPIO_PIN_10 | GPIO_PIN_9 | GPIO_PIN_8);

    HAL_GPIO_DeInit(GPIOF, GPIO_PIN_2 | GPIO_PIN_1 | GPIO_PIN_0 | GPIO_PIN_3 | GPIO_PIN_5 | GPIO_PIN_4 | GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_12 | GPIO_PIN_15 | GPIO_PIN_11);

    /* USER CODE BEGIN FMC_MspDeInit 1 */

    /* USER CODE END FMC_MspDeInit 1 */
}

void HAL_SDRAM_MspDeInit(SDRAM_HandleTypeDef *hsdram)
{
    /* USER CODE BEGIN SDRAM_MspDeInit 0 */

    /* USER CODE END SDRAM_MspDeInit 0 */
    HAL_FMC_MspDeInit();
    /* USER CODE BEGIN SDRAM_MspDeInit 1 */

    /* USER CODE END SDRAM_MspDeInit 1 */
}
/* ### STM32CubeMX CODE END */

/*
*********************************************************************************************************
*    函 数 名: bsp_InitExtSDRAM
*    功能说明: 配置连接外部SDRAM的GPIO和FMC
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void bsp_InitExtSDRAM(void)
{
    SDRAM_HandleTypeDef hsdram = {0};
    FMC_SDRAM_TimingTypeDef SdramTiming = {0};
    FMC_SDRAM_CommandTypeDef command = {0};

    /* 使能GPIO时钟 */
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();
    __HAL_RCC_GPIOF_CLK_ENABLE();
    __HAL_RCC_GPIOG_CLK_ENABLE();
    __HAL_RCC_GPIOH_CLK_ENABLE();
    __HAL_RCC_GPIOI_CLK_ENABLE();

    /* SDRAM配置 */
    hsdram.Instance = FMC_SDRAM_DEVICE;

    /*
       FMC使用的HCLK3时钟，200MHz，用于SDRAM的话，至少2分频，也就是100MHz，即1个SDRAM时钟周期是10ns
       下面参数单位均为10ns。
    */
    SdramTiming.LoadToActiveDelay = 2;    /* 20ns, TMRD定义加载模式寄存器的命令与激活命令或刷新命令之间的延迟 */
    SdramTiming.ExitSelfRefreshDelay = 7; /* 70ns, TXSR定义从发出自刷新命令到发出激活命令之间的延迟 */
    SdramTiming.SelfRefreshTime = 4;      /* 50ns, TRAS定义最短的自刷新周期 */
    SdramTiming.RowCycleDelay = 7;        /* 70ns, TRC定义刷新命令和激活命令之间的延迟 */
    SdramTiming.WriteRecoveryTime = 3;    /* 20ns, TWR定义在写命令和预充电命令之间的延迟 */
    SdramTiming.RPDelay = 2;              /* 20ns, TRP定义预充电命令与其它命令之间的延迟 */
    SdramTiming.RCDDelay = 2;             /* 20ns, TRCD定义激活命令与读/写命令之间的延迟 */

    hsdram.Init.SDBank = FMC_SDRAM_BANK1;                             /* 硬件设计上用的BANK1 */
    hsdram.Init.ColumnBitsNumber = FMC_SDRAM_COLUMN_BITS_NUM_9;       /* 9列 */
    hsdram.Init.RowBitsNumber = FMC_SDRAM_ROW_BITS_NUM_12;            /* 12行 */
    hsdram.Init.MemoryDataWidth = FMC_SDRAM_MEM_BUS_WIDTH_32;         /* 32位带宽 */
    hsdram.Init.InternalBankNumber = FMC_SDRAM_INTERN_BANKS_NUM_4;    /* SDRAM有4个BANK */
    hsdram.Init.CASLatency = FMC_SDRAM_CAS_LATENCY_3;                 /* CAS Latency可以设置Latency1，2和3，实际测试Latency3稳定 */
    hsdram.Init.WriteProtection = FMC_SDRAM_WRITE_PROTECTION_DISABLE; /* 禁止写保护 */
    hsdram.Init.SDClockPeriod = FMC_SDRAM_CLOCK_PERIOD_2;             /* FMC时钟200MHz，2分频后给SDRAM，即100MHz */
    hsdram.Init.ReadBurst = FMC_SDRAM_RBURST_ENABLE;                  /* 使能读突发 */
    hsdram.Init.ReadPipeDelay = FMC_SDRAM_RPIPE_DELAY_0;              /* 此位定CAS延时后延后多少个SDRAM时钟周期读取数据，实际测此位可以设置无需延迟 */

    /* 配置SDRAM控制器基本参数 */
    if (HAL_SDRAM_Init(&hsdram, &SdramTiming) != HAL_OK)
    {
        /* Initialization Error */
        ERROR_HANDLER();
    }

    /* 完成SDRAM序列初始化 */
    SDRAM_Initialization_Sequence(&hsdram, &command);
}

/*
*********************************************************************************************************
*    函 数 名: SDRAM初始化序列
*    功能说明: 完成SDRAM序列初始化
*    形    参: hsdram: SDRAM句柄
*              Command: 命令结构体指针
*    返 回 值: None
*********************************************************************************************************
*/
static void SDRAM_Initialization_Sequence(SDRAM_HandleTypeDef *hsdram, FMC_SDRAM_CommandTypeDef *Command)
{
    __IO uint32_t tmpmrd = 0;

    /*##-1- 时钟使能命令 ##################################################*/
    Command->CommandMode = FMC_SDRAM_CMD_CLK_ENABLE;
    Command->CommandTarget = FMC_SDRAM_CMD_TARGET_BANK1;
    ;
    Command->AutoRefreshNumber = 1;
    Command->ModeRegisterDefinition = 0;

    /* 发送命令 */
    HAL_SDRAM_SendCommand(hsdram, Command, SDRAM_TIMEOUT);

    /*##-2- 插入延迟，至少100us ##################################################*/
    HAL_Delay(1);

    /*##-3- 整个SDRAM预充电命令，PALL(precharge all) #############################*/
    Command->CommandMode = FMC_SDRAM_CMD_PALL;
    Command->CommandTarget = FMC_SDRAM_CMD_TARGET_BANK1;
    Command->AutoRefreshNumber = 1;
    Command->ModeRegisterDefinition = 0;

    /* 发送命令 */
    HAL_SDRAM_SendCommand(hsdram, Command, SDRAM_TIMEOUT);

    /*##-4- 自动刷新命令 #######################################################*/
    Command->CommandMode = FMC_SDRAM_CMD_AUTOREFRESH_MODE;
    Command->CommandTarget = FMC_SDRAM_CMD_TARGET_BANK1;
    Command->AutoRefreshNumber = 8;
    Command->ModeRegisterDefinition = 0;

    /* 发送命令 */
    HAL_SDRAM_SendCommand(hsdram, Command, SDRAM_TIMEOUT);

    /*##-5- 配置SDRAM模式寄存器 ###############################################*/
    tmpmrd = (uint32_t)SDRAM_MODEREG_BURST_LENGTH_1 |
             SDRAM_MODEREG_BURST_TYPE_SEQUENTIAL |
             SDRAM_MODEREG_CAS_LATENCY_3 |
             SDRAM_MODEREG_OPERATING_MODE_STANDARD |
             SDRAM_MODEREG_WRITEBURST_MODE_SINGLE;

    Command->CommandMode = FMC_SDRAM_CMD_LOAD_MODE;
    Command->CommandTarget = FMC_SDRAM_CMD_TARGET_BANK1;
    Command->AutoRefreshNumber = 1;
    Command->ModeRegisterDefinition = tmpmrd;

    /* 发送命令 */
    HAL_SDRAM_SendCommand(hsdram, Command, SDRAM_TIMEOUT);

    /*##-6- 设置自刷新率 ####################################################*/
    /*
        SDRAM refresh period / Number of rows）*SDRAM时钟速度 – 20
      = 64ms / 4096 *100MHz - 20
      = 1542.5 取值1543
    */
    HAL_SDRAM_ProgramRefreshRate(hsdram, SDRAM_REFRESH_COUNT);
}

/*
*********************************************************************************************************
*    函 数 名: bsp_TestExtSDRAM
*    功能说明: 扫描测试外部SDRAM的全部单元。
*    形    参: 无
*    返 回 值: 0 表示测试通过； 大于0表示错误单元的个数。
*********************************************************************************************************
*/
uint32_t bsp_TestExtSDRAM1(void)
{
    uint32_t i;
    uint32_t *pSRAM;
    uint8_t *pBytes;
    uint32_t err;
    const uint8_t ByteBuf[4] = {0x55, 0xA5, 0x5A, 0xAA};

    /* 写SRAM */
    pSRAM = (uint32_t *)EXT_SDRAM_ADDR;
    for (i = 0; i < EXT_SDRAM_SIZE / 4; i++)
    {
        *pSRAM++ = i;
    }

    /* 读SRAM */
    err = 0;
    pSRAM = (uint32_t *)EXT_SDRAM_ADDR;
    for (i = 0; i < EXT_SDRAM_SIZE / 4; i++)
    {
        if (*pSRAM++ != i)
        {
            err++;
        }
    }

    if (err > 0)
    {
        return (4 * err);
    }

    /* 对SRAM 的数据求反并写入 */
    pSRAM = (uint32_t *)EXT_SDRAM_ADDR;
    for (i = 0; i < EXT_SDRAM_SIZE / 4; i++)
    {
        *pSRAM = ~*pSRAM;
        pSRAM++;
    }

    /* 再次比较SDRAM的数据 */
    err = 0;
    pSRAM = (uint32_t *)EXT_SDRAM_ADDR;
    for (i = 0; i < EXT_SDRAM_SIZE / 4; i++)
    {
        if (*pSRAM++ != (~i))
        {
            err++;
        }
    }

    if (err > 0)
    {
        return (4 * err);
    }

    /* 测试按字节方式访问, 目的是验证 FSMC_NBL0 、 FSMC_NBL1 口线 */
    pBytes = (uint8_t *)EXT_SDRAM_ADDR;
    for (i = 0; i < sizeof(ByteBuf); i++)
    {
        *pBytes++ = ByteBuf[i];
    }

    /* 比较SDRAM的数据 */
    err = 0;
    pBytes = (uint8_t *)EXT_SDRAM_ADDR;
    for (i = 0; i < sizeof(ByteBuf); i++)
    {
        if (*pBytes++ != ByteBuf[i])
        {
            err++;
        }
    }
    if (err > 0)
    {
        return err;
    }
    return 0;
}

/*
*********************************************************************************************************
*    函 数 名: bsp_TestExtSDRAM2
*    功能说明: 扫描测试外部SDRAM，不扫描前面4M字节的显存。
*    形    参: 无
*    返 回 值: 0 表示测试通过； 大于0表示错误单元的个数。
*********************************************************************************************************
*/
uint32_t bsp_TestExtSDRAM2(void)
{
    uint32_t i;
    uint32_t *pSRAM;
    uint8_t *pBytes;
    uint32_t err;
    const uint8_t ByteBuf[4] = {0x55, 0xA5, 0x5A, 0xAA};

    /* 写SRAM */
    pSRAM = (uint32_t *)SDRAM_APP_BUF;
    for (i = 0; i < SDRAM_APP_SIZE / 4; i++)
    {
        *pSRAM++ = i;
    }

    /* 读SRAM */
    err = 0;
    pSRAM = (uint32_t *)SDRAM_APP_BUF;
    for (i = 0; i < SDRAM_APP_SIZE / 4; i++)
    {
        if (*pSRAM++ != i)
        {
            err++;
        }
    }

    if (err > 0)
    {
        return (4 * err);
    }

#if 0
    /* 对SRAM 的数据求反并写入 */
    pSRAM = (uint32_t *)SDRAM_APP_BUF;
    for (i = 0; i < SDRAM_APP_SIZE / 4; i++)
    {
        *pSRAM = ~*pSRAM;
        pSRAM++;
    }

    /* 再次比较SDRAM的数据 */
    err = 0;
    pSRAM = (uint32_t *)SDRAM_APP_BUF;
    for (i = 0; i < SDRAM_APP_SIZE / 4; i++)
    {
        if (*pSRAM++ != (~i))
        {
            err++;
        }
    }

    if (err >  0)
    {
        return (4 * err);
    }
#endif

    /* 测试按字节方式访问, 目的是验证 FSMC_NBL0 、 FSMC_NBL1 口线 */
    pBytes = (uint8_t *)SDRAM_APP_BUF;
    for (i = 0; i < sizeof(ByteBuf); i++)
    {
        *pBytes++ = ByteBuf[i];
    }

    /* 比较SDRAM的数据 */
    err = 0;
    pBytes = (uint8_t *)SDRAM_APP_BUF;
    for (i = 0; i < sizeof(ByteBuf); i++)
    {
        if (*pBytes++ != ByteBuf[i])
        {
            err++;
        }
    }
    if (err > 0)
    {
        return err;
    }
    return 0;
}

#if defined(__SHELL_H__) && defined(DEBUG_MODE)
static int _cmd(int argc, char *argv[])
{
#define __is_print(ch) ((unsigned int)((ch) - ' ') < 127u - ' ')
#define HEXDUMP_WIDTH 16

    const char *help_info[] = {
        "set init/deinit",
        "test 1/2",
        "read bit32/buff",
        "write bit32/buff"};

    // printf("\r\nargc = %d\r\n\r\n", argc);

    if (!strcmp(argv[1], "set"))
    {
        if (!strcmp(argv[2], "init"))
        {
            FMC_DeInitialized = 0;
            bsp_InitExtSDRAM();

            return 0;
        }
        else if (!strcmp(argv[2], "deinit"))
        {
            SDRAM_HandleTypeDef hsdram = {0};

            FMC_Initialized = 0;
            /* SDRAM配置 */
            hsdram.Instance = FMC_SDRAM_DEVICE;
            HAL_SDRAM_DeInit(&hsdram);

            return 0;
        }
        else
        {
            printf("write parameter Error.\r\n");
            printf("%s ", argv[0]);
            printf("%s\r\n", help_info[0]);
            return -1;
        }
    }
    else if (!strcmp(argv[1], "test"))
    {
        if (!strcmp(argv[2], "1"))
        {
            return bsp_TestExtSDRAM1();
        }
        else if (!strcmp(argv[2], "2"))
        {
            return bsp_TestExtSDRAM2();
        }
        else
        {
            printf("write parameter Error.\r\n");
            printf("%s ", argv[0]);
            printf("%s\r\n", help_info[1]);
            return -1;
        }
    }
    else if (!strcmp(argv[1], "read"))
    {
        if (!strcmp(argv[2], "bit32"))
        {
            if (argc < 3 + 1)
            {
                printf("Error Command.\r\n%s %s %s Add(4).\r\n",
                       argv[0], argv[1], argv[2]);
                return -1;
            }
            uint32_t add = AsciiToUint32(argv[3]);
            add = (add & (EXT_SDRAM_SIZE - 1)) + EXT_SDRAM_ADDR;
            add &= ~0x3UL; // 4字节对齐访问
            printf("address = 0x%08x\r\n", add);

            printf("data = %d 0x%08x\r\n", *(uint32_t *)add, *(uint32_t *)add);

            return 0;
        }
        else if (!strcmp(argv[2], "buff"))
        {
            if (argc < 5)
            {
                printf("Error Command.\r\n%s %s %s add size.\r\n",
                       argv[0], argv[1], argv[2]);
                return -1;
            }
            uint32_t add = AsciiToUint32(argv[3]);
            uint16_t size = atoi(argv[4]);
            uint8_t *buff = malloc(size);
            add = (add & (EXT_SDRAM_SIZE - 1)) + EXT_SDRAM_ADDR;
            printf("address = 0x%08x\r\n", add);

            if (buff)
            {
                printf("Read buff success. add = %d 0x%08x size = %d.\r\nThe data is:\r\n", add, add, size);

                for (uint32_t i = 0; i < size; i++)
                {
                    buff[i] = *(uint8_t *)add;
                    add++;
                }

                printf("Offset (h) 00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F\r\n");
                for (uint16_t i = 0; i < size; i += HEXDUMP_WIDTH)
                {
                    printf("[%08X] ", i);
                    /* dump hex */
                    for (uint16_t j = 0; j < HEXDUMP_WIDTH; j++)
                    {
                        if (i + j < size)
                        {
                            printf("%02X ", buff[i + j]);
                        }
                        else
                        {
                            printf("   ");
                        }
                    }
                    /* dump char for hex */
                    for (uint16_t j = 0; j < HEXDUMP_WIDTH; j++)
                    {
                        if (i + j < size)
                        {
                            printf("%c", __is_print(buff[i + j]) ? buff[i + j] : '.');
                        }
                    }
                    printf("\r\n");
                }
                printf("\r\n");

                if (buff != NULL)
                {
                    free(buff);
                }

                return 0;
            }
            else
            {
                printf("Low memory! size = %d\r\n", size);
            }
        }
        else
        {
            printf("write parameter Error.\r\n");
            printf("%s ", argv[0]);
            printf("%s\r\n", help_info[2]);

            return -1;
        }
    }
    else if (!strcmp(argv[1], "write"))
    {

        if (!strcmp(argv[2], "bit32"))
        {
            if (argc < 4 + 1)
            {
                printf("Error Command\r\n%s %s %s add(4) data.\r\n",
                       argv[0], argv[1], argv[2]);

                return -1;
            }

            uint32_t add = AsciiToUint32(argv[3]);
            uint32_t data = AsciiToUint32(argv[4]);
            add = (add & (EXT_SDRAM_SIZE - 1)) + EXT_SDRAM_ADDR;
            add &= ~0x3UL; // 4字节对齐访问
            printf("address = 0x%08x\r\n", add);

            *(uint32_t *)add = data;
            printf("data = %d 0x%08x\r\n", *(uint32_t *)add, *(uint32_t *)add);

            return 0;
        }
        else if (!strcmp(argv[2], "buff"))
        {
            if (argc < 5)
            {
                printf("Error Command\r\n%s %s %s add str.\r\n",
                       argv[0], argv[1], argv[2]);

                return -1;
            }
            uint32_t add = AsciiToUint32(argv[3]);
            uint32_t len = str_len(argv[4]);
            add = (add & (EXT_SDRAM_SIZE - 1)) + EXT_SDRAM_ADDR;
            printf("address = 0x%08x\r\n", add);

            for (uint32_t i = 0; i < len; i++)
            {
                ((uint8_t *)add)[i] = argv[4][i];
            }

            return 0;
        }
        else
        {
            printf("write parameter Error.\r\n");
            printf("%s ", argv[0]);
            printf("%s\r\n", help_info[3]);

            return -1;
        }
    }
    else
    {
        printf("Error Command\r\nUsage:\r\n");
        for (uint32_t i = 0; i < sizeof(help_info) / sizeof(char *); i++)
        {
            printf("%s ", argv[0]);
            printf("%s\r\n", help_info[i]);
        }
        printf("\r\n");
    }

    return -1;
}
// 导出到命令列表里
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), sdram, _cmd, sdram[set test read write]);
#endif // #ifdef DEBUG_MODE

/***************************** 安富莱电子 www.armfly.com (END OF FILE) *********************************/
