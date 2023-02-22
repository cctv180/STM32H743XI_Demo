/*
*********************************************************************************************************
*
*    模块名称 : W25Q256 QSPI驱动模块
*    文件名称 : bsp_qspi_w25q256.c
*    版    本 : V1.0
*    说    明 : 使用CPU的QSPI总线驱动串行FLASH，提供基本的读写函数，采用4线方式，MDMA传输
*
*    修改记录 :
*        版本号  日期        作者     说明
*        V1.0    2020-11-01  armfly  正式发布
*
*    Copyright (C), 2020-2030, 安富莱电子 www.armfly.com
*
*********************************************************************************************************
*/
#include "bsp.h"
#include "bsp_qspi.h"

QSPI_HandleTypeDef hqspi;

static inline HAL_StatusTypeDef QSPI_SendCommand(uint32_t _instruction,
                                                 uint32_t _instructionMode,
                                                 uint32_t _address,
                                                 uint32_t _addressMode,
                                                 uint32_t _addressSize,
                                                 uint32_t _dummyCycles,
                                                 uint32_t _dataMode,
                                                 uint32_t _nData);
static void QSPI_WriteEnable(void);
static void QSPI_WriteEnableREG(void);
static void QSPI_WriteDisable(void);

/**
 * @brief QSPI MSP Initialization
 * This function configures the hardware resources used in this example
 * @param hqspi: QSPI handle pointer
 * @retval None
 */
void HAL_QSPI_MspInit(QSPI_HandleTypeDef *hqspi)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};
    if (hqspi->Instance == QUADSPI)
    {
        /* USER CODE BEGIN QUADSPI_MspInit 0 */

        /* USER CODE END QUADSPI_MspInit 0 */

        /** Initializes the peripherals clock
         */
        PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_QSPI;
        PeriphClkInitStruct.QspiClockSelection = RCC_QSPICLKSOURCE_D1HCLK;
        if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
        {
            ERROR_HANDLER();
        }

        /* Peripheral clock enable */
        __HAL_RCC_QSPI_CLK_ENABLE();

        __HAL_RCC_GPIOG_CLK_ENABLE();
        __HAL_RCC_GPIOF_CLK_ENABLE();
        /**QUADSPI GPIO Configuration
        PG6     ------> QUADSPI_BK1_NCS
        PF6     ------> QUADSPI_BK1_IO3
        PF7     ------> QUADSPI_BK1_IO2
        PF8     ------> QUADSPI_BK1_IO0
        PF10     ------> QUADSPI_CLK
        PF9     ------> QUADSPI_BK1_IO1
        */
        GPIO_InitStruct.Pin = GPIO_PIN_6;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        GPIO_InitStruct.Alternate = GPIO_AF10_QUADSPI;
        HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);

        GPIO_InitStruct.Pin = GPIO_PIN_6 | GPIO_PIN_7 | GPIO_PIN_10;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        GPIO_InitStruct.Alternate = GPIO_AF9_QUADSPI;
        HAL_GPIO_Init(GPIOF, &GPIO_InitStruct);

        GPIO_InitStruct.Pin = GPIO_PIN_8 | GPIO_PIN_9;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        GPIO_InitStruct.Alternate = GPIO_AF10_QUADSPI;
        HAL_GPIO_Init(GPIOF, &GPIO_InitStruct);

        /* USER CODE BEGIN QUADSPI_MspInit 1 */

        /* USER CODE END QUADSPI_MspInit 1 */
    }
}

/**
 * @brief QSPI MSP De-Initialization
 * This function freeze the hardware resources used in this example
 * @param hqspi: QSPI handle pointer
 * @retval None
 */
void HAL_QSPI_MspDeInit(QSPI_HandleTypeDef *hqspi)
{
    if (hqspi->Instance == QUADSPI)
    {
        /* USER CODE BEGIN QUADSPI_MspDeInit 0 */

        /* USER CODE END QUADSPI_MspDeInit 0 */
        /* Peripheral clock disable */
        __HAL_RCC_QSPI_CLK_DISABLE();

        /**QUADSPI GPIO Configuration
        PG6     ------> QUADSPI_BK1_NCS
        PF6     ------> QUADSPI_BK1_IO3
        PF7     ------> QUADSPI_BK1_IO2
        PF8     ------> QUADSPI_BK1_IO0
        PF10     ------> QUADSPI_CLK
        PF9     ------> QUADSPI_BK1_IO1
        */
        HAL_GPIO_DeInit(GPIOG, GPIO_PIN_6);

        HAL_GPIO_DeInit(GPIOF, GPIO_PIN_6 | GPIO_PIN_7 | GPIO_PIN_8 | GPIO_PIN_10 | GPIO_PIN_9);

        /* USER CODE BEGIN QUADSPI_MspDeInit 1 */

        /* USER CODE END QUADSPI_MspDeInit 1 */
    }
}

/**
 * @brief   向SPI Flash发送指令
 * @param   _instruction       ——  要发送的指令
 * @param   _instructionMode   ——  指令线模式
 * @param   _address,          ——  要发送的地址
 * @param   _addressMode       ——  地址线模式
 * @param   _addressSize       ——  地址长度
 * @param   _dummyCycles       ——  空指令周期数
 * @param   _dataMode          ——  数据线模式
 * @param   _nData             ——  数据长度
 * @retval  成功返回HAL_OK
 */
static inline HAL_StatusTypeDef QSPI_SendCommand(uint32_t _instruction,     /* 要发送的指令 */
                                                 uint32_t _instructionMode, /* 指令线模式 */
                                                 uint32_t _address,         /* 要发送的地址 */
                                                 uint32_t _addressMode,     /* 地址线模式 */
                                                 uint32_t _addressSize,     /* 地址长度 */
                                                 uint32_t _dummyCycles,     /* 空指令周期数 */
                                                 uint32_t _dataMode,        /* 数据线模式 */
                                                 uint32_t _nData)           /* 数据长度 */
{
    QSPI_CommandTypeDef cmd = {0};
    /* 参数配置 */
    cmd.Instruction = _instruction;                       /* 指令 */
    cmd.InstructionMode = _instructionMode;               /* 指令线模式 */
    cmd.Address = _address;                               /* 地址 */
    cmd.AddressMode = _addressMode;                       /* 地址线模式 */
    cmd.AddressSize = _addressSize;                       /* 地址长度 */
    cmd.AlternateBytes = 0x00;                            /* 交替字节 */
    cmd.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;    /* 交替字节线模式 */
    cmd.AlternateBytesSize = QSPI_ALTERNATE_BYTES_8_BITS; /* 交替字节长度 */
    cmd.DummyCycles = _dummyCycles;                       /* 设置空指令周期数 */
    cmd.DataMode = _dataMode;                             /* 数据线模式 */
    cmd.NbData = _nData;                                  /* 数据长度 */

    /* 基本配置 */
    cmd.SIOOMode = QSPI_SIOO_INST_EVERY_CMD;          /* 每次都发送指令 */
    cmd.DdrMode = QSPI_DDR_MODE_DISABLE;              /* 关闭DDR模式 */
    cmd.DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY; /* 数据输出延迟 */

    return HAL_QSPI_Command(&hqspi, &cmd, 5000);
}

/*
*********************************************************************************************************
*    函 数 名: bsp_InitQspi
*    功能说明: QSPI Flash硬件初始化，配置基本参数
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void bsp_InitQspi(void)
{
    /* 复位QSPI */
    hqspi.Instance = QUADSPI;
    if (HAL_QSPI_DeInit(&hqspi) != HAL_OK)
    {
        ERROR_HANDLER();
    }

    /* 设置时钟速度，QSPI clock = 200MHz / (ClockPrescaler+1) = 100MHz */
    hqspi.Init.ClockPrescaler = 1;

    /* 设置FIFO阀值，范围1 - 32 */
    hqspi.Init.FifoThreshold = 32;

    /*
        QUADSPI在FLASH驱动信号后过半个CLK周期才对FLASH驱动的数据采样。
        在外部信号延迟时，这有利于推迟数据采样。
    */
    hqspi.Init.SampleShifting = QSPI_SAMPLE_SHIFTING_HALFCYCLE;

    /*Flash大小是2^(FlashSize + 1) = 2^25 = 32MB */
    // QSPI_FLASH_SIZE - 1; 需要扩大一倍，否则内存映射方式最后1个地址时，会异常。
    hqspi.Init.FlashSize = QSPI_FLASH_SIZE;

    /* 命令之间的CS片选至少保持2个时钟周期的高电平 */
    hqspi.Init.ChipSelectHighTime = QSPI_CS_HIGH_TIME_2_CYCLE;

    /*
       MODE0: 表示片选信号空闲期间，CLK时钟信号是低电平
       MODE3: 表示片选信号空闲期间，CLK时钟信号是高电平
    */
    hqspi.Init.ClockMode = QSPI_CLOCK_MODE_0;

    /* QSPI有两个BANK，这里使用的BANK1 */
    hqspi.Init.FlashID = QSPI_FLASH_ID_1;

    /* 仅使用了BANK1，这里是禁止双BANK */
    hqspi.Init.DualFlash = QSPI_DUALFLASH_DISABLE;

    /* 初始化配置QSPI */
    if (HAL_QSPI_Init(&hqspi) != HAL_OK)
    {
        ERROR_HANDLER();
    }
}

/**
 * @brief    W25QXX写使能,将S1寄存器的WEL置位
 * @param    none
 * @retval
 */
static void QSPI_WriteEnable(void)
{
    if (QSPI_SendCommand(QSPI_WRITE_ENABLE_CMD,   /* 要发送的指令 */
                         QSPI_INSTRUCTION_1_LINE, /* 指令线模式 */
                         0,                       /* 要发送的地址 */
                         QSPI_ADDRESS_NONE,       /* 地址线模式 */
                         QSPI_ADDRESS_8_BITS,     /* 地址长度 */
                         0,                       /* 空指令周期数 */
                         QSPI_DATA_NONE,          /* 数据线模式 */
                         0) != HAL_OK)            /* 数据长度 */
    {
        ERROR_HANDLER();
    }

    QSPI_WaitBusy();
}

/**
 * @brief    W25QXX 写易失STATUS寄存器使能指令
 * @param    none
 * @retval
 */
static void QSPI_WriteEnableREG(void)
{
    if (QSPI_SendCommand(QSPI_WRITE_ENABLE_REG_CMD, /* 要发送的指令 */
                         QSPI_INSTRUCTION_1_LINE,   /* 指令线模式 */
                         0,                         /* 要发送的地址 */
                         QSPI_ADDRESS_NONE,         /* 地址线模式 */
                         QSPI_ADDRESS_8_BITS,       /* 地址长度 */
                         0,                         /* 空指令周期数 */
                         QSPI_DATA_NONE,            /* 数据线模式 */
                         0) != HAL_OK)              /* 数据长度 */
    {
        ERROR_HANDLER();
    }

    QSPI_WaitBusy();
}

/**
 * @brief    W25QXX写禁止,将WEL清零
 * @param    none
 * @retval    none
 */
static void QSPI_WriteDisable(void)
{
    if (QSPI_SendCommand(QSPI_WRITE_DISABLE_CMD,  /* 要发送的指令 */
                         QSPI_INSTRUCTION_1_LINE, /* 指令线模式 */
                         0,                       /* 要发送的地址 */
                         QSPI_ADDRESS_NONE,       /* 地址线模式 */
                         QSPI_ADDRESS_8_BITS,     /* 地址长度 */
                         0,                       /* 空指令周期数 */
                         QSPI_DATA_NONE,          /* 数据线模式 */
                         0) != HAL_OK)            /* 数据长度 */
    {
        ERROR_HANDLER();
    }

    QSPI_WaitBusy();
}

/**
 * @brief   读取W25QXX的状态寄存器
 * @param   reg  —— 状态寄存器编号(1~3)
 * @retval  状态寄存器的值
 */
uint8_t QSPI_ReadSR(uint8_t _reg)
{
    uint8_t status_reg;
    uint8_t result = 99;
    switch (_reg)
    {
    case 1:
        /* 读取状态寄存器1的值 */
        status_reg = QSPI_READ_STATU_REG_1;
        break;
    case 2:
        status_reg = QSPI_READ_STATU_REG_2;
        break;
    case 3:
        status_reg = QSPI_READ_STATU_REG_3;
        break;
    default:
        status_reg = QSPI_READ_STATU_REG_1;
    }

    if (QSPI_SendCommand(status_reg,              /* 要发送的指令 */
                         QSPI_INSTRUCTION_1_LINE, /* 指令线模式 */
                         0,                       /* 要发送的地址 */
                         QSPI_ADDRESS_NONE,       /* 地址线模式 */
                         QSPI_ADDRESS_8_BITS,     /* 地址长度 */
                         0,                       /* 空指令周期数 */
                         QSPI_DATA_1_LINE,        /* 数据线模式 */
                         1) != HAL_OK)            /* 数据长度 */
    {
        ERROR_HANDLER();
    }

    if (HAL_QSPI_Receive(&hqspi, (uint8_t *)&result, 5000) != HAL_OK)
    {
        ERROR_HANDLER();
    }

    return result;
}

/**
 * @brief   读取Flash内部的ID
 * @param   none
 * @retval    成功返回 id
 */
uint32_t QSPI_ReadID(void)
{
    uint8_t buf[3]; // recv_buf[0]存放Manufacture ID, recv_buf[1]存放Device ID
    uint32_t id = 0;

    if (QSPI_SendCommand(QSPI_READ_JEDEC_ID,      /* 要发送的指令 */
                         QSPI_INSTRUCTION_1_LINE, /* 指令线模式 */
                         0,                       /* 要发送的地址 */
                         QSPI_ADDRESS_NONE,       /* 地址线模式 */
                         QSPI_ADDRESS_8_BITS,     /* 地址长度 */
                         0,                       /* 空指令周期数 */
                         QSPI_DATA_1_LINE,        /* 数据线模式 */
                         3) != HAL_OK)            /* 数据长度 */
    {
        ERROR_HANDLER();
    }

    if (HAL_QSPI_Receive(&hqspi, buf, 5000) == HAL_OK)
    {
        id = (buf[0] << 16) | (buf[1] << 8) | buf[2];
        return id;
    }

    return id;
}

/**
 * @brief    阻塞等待Flash处于空闲状态
 * @param   none
 * @retval  none
 */
void QSPI_WaitBusy(void)
{
    while ((QSPI_ReadSR(1) & 0x01) == 0x01)
        ; // 等待BUSY位清空
}

/**
 * @brief   写W25QXX的状态寄存器
 * @param   reg     —— 状态寄存器编号(1~3)
 * @param   value   —— 数值
 * @retval  状态寄存器的值
 */
uint8_t QSPI_WriteSR(uint8_t _reg, uint8_t _value)
{
    uint8_t status_reg;

    switch (_reg)
    {
    case 1:
        /* 读取状态寄存器1的值 */
        status_reg = QSPI_WRITE_STATU_REG_1;
        break;
    case 2:
        status_reg = QSPI_WRITE_STATU_REG_2;
        break;
    case 3:
        status_reg = QSPI_WRITE_STATU_REG_3;
        break;
    default:
        status_reg = QSPI_WRITE_STATU_REG_1;
    }

    if (QSPI_SendCommand(status_reg,              /* 要发送的指令 */
                         QSPI_INSTRUCTION_1_LINE, /* 指令线模式 */
                         0,                       /* 要发送的地址 */
                         QSPI_ADDRESS_NONE,       /* 地址线模式 */
                         QSPI_ADDRESS_8_BITS,     /* 地址长度 */
                         0,                       /* 空指令周期数 */
                         QSPI_DATA_1_LINE,        /* 数据线模式 */
                         1) != HAL_OK)            /* 数据长度 */
    {
        ERROR_HANDLER();
    }

    return HAL_QSPI_Transmit(&hqspi, &_value, 5000);
}

/*
*********************************************************************************************************
*   函 数 名: QSPI_EraseSector
*   功能说明: 擦除指定的扇区，扇区大小4KB
*   形    参: _uiSectorAddr : 扇区地址，以4KB为单位的地址，比如0，4096, 8192等
*   返 回 值: 无
*********************************************************************************************************
*/
void QSPI_EraseSector(uint32_t _uiSectorAddr)
{
    /* 等待Flash处于空闲状态 */
    QSPI_WaitBusy();

    /* 写使能 */
    QSPI_WriteEnable();

    if (QSPI_SendCommand(QSPI_SECTOR_ERASE_32ADD_4K_CMD,                        /* 要发送的指令 */
                         QSPI_INSTRUCTION_1_LINE,                               /* 指令线模式 */
                         _uiSectorAddr & (0xffffffff - (QSPI_SECTOR_SIZE - 1)), /* 要发送的地址 */
                         QSPI_ADDRESS_1_LINE,                                   /* 地址线模式 */
                         QSPI_ADDRESS_32_BITS,                                  /* 地址长度 */
                         0,                                                     /* 空指令周期数 */
                         QSPI_DATA_NONE,                                        /* 数据线模式 */
                         0) != HAL_OK)                                          /* 数据长度 */
    {
        ERROR_HANDLER();
    }
}

/*
*********************************************************************************************************
*   函 数 名: QSPI_EraseChip
*   功能说明: 整个芯片擦除
*   形    参: 无
*   返 回 值: 无
*********************************************************************************************************
*/
void QSPI_EraseChip(void)
{
    /* 等待Flash处于空闲状态 */
    QSPI_WaitBusy();

    /* 写使能 */
    QSPI_WriteEnable();

    if (QSPI_SendCommand(QSPI_BULK_ERASE_CMD,     /* 要发送的指令 */
                         QSPI_INSTRUCTION_1_LINE, /* 指令线模式 */
                         0,                       /* 要发送的地址 */
                         QSPI_ADDRESS_NONE,       /* 地址线模式 */
                         QSPI_ADDRESS_24_BITS,    /* 地址长度 */
                         0,                       /* 空指令周期数 */
                         QSPI_DATA_NONE,          /* 数据线模式 */
                         0) != HAL_OK)            /* 数据长度 */
    {
        ERROR_HANDLER();
    }
}

/*
*********************************************************************************************************
*   函 数 名: QSPI_ReadBuffer
*   功能说明: 连续读取若干字节，字节个数不能超出芯片容量。
*   形    参: _pBuf : 数据源缓冲区。
*             _uiReadAddr ：起始地址。
*             _usSize ：数据个数, 可以大于PAGE_SIZE, 但是不能超出芯片总容量。
*   返 回 值: 无
*********************************************************************************************************
*/
void QSPI_ReadBuffer(uint8_t *_pBuf, uint32_t _uiReadAddr, uint32_t _uiSize)
{
    QSPI_CommandTypeDef cmd = {0};

    QSPI_WaitBusy();

    /* 参数配置 */
    cmd.Instruction = QSPI_FAST_READ_32ADD_4_CMD;         /* 指令 */
    cmd.InstructionMode = QSPI_INSTRUCTION_1_LINE;        /* 指令线模式 */
    cmd.Address = _uiReadAddr;                            /* 地址 */
    cmd.AddressMode = QSPI_ADDRESS_4_LINES;               /* 地址线模式 */
    cmd.AddressSize = QSPI_ADDRESS_32_BITS;               /* 地址长度 */
    cmd.AlternateBytes = 0xf0;                            /* 交替字节 */
    cmd.AlternateByteMode = QSPI_ALTERNATE_BYTES_4_LINES; /* 交替字节线模式 */
    cmd.AlternateBytesSize = QSPI_ALTERNATE_BYTES_8_BITS; /* 交替字节长度 */
    cmd.DummyCycles = 4;                                  /* 设置空指令周期数 */
    cmd.DataMode = QSPI_DATA_4_LINES;                     /* 数据线模式 */
    cmd.NbData = _uiSize;                                 /* 数据长度 */
    /* 基本配置 */
    cmd.SIOOMode = QSPI_SIOO_INST_EVERY_CMD;          /* 每次都发送指令 */
    cmd.DdrMode = QSPI_DDR_MODE_DISABLE;              /* 关闭DDR模式 */
    cmd.DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY; /* 数据输出延迟 */

    if (HAL_QSPI_Command(&hqspi, &cmd, 10000) != HAL_OK)
    {
        ERROR_HANDLER();
    }

    /* 读取 */
    if (HAL_QSPI_Receive(&hqspi, _pBuf, 10000) != HAL_OK)
    {
        ERROR_HANDLER();
    }
}

/*
*********************************************************************************************************
*   函 数 名: QSPI_WriteBuffer
*   功能说明: 页编程，页大小256字节，任意页都可以写入
*   形    参: _pBuf : 数据源缓冲区；
*             _uiWriteAddr ：目标区域首地址，即页首地址，比如0， 256, 512等。
*             _uiSize ：数据个数，不能超过页面大小，范围1 - 256。
*   返 回 值: 1:成功， 0：失败
*********************************************************************************************************
*/
void QSPI_WriteBuffer(uint8_t *_pBuf, uint32_t _uiWriteAddr, uint16_t _uiSize)
{
    /* 等待Flash处于空闲状态 */
    QSPI_WaitBusy();
    /* 写使能 */
    QSPI_WriteEnable();

    if (QSPI_SendCommand(QSPI_PAGE_PROG_32ADD_CMD, /* 要发送的指令 */
                         QSPI_INSTRUCTION_1_LINE,  /* 指令线模式 */
                         _uiWriteAddr,             /* 要发送的地址 */
                         QSPI_ADDRESS_1_LINE,      /* 地址线模式 */
                         QSPI_ADDRESS_32_BITS,     /* 地址长度 */
                         0,                        /* 空指令周期数 */
                         QSPI_DATA_1_LINE,         /* 数据线模式 */
                         _uiSize) != HAL_OK)       /* 数据长度 */
    {
        ERROR_HANDLER();
    }

    /* 启动传输 */
    if (HAL_QSPI_Transmit(&hqspi, _pBuf, 10000) != HAL_OK)
    {
        ERROR_HANDLER();
    }
}

/*
*********************************************************************************************************
*    函 数 名: QSPI_MemoryMapped
*    功能说明: QSPI内存映射，地址 0x90000000
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void QSPI_MemoryMapped(void)
{
    QSPI_CommandTypeDef cmd = {0};
    QSPI_MemoryMappedTypeDef cfg = {0};

    /* 参数配置 */
    cmd.Instruction = QSPI_FAST_READ_32ADD_4_CMD;         /* 指令 */
    cmd.InstructionMode = QSPI_INSTRUCTION_1_LINE;        /* 指令线模式 */
    cmd.Address = 0;                                      /* 地址 */
    cmd.AddressMode = QSPI_ADDRESS_4_LINES;               /* 地址线模式 */
    cmd.AddressSize = QSPI_ADDRESS_32_BITS;               /* 地址长度 */
    cmd.AlternateBytes = 0xff;                            /* 交替字节 */
    cmd.AlternateByteMode = QSPI_ALTERNATE_BYTES_4_LINES; /* 交替字节线模式 */
    cmd.AlternateBytesSize = QSPI_ALTERNATE_BYTES_8_BITS; /* 交替字节长度 */
    cmd.DummyCycles = 4;                                  /* 设置空指令周期数 */
    cmd.DataMode = QSPI_DATA_4_LINES;                     /* 数据线模式 */
    cmd.NbData = 0;                                       /* 数据长度 */
    /* 基本配置 */
    cmd.SIOOMode = QSPI_SIOO_INST_EVERY_CMD;          /* 每次都发送指令 */
    cmd.DdrMode = QSPI_DDR_MODE_DISABLE;              /* 关闭DDR模式 */
    cmd.DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY; /* 数据输出延迟 */

    /* 关闭溢出计数 */
    cfg.TimeOutActivation = QSPI_TIMEOUT_COUNTER_DISABLE;
    cfg.TimeOutPeriod = 0;
    /* 写入配置 */
    if (HAL_QSPI_MemoryMapped(&hqspi, &cmd, &cfg) != HAL_OK)
    {
        ERROR_HANDLER();
    }
}

#if defined(__SHELL_H__) && defined(DEBUG_MODE)
static int cmd_qspi(int argc, char *argv[])
{
#define __is_print(ch) ((unsigned int)((ch) - ' ') < 127u - ' ')
#define HEXDUMP_WIDTH 16

    static uint8_t s_probe = 0;

    const char *help_info[] = {
        "qspi probe Select 1 - 2",
        "qspi read reg/buff",
        "qspi write reg/buff",
        "qspi erase sector/chip",
        "qspi xip init/read"};

    // printf("\r\nargc = %d\r\n\r\n", argc);

    if (argc < 2)
    {
        printf("Error Command.\r\nUsage:\r\n");
        for (uint8_t i = 0; i < sizeof(help_info) / sizeof(help_info[0]); i++)
        {
            printf("%s\r\n", help_info[i]);
        }
        printf("\r\n");
    }
    else
    {
        /* 选择串口号 */
        if (!strcmp(argv[1], "probe"))
        {
            uint8_t num;
            num = atoi(argv[2]);
            if (num >= 1 && num <= 2)
            {
                uint32_t id = QSPI_ReadID();
                printf("QSPI_ReadID = 0x%08x\r\n", id);
                printf("hqspi.Init.FlashSize = %d\r\n", hqspi.Init.FlashSize);
                s_probe = num;

                return 0;
            }
            else
            {
                printf("Error Command.\r\n%s %s 1-2.\r\n",
                       argv[0], argv[1]);
                return -1;
            }
        }
        else if (!strcmp(argv[1], "read"))
        {
            if (!strcmp(argv[2], "reg"))
            {
                uint8_t reg;
                if (argc < 4)
                {
                    printf("Error Command.\r\n%s %s %s 1-3.\r\n",
                           argv[0], argv[1], argv[2]);
                    return -1;
                }
                reg = QSPI_ReadSR(atoi(argv[3]));
                printf("reg = 0b%c%c%c%c%c%c%c%c\r\n", BYTE_TO_BINARY(reg));

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
                uint32_t add = atoi(argv[3]);
                uint16_t size = atoi(argv[4]);
                uint8_t *buff = malloc(size);

                if (buff)
                {
                    printf("Read buff success. add = %d size = %d.\r\nThe data is:\r\n", add, size);

                    QSPI_ReadBuffer(buff, add, size);

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
                printf("read parameter Error.\r\n%s\r\n", help_info[1]);

                return -1;
            }
        }
        else if (!strcmp(argv[1], "write"))
        {

            if (!strcmp(argv[2], "reg"))
            {
                if (argc < 5)
                {
                    printf("Error Command\r\n%s %s %s 1-3 data.\r\n",
                           argv[0], argv[1], argv[2]);

                    return -1;
                }
                uint8_t reg = atoi(argv[3]);
                uint8_t data = atoi(argv[4]);
                QSPI_WriteSR(reg, data);
                printf("REG = %d data = %d\r\n", reg, data);

                return QSPI_ReadSR(reg);
            }
            else if (!strcmp(argv[2], "buff"))
            {
                if (argc < 5)
                {
                    printf("Error Command\r\n%s %s %s add str.\r\n",
                           argv[0], argv[1], argv[2]);

                    return -1;
                }

                QSPI_WriteBuffer((uint8_t *)argv[4], atoi(argv[3]), str_len(argv[4]));

                return 0;
            }
            else
            {
                printf("write parameter Error.\r\n%s\r\n", help_info[2]);
                return -1;
            }
        }
        else if (!strcmp(argv[1], "erase"))
        {
            if (!strcmp(argv[2], "chip"))
            {
                QSPI_EraseChip();

                return 0;
            }
            else if (!strcmp(argv[2], "sector"))
            {
                if (argc < 4)
                {
                    printf("Error Command\r\n%s %s %s add 4K.\r\n",
                           argv[0], argv[1], argv[2]);
                    return -1;
                }
                uint32_t addr = (0xffffffff - (QSPI_SECTOR_SIZE - 1)) & (uint32_t)atoi(argv[3]);
                printf("Erase Sector address = 0x%08x %d\r\n", addr, addr);
                QSPI_EraseSector(addr);

                return 0;
            }
            else
            {
                printf("write parameter Error.\r\n%s\r\n", help_info[3]);
                return -1;
            }
        }
        else if (!strcmp(argv[1], "xip"))
        {
            if (!strcmp(argv[2], "init"))
            {
                QSPI_MemoryMapped();

                return 0;
            }
            else if (!strcmp(argv[2], "read"))
            {
                if (argc < 4)
                {
                    printf("Error Command\r\n%s %s %s add.\r\n",
                           argv[0], argv[1], argv[2]);
                    return -1;
                }
                uint32_t addr = 0x90000000 + atoi(argv[3]);
                printf("qspi address 0x%08x = 0x%08x\r\n", addr, *(uint32_t *)addr);

                return 0;
            }
            else
            {
                printf("write parameter Error.\r\n%s\r\n", help_info[4]);
                return -1;
            }
        }
        else
        {
            printf("Error Command\r\nUsage:\r\n");
            for (uint32_t i = 0; i < sizeof(help_info) / sizeof(char *); i++)
            {
                printf("%s\r\n", help_info[i]);
            }
            printf("\r\n");
        }
    }

    return -1;
}
// 导出到命令列表里
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), qspi, cmd_qspi, qspi[probe read write erase]);
#endif // #ifdef DEBUG_MODE

/***************************** 安富莱电子 www.armfly.com (END OF FILE) *********************************/
