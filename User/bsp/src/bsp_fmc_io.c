/**
 * @file    bsp_fmc_io.c
 * @brief   FMC 总线扩展 IO 驱动（HC574 锁存器，FMC @ 0x68200000，当前仅 LED1-LED4 已接线）
 */

#include "bsp.h"

/* FMC 总线地址 = 0x60001000（STM32H743XI-Demo 实际使用 FMC_NE1）*/

#define HC574_PORT *(uint32_t *)0x60001000

__IO uint32_t g_HC574; /* 保存74HC574端口状态 */

static void HC574_ConfigGPIO(void);
static void HC574_ConfigFMC(void);

/**
 * @brief  初始化 FMC 扩展 IO（GPIO + FMC 时序），上电执行一次
 * @retval 无
 */
void bsp_InitExtIO(void)
{
    HC574_ConfigGPIO();
    HC574_ConfigFMC();

    /* 将开发板一些片选，LED口设置为高 */
    g_HC574 = (HC574_NRF24L01_CE | HC574_VS1053_XDCS | HC574_LED1 | HC574_LED2 | HC574_LED3 | HC574_LED4);
    HC574_PORT = g_HC574; /* 写硬件端口，更改IO状态 */
}

/**
 * @brief  设置 74HC574 指定引脚输出电平
 * @param  _pin   引脚掩码（GPIO_PIN_0..31），每次只能选一个
 * @param  _value 目标电平，0 或 1
 * @retval 无
 */
void HC574_SetPin(uint32_t _pin, uint8_t _value)
{
    if (_value == 0)
    {
        g_HC574 &= (~_pin);
    }
    else
    {
        g_HC574 |= _pin;
    }
    HC574_PORT = g_HC574;
}

/**
 * @brief  翻转 74HC574 指定引脚输出电平
 * @param  _pin  引脚掩码（GPIO_PIN_0..31），每次只能选一个
 * @retval 无
 */
void HC574_TogglePin(uint32_t _pin)
{
    if (g_HC574 & _pin)
    {
        g_HC574 &= (~_pin);
    }
    else
    {
        g_HC574 |= _pin;
    }
    HC574_PORT = g_HC574;
}

/**
 * @brief  读取 74HC574 指定引脚当前输出状态
 * @param  _pin  引脚掩码（GPIO_PIN_0..31），每次只能选一个
 * @retval 0 或 1
 */
uint8_t HC574_GetPin(uint32_t _pin)
{
    if (g_HC574 & _pin)
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

/**
 * @brief  配置 FMC 数据/地址/控制引脚为复用推挽输出
 * @retval 无
 */
static void HC574_ConfigGPIO(void)
{
    /*
        安富莱STM32-H7开发板接线方法：4片74HC574挂在FMC 32位总线上。1个地址端口可以扩展出32个IO
        PD0/FMC_D2
        PD1/FMC_D3
        PD4/FMC_NOE        ---- 读控制信号，OE = Output Enable ， N 表示低有效
        PD5/FMC_NWE        -XX- 写控制信号，AD7606 只有读，无写信号
        PD8/FMC_D13
        PD9/FMC_D14
        PD10/FMC_D15
        PD14/FMC_D0
        PD15/FMC_D1

        PE7/FMC_D4
        PE8/FMC_D5
        PE9/FMC_D6
        PE10/FMC_D7
        PE11/FMC_D8
        PE12/FMC_D9
        PE13/FMC_D10
        PE14/FMC_D11
        PE15/FMC_D12

        PG0/FMC_A10        --- 和主片选FMC_NE2一起译码
        PG1/FMC_A11        --- 和主片选FMC_NE2一起译码
        XX --- PG9/FMC_NE2        --- 主片选（OLED, 74HC574, DM9000, AD7606）
         --- PD7/FMC_NE1        --- 主片选（OLED, 74HC574, DM9000, AD7606）

        +-------------------+------------------+
        +   32-bits Mode: D31-D16              +
        +-------------------+------------------+
        | PH8 <-> FMC_D16   | PI0 <-> FMC_D24  |
        | PH9 <-> FMC_D17   | PI1 <-> FMC_D25  |
        | PH10 <-> FMC_D18  | PI2 <-> FMC_D26  |
        | PH11 <-> FMC_D19  | PI3 <-> FMC_D27  |
        | PH12 <-> FMC_D20  | PI6 <-> FMC_D28  |
        | PH13 <-> FMC_D21  | PI7 <-> FMC_D29  |
        | PH14 <-> FMC_D22  | PI9 <-> FMC_D30  |
        | PH15 <-> FMC_D23  | PI10 <-> FMC_D31 |
        +------------------+-------------------+
    */

    GPIO_InitTypeDef gpio_init_structure;

    /* 使能 GPIO时钟 */
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();
    __HAL_RCC_GPIOG_CLK_ENABLE();
    __HAL_RCC_GPIOH_CLK_ENABLE();
    __HAL_RCC_GPIOI_CLK_ENABLE();

    /* 使能FMC时钟 */
    __HAL_RCC_FMC_CLK_ENABLE();

    /* 设置 GPIOD 相关的IO为复用推挽输出 */
    gpio_init_structure.Mode = GPIO_MODE_AF_PP;
    gpio_init_structure.Pull = GPIO_PULLUP;
    gpio_init_structure.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio_init_structure.Alternate = GPIO_AF12_FMC;

    /* 配置GPIOD */
    gpio_init_structure.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_7 |
                              GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_14 |
                              GPIO_PIN_15;
    HAL_GPIO_Init(GPIOD, &gpio_init_structure);

    /* 配置GPIOE */
    gpio_init_structure.Pin = GPIO_PIN_7 | GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 |
                              GPIO_PIN_11 | GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14 |
                              GPIO_PIN_15;
    HAL_GPIO_Init(GPIOE, &gpio_init_structure);

    /* 配置GPIOG */
    gpio_init_structure.Pin = GPIO_PIN_0 | GPIO_PIN_1;
    HAL_GPIO_Init(GPIOG, &gpio_init_structure);

    /* 配置GPIOH */
    gpio_init_structure.Pin = GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_11 | GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15;
    HAL_GPIO_Init(GPIOH, &gpio_init_structure);

    /* 配置GPIOI */
    gpio_init_structure.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3 | GPIO_PIN_6 | GPIO_PIN_7 | GPIO_PIN_9 | GPIO_PIN_10;
    HAL_GPIO_Init(GPIOI, &gpio_init_structure);
}

/**
 * @brief  配置 FMC NOR/SRAM Bank1 访问时序（Mode A，5-2-2-1 等待周期）
 * @retval 无
 */
static void HC574_ConfigFMC(void)
{
    SRAM_HandleTypeDef hsram = {0};
    FMC_NORSRAM_TimingTypeDef SRAM_Timing = {0};

    hsram.Instance = FMC_NORSRAM_DEVICE;
    hsram.Extended = FMC_NORSRAM_EXTENDED_DEVICE;

    /* FMC使用的HCLK3，主频200MHz，1个FMC时钟周期就是5ns */
    /* SRAM 总线时序配置 4-1-2-1-2-2 不稳定，5-2-2-1-2-2 稳定 */
    SRAM_Timing.AddressSetupTime = 5;           /* 5*5ns=25ns，地址建立时间，范围0 -15个FMC时钟周期个数 */
    SRAM_Timing.AddressHoldTime = 2;            /* 地址保持时间，配置为模式A时，用不到此参数 范围1 -15个时钟周期个数 */
    SRAM_Timing.DataSetupTime = 2;              /* 2*5ns=10ns，数据保持时间，范围1 -255个时钟周期个数 */
    SRAM_Timing.BusTurnAroundDuration = 1;      /* 此配置用不到这个参数 */
    SRAM_Timing.CLKDivision = 2;                /* 此配置用不到这个参数 */
    SRAM_Timing.DataLatency = 2;                /* 此配置用不到这个参数 */
    SRAM_Timing.AccessMode = FMC_ACCESS_MODE_A; /* 配置为模式A */

    hsram.Init.NSBank = FMC_NORSRAM_BANK1;                        /* 使用的BANK1，即使用的片选FMC_NE1 */
    hsram.Init.DataAddressMux = FMC_DATA_ADDRESS_MUX_DISABLE;     /* 禁止地址数据复用 */
    hsram.Init.MemoryType = FMC_MEMORY_TYPE_SRAM;                 /* 存储器类型SRAM */
    hsram.Init.MemoryDataWidth = FMC_NORSRAM_MEM_BUS_WIDTH_32;    /* 32位总线宽度 */
    hsram.Init.BurstAccessMode = FMC_BURST_ACCESS_MODE_DISABLE;   /* 关闭突发模式 */
    hsram.Init.WaitSignalPolarity = FMC_WAIT_SIGNAL_POLARITY_LOW; /* 用于设置等待信号的极性，关闭突发模式，此参数无效 */
    hsram.Init.WaitSignalActive = FMC_WAIT_TIMING_BEFORE_WS;      /* 关闭突发模式，此参数无效 */
    hsram.Init.WriteOperation = FMC_WRITE_OPERATION_ENABLE;       /* 用于使能或者禁止写保护 */
    hsram.Init.WaitSignal = FMC_WAIT_SIGNAL_DISABLE;              /* 关闭突发模式，此参数无效 */
    hsram.Init.ExtendedMode = FMC_EXTENDED_MODE_DISABLE;          /* 禁止扩展模式 */
    hsram.Init.AsynchronousWait = FMC_ASYNCHRONOUS_WAIT_DISABLE;  /* 用于异步传输期间，使能或者禁止等待信号，这里选择关闭 */
    hsram.Init.WriteBurst = FMC_WRITE_BURST_DISABLE;              /* 禁止写突发 */
    hsram.Init.ContinuousClock = FMC_CONTINUOUS_CLOCK_SYNC_ONLY;  /* 仅同步模式才做时钟输出 */
    hsram.Init.WriteFifo = FMC_WRITE_FIFO_ENABLE;                 /* 使能写FIFO */

    /* 初始化SRAM控制器 */
    if (HAL_SRAM_Init(&hsram, &SRAM_Timing, &SRAM_Timing) != HAL_OK)
    {
        /* 初始化错误 */
        Error_Handler(__FILE__, __LINE__);
    }
}

/**
 * @brief  测试 74HC574 扩展 IO：32 路引脚交替翻转（周期 200+200ms），死循环
 * @retval 无
 */
void bsp_TestExtIO(void)
{
    while (1)
    {
        HC574_SetPin(GPIO_PIN_0, 0);
        HC574_SetPin(GPIO_PIN_1, 0);
        HC574_SetPin(GPIO_PIN_2, 0);
        HC574_SetPin(GPIO_PIN_3, 0);
        HC574_SetPin(GPIO_PIN_4, 0);
        HC574_SetPin(GPIO_PIN_5, 0);
        HC574_SetPin(GPIO_PIN_6, 0);
        HC574_SetPin(GPIO_PIN_7, 0);
        HC574_SetPin(GPIO_PIN_8, 0);
        HC574_SetPin(GPIO_PIN_9, 0);
        HC574_SetPin(GPIO_PIN_10, 0);
        HC574_SetPin(GPIO_PIN_11, 0);
        HC574_SetPin(GPIO_PIN_12, 0);
        HC574_SetPin(GPIO_PIN_13, 0);
        HC574_SetPin(GPIO_PIN_14, 0);
        HC574_SetPin(GPIO_PIN_15, 0);

        HC574_SetPin(GPIO_PIN_16, 0);
        HC574_SetPin(GPIO_PIN_17, 1);
        HC574_SetPin(GPIO_PIN_18, 0);
        HC574_SetPin(GPIO_PIN_19, 1);
        HC574_SetPin(GPIO_PIN_20, 0);
        HC574_SetPin(GPIO_PIN_21, 1);
        HC574_SetPin(GPIO_PIN_22, 0);
        HC574_SetPin(GPIO_PIN_23, 1);
        HC574_SetPin(GPIO_PIN_24, 0);
        HC574_SetPin(GPIO_PIN_25, 1);
        HC574_SetPin(GPIO_PIN_26, 0);
        HC574_SetPin(GPIO_PIN_27, 1);
        HC574_SetPin(GPIO_PIN_28, 0);
        HC574_SetPin(GPIO_PIN_29, 1);
        HC574_SetPin(GPIO_PIN_30, 0);
        HC574_SetPin(GPIO_PIN_31, 1);

        HAL_Delay(200);

        HC574_SetPin(GPIO_PIN_0, 1);
        HC574_SetPin(GPIO_PIN_1, 1);
        HC574_SetPin(GPIO_PIN_2, 1);
        HC574_SetPin(GPIO_PIN_3, 1);
        HC574_SetPin(GPIO_PIN_4, 1);
        HC574_SetPin(GPIO_PIN_5, 1);
        HC574_SetPin(GPIO_PIN_6, 1);
        HC574_SetPin(GPIO_PIN_7, 1);
        HC574_SetPin(GPIO_PIN_8, 1);
        HC574_SetPin(GPIO_PIN_9, 1);
        HC574_SetPin(GPIO_PIN_10, 1);
        HC574_SetPin(GPIO_PIN_11, 1);
        HC574_SetPin(GPIO_PIN_12, 1);
        HC574_SetPin(GPIO_PIN_13, 1);
        HC574_SetPin(GPIO_PIN_14, 1);
        HC574_SetPin(GPIO_PIN_15, 1);

        HC574_SetPin(GPIO_PIN_16, 1);
        HC574_SetPin(GPIO_PIN_17, 0);
        HC574_SetPin(GPIO_PIN_18, 1);
        HC574_SetPin(GPIO_PIN_19, 0);
        HC574_SetPin(GPIO_PIN_20, 1);
        HC574_SetPin(GPIO_PIN_21, 0);
        HC574_SetPin(GPIO_PIN_22, 1);
        HC574_SetPin(GPIO_PIN_23, 0);
        HC574_SetPin(GPIO_PIN_24, 1);
        HC574_SetPin(GPIO_PIN_25, 0);
        HC574_SetPin(GPIO_PIN_26, 1);
        HC574_SetPin(GPIO_PIN_27, 0);
        HC574_SetPin(GPIO_PIN_28, 1);
        HC574_SetPin(GPIO_PIN_29, 0);
        HC574_SetPin(GPIO_PIN_30, 1);
        HC574_SetPin(GPIO_PIN_31, 0);

        HAL_Delay(200);
    }
}

/* end of file */
