/**
 * @file    bsp.c
 * @brief   板级支持包（BSP）主文件，负责 MPU/Cache/时钟配置及所有外设模块初始化
 */
#include "bsp.h"
#include "perf_counter.h"
#include "bsp_fmc_sdram.h"
/*
*********************************************************************************************************
*                                       函数声明
*********************************************************************************************************
*/
static void SystemClock_Config(void);
static void CPU_CACHE_Enable(void);
static void MPU_Config(void);

/**
 * @brief  系统初始化：MPU、D/I-Cache 及系统时钟配置
 * @retval 无
 */
void System_Init(void)
{
    /* 配置MPU */
    MPU_Config();

    /* 使能L1 Cache */
    CPU_CACHE_Enable();

    /*
       STM32H7xx HAL 库初始化，此时系统用的还是H7自带的64MHz，HSI时钟:
       - 调用函数HAL_InitTick，初始化滴答时钟中断1ms。
       - 设置NVIV优先级分组为4。
     */
    HAL_Init();

    /*
       配置系统时钟到400MHz
       - 切换使用HSE。
       - 此函数会更新全局变量SystemCoreClock，并重新配置HAL_InitTick。
    */
    SystemClock_Config();

    /*
       Event Recorder：
       - 可用于代码执行时间测量，MDK5.25及其以上版本才支持，IAR不支持。
       - 默认不开启，如果要使能此选项，务必看V7开发板用户手册第8章
    */
#if Enable_EventRecorder == 1
    /* 初始化EventRecorder并开启 */
    EventRecorderInitialize(EventRecordAll, 1U);
    EventRecorderStart();
#endif
}

/**
 * @brief  初始化所有硬件设备（外设寄存器 + 全局变量），只需调用一次
 * @retval 无
 */
void bsp_Init(void)
{
    bsp_InitExtSDRAM();       /* 初始化SDRAM */
    bsp_InitQspi();           /* 初始化QSPI */
    init_cycle_counter(TRUE); /* 初始化perf_counter库 定时器已经初始化 */
    bsp_InitKey();            /* 按键初始化，要放在滴答定时器之前，因为按钮检测是通过滴答定时器扫描 */
    bsp_Init_dma();           /* 初始化DMA */
    bsp_InitUart();           /* 初始化串口 */
    userInitShell();          /* 初始化shell */
    bsp_InitExtIO();          /* 初始化FMC总线74HC574扩展IO. 必须在 bsp_InitLed()前执行 */
    bsp_InitLed();            /* 初始化LED */
    BEEP_InitHard();          /* 初始化beep */
    userInitMultiTime();      /* 初始化MultiTime */
    bsp_InitTFT();            /* 初始化LCD */
    bsp_Init_SD();            /* 初始化SD卡 */
}

/**
 * @brief  初始化系统时钟（PLL from HSE 25MHz → SYSCLK 400MHz / HCLK 200MHz / APBx 100MHz）
 * @note   PLLM=5, PLLN=160, PLLP=2, PLLQ=4, PLLR=2; Flash Latency=4 WS; VDD=3.3V
 * @retval 无
 */
static void SystemClock_Config(void)
{
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    HAL_StatusTypeDef ret = HAL_OK;

    /* 锁住SCU(Supply configuration update) */
    MODIFY_REG(PWR->CR3, PWR_CR3_SCUEN, 0);

    /*
      1、芯片内部的LDO稳压器输出的电压范围，可选VOS1，VOS2和VOS3，不同范围对应不同的Flash读速度，
         详情看参考手册的Table 12的表格。
      2、这里选择使用VOS1，电压范围1.15V - 1.26V。
    */
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    while (!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY))
    {
    }

    /* 使能HSE，并选择HSE作为PLL时钟源 */
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.HSIState = RCC_HSI_OFF;
    RCC_OscInitStruct.CSIState = RCC_CSI_OFF;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;

    RCC_OscInitStruct.PLL.PLLM = 5;
    RCC_OscInitStruct.PLL.PLLN = 160;
    RCC_OscInitStruct.PLL.PLLP = 2;
    RCC_OscInitStruct.PLL.PLLR = 2;
    RCC_OscInitStruct.PLL.PLLQ = 4;

    RCC_OscInitStruct.PLL.PLLVCOSEL = RCC_PLL1VCOWIDE;
    RCC_OscInitStruct.PLL.PLLRGE = RCC_PLL1VCIRANGE_2;
    ret = HAL_RCC_OscConfig(&RCC_OscInitStruct);
    if (ret != HAL_OK)
    {
        Error_Handler(__FILE__, __LINE__);
    }

    /*
       选择PLL的输出作为系统时钟
       配置RCC_CLOCKTYPE_SYSCLK系统时钟
       配置RCC_CLOCKTYPE_HCLK 时钟，对应AHB1，AHB2，AHB3和AHB4总线
       配置RCC_CLOCKTYPE_PCLK1时钟，对应APB1总线
       配置RCC_CLOCKTYPE_PCLK2时钟，对应APB2总线
       配置RCC_CLOCKTYPE_D1PCLK1时钟，对应APB3总线
       配置RCC_CLOCKTYPE_D3PCLK1时钟，对应APB4总线
    */
    RCC_ClkInitStruct.ClockType = (RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_D1PCLK1 | RCC_CLOCKTYPE_PCLK1 |
                                   RCC_CLOCKTYPE_PCLK2 | RCC_CLOCKTYPE_D3PCLK1);

    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV2;
    RCC_ClkInitStruct.APB3CLKDivider = RCC_APB3_DIV2;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV2;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV2;
    RCC_ClkInitStruct.APB4CLKDivider = RCC_APB4_DIV2;

    /* 此函数会更新SystemCoreClock，并重新配置HAL_InitTick */
    ret = HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4);
    if (ret != HAL_OK)
    {
        Error_Handler(__FILE__, __LINE__);
    }

    /*
      使用IO的高速模式，要使能IO补偿，即调用下面三个函数
      （1）使能CSI clock
      （2）使能SYSCFG clock
      （3）使能I/O补偿单元， 设置SYSCFG_CCCSR寄存器的bit0
    */
    __HAL_RCC_CSI_ENABLE();

    __HAL_RCC_SYSCFG_CLK_ENABLE();

    HAL_EnableCompensationCell();

    /* AXI SRAM的时钟是上电自动使能的，而D2域的SRAM1，SRAM2和SRAM3要单独使能 */
#if 1
    __HAL_RCC_D2SRAM1_CLK_ENABLE();
    __HAL_RCC_D2SRAM2_CLK_ENABLE();
    __HAL_RCC_D2SRAM3_CLK_ENABLE();

    __HAL_RCC_BKPRAM_CLKAM_ENABLE();
    __HAL_RCC_D3SRAM1_CLKAM_ENABLE();
#endif
}

/**
 * @brief  错误处理（死循环），用于断言失败定位
 * @param  file 源代码文件名（传入 __FILE__）
 * @param  line 代码行号（传入 __LINE__）
 * @retval 无
 */
void Error_Handler(char *file, uint32_t line)
{
    /*
        用户可以添加自己的代码报告源代码文件名和代码行号，比如将错误文件和行号打印到串口
        printf("Wrong parameters value: file %s on line %d\r\n", file, line)
    */

    /* 这是一个死循环，断言失败时程序会在此处死机，以便于用户查错 */
    if (line == 0)
    {
        return;
    }

    while (1)
    {
    }
}

/**
 * @brief  配置 MPU（Memory Protection Unit，内存保护单元）
 *
 * ---------------------------------------------------------------------------
 * 【MPU 是什么？为什么要配？】
 *
 * STM32H7 的 Cortex-M7 内核开启了 D-Cache 后，CPU 读写内存会经过 Cache，
 * 而 DMA、LTDC、以太网 MAC 等外设直接访问物理内存（SRAM / SDRAM / 寄存器），
 * 不经过 Cache。若不对某段地址声明正确的“内存属性”，就会出现：
 *   - CPU 写了数据，外设读到的还是旧值（Cache 里没刷回内存）
 *   - 外设写了数据，CPU 读到的还是旧值（Cache 里仍是脏数据）
 *   - 对 FMC 寄存器做 Cache 写回，总线上出现重复片选/写使能，硬件异常
 *
 * MPU 的作用：把地址空间切成若干 Region（本芯片最多 8 个），为每段地址
 * 指定访问权限和 Cache 策略，让 CPU 与外设对同一块内存的行为一致。
 *
 * 本函数在 System_Init() 里、开启 D-Cache 之前调用（见 CPU_CACHE_Enable）。
 *
 * ---------------------------------------------------------------------------
 * 【Region 结构体各字段含义（HAL 封装）】
 *
 *   BaseAddress      区域起始地址（必须按 Size 对齐，例如 4MB 区须 4MB 对齐）
 *   Size             区域大小，只能是 2 的幂（32B ~ 4GB）
 *   AccessPermission 读/写权限（本工程全部 MPU_REGION_FULL_ACCESS）
 *   IsCacheable      是否可缓存（1=可进 D-Cache）
 *   IsBufferable     是否写缓冲（Write-Back 时常配合使用）
 *   TypeExtField     TEX 位，与 C/B 组合决定内存类型（见下表）
 *   IsShareable      多核共享属性（M7 单核一般 NOT_SHAREABLE）
 *   DisableExec      是否禁止在该区域取指执行（显存通常禁执行）
 *   SubRegionDisable 把区域均分 8 段，按位关闭子区域（本工程未用）
 *   Number           使用 MPU 硬件槽位编号 0~7
 *
 * ---------------------------------------------------------------------------
 * 【Cortex-M7 常用内存属性组合（TEX=0 时）】
 *
 *   C=0, B=0  →  Normal，不可缓存          → 外设寄存器映射、FMC IO、NAND
 *   C=1, B=0  →  Normal，Write-Through      → 写穿：写操作同时更新 Cache 和内存
 *   C=1, B=1  →  Normal，Write-Back         → 写回：只更新 Cache，需手动 Clean
 *   C=0, B=1  →  Device                     → 严格顺序的设备内存（ETH 描述符）
 *
 * Write-Through：性能略低于 Write-Back，但 CPU 写入后内存中立即可见，适合
 * 与 DMA/LTDC 共享的缓冲区。Write-Back 最快，但 DMA 前后必须调用
 * SCB_CleanDCache_by_Addr / SCB_InvalidateDCache_by_Addr（本工程 SD/UART 已做）。
 *
 * ---------------------------------------------------------------------------
 * 【本板 Region 分配一览】
 *
 *   Region 0  0x30040000  256B   Device，不可缓存     ETH DMA 描述符（预留给 LwIP）
 *   Region 1  0x30044000  16KB   Write-Through        LwIP 协议栈堆内存
 *   Region 2  0x24000000  512KB  Write-Through        片内 AXI SRAM（主 RAM）
 *   Region 3  0x60000000  64KB   不可缓存             FMC 扩展 IO（74HC574）
 *   Region 4  0xC0000000  32MB   Write-Through        外部 SDRAM 全片（显存 + 应用区）
 *   Region 5  0x80000000  512MB  不可缓存             FMC NAND Flash
 *
 * 注：Region 4 覆盖整片 32MB SDRAM。若不显式划区，0xC0000000 在 Cortex-M7 默认
 * 内存图中属于 Device 类型，会导致非对齐访问 fault 且无法缓存（速度慢）。统一配为
 * Normal/Write-Through 后，显存与应用区行为一致，可正常做堆、大缓冲、LVGL 缓冲等。
 *
 * @retval 无
 */
static void MPU_Config(void)
{
    MPU_Region_InitTypeDef MPU_InitStruct;

    /* 配置前必须先关闭 MPU，否则无法修改 Region 寄存器 */
    HAL_MPU_Disable();

#if 1 /* 以太网 + LwIP 尚未启用时可改为 0，释放 Region 0/1 */
    /*
     * Region 0：以太网 DMA 描述符（位于 D2 域 AXI SRAM）
     * -----------------------------------------------------------------------
     * 地址 0x30040000 是 ST 以太网驱动存放 RX/TX 描述符链表的固定位置。
     * 描述符由 ETH MAC 硬件直接读写，必须设为 Device、不可缓存；
     * 若设为 Cacheable，CPU 修改的描述符可能只留在 Cache，MAC 看不到更新。
     */
    MPU_InitStruct.Enable = MPU_REGION_ENABLE;
    MPU_InitStruct.BaseAddress = 0x30040000;
    MPU_InitStruct.Size = MPU_REGION_SIZE_256B;
    MPU_InitStruct.AccessPermission = MPU_REGION_FULL_ACCESS;
    MPU_InitStruct.IsBufferable = MPU_ACCESS_BUFFERABLE;
    MPU_InitStruct.IsCacheable = MPU_ACCESS_NOT_CACHEABLE;
    MPU_InitStruct.IsShareable = MPU_ACCESS_NOT_SHAREABLE;
    MPU_InitStruct.Number = MPU_REGION_NUMBER0;
    MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL0;
    MPU_InitStruct.SubRegionDisable = 0x00;
    MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_ENABLE;
    HAL_MPU_ConfigRegion(&MPU_InitStruct);

    /*
     * Region 1：LwIP 堆内存（0x30044000，16KB）
     * -----------------------------------------------------------------------
     * LwIP 的 pbuf / 发送缓冲区放在 D2 SRAM。以太网 DMA 会读取这里的 Tx 数据，
     * 使用 Write-Through：CPU 写入后数据立即落到物理 RAM，DMA 能直接读到。
     */
    MPU_InitStruct.Enable = MPU_REGION_ENABLE;
    MPU_InitStruct.BaseAddress = 0x30044000;
    MPU_InitStruct.Size = MPU_REGION_SIZE_16KB;
    MPU_InitStruct.AccessPermission = MPU_REGION_FULL_ACCESS;
    MPU_InitStruct.IsBufferable = MPU_ACCESS_NOT_BUFFERABLE;
    MPU_InitStruct.IsCacheable = MPU_ACCESS_CACHEABLE;
    MPU_InitStruct.IsShareable = MPU_ACCESS_NOT_SHAREABLE;
    MPU_InitStruct.Number = MPU_REGION_NUMBER1;
    MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL0;
    MPU_InitStruct.SubRegionDisable = 0x00;
    MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_ENABLE;
    HAL_MPU_ConfigRegion(&MPU_InitStruct);
#endif

    /*
     * Region 2：片内 AXI SRAM（0x24000000，512KB）
     * -----------------------------------------------------------------------
     * H7 的主 SRAM，存放栈、堆、全局变量、DMA 缓冲区（如 UART kfifo）等。
     * 使用 Write-Through 而非 Write-Back：兼顾访问速度与 DMA 一致性，
     * 减少每次 DMA 传输前手动 Clean Cache 的次数。
     */
    MPU_InitStruct.Enable = MPU_REGION_ENABLE;
    MPU_InitStruct.BaseAddress = 0x24000000;
    MPU_InitStruct.Size = MPU_REGION_SIZE_512KB;
    MPU_InitStruct.AccessPermission = MPU_REGION_FULL_ACCESS;
    MPU_InitStruct.IsBufferable = MPU_ACCESS_NOT_BUFFERABLE;
    MPU_InitStruct.IsCacheable = MPU_ACCESS_CACHEABLE;
    MPU_InitStruct.IsShareable = MPU_ACCESS_NOT_SHAREABLE;
    MPU_InitStruct.Number = MPU_REGION_NUMBER2;
    MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL0;
    MPU_InitStruct.SubRegionDisable = 0x00;
    MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_ENABLE;
    HAL_MPU_ConfigRegion(&MPU_InitStruct);

    /*
     * Region 3：FMC 总线扩展 IO（0x60000000 起，64KB）
     * -----------------------------------------------------------------------
     * 74HC574 等扩展芯片映射在 FMC 地址空间（实际访问地址如 0x68200000
     * 落在此 64KB 覆盖范围内）。这是“内存映射 IO”，每次写操作必须直接
     * 到达芯片，绝不能 Cache：
     *   - 若 Cacheable，写操作可能被合并或延迟写回
     *   - 会出现 2 次片选 CS / 写使能 WE，导致 IO 锁存器状态错乱
     */
    MPU_InitStruct.Enable = MPU_REGION_ENABLE;
    MPU_InitStruct.BaseAddress = 0x60000000;
    MPU_InitStruct.Size = MPU_REGION_SIZE_64KB;
    MPU_InitStruct.AccessPermission = MPU_REGION_FULL_ACCESS;
    MPU_InitStruct.IsBufferable = MPU_ACCESS_NOT_BUFFERABLE;
    MPU_InitStruct.IsCacheable = MPU_ACCESS_NOT_CACHEABLE;
    MPU_InitStruct.IsShareable = MPU_ACCESS_NOT_SHAREABLE;
    MPU_InitStruct.Number = MPU_REGION_NUMBER3;
    MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL0;
    MPU_InitStruct.SubRegionDisable = 0x00;
    MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_ENABLE;
    HAL_MPU_ConfigRegion(&MPU_InitStruct);

    /*
     * Region 4：外部 SDRAM 全片（EXT_SDRAM_ADDR = 0xC0000000，32MB）
     * -----------------------------------------------------------------------
     * 覆盖整片 SDRAM，包含两部分（地址见 bsp_fmc_sdram.h）：
     *   显存区：0xC0000000 起 4MB —— LTDC 帧缓冲（Layer0 / Layer1 各 2MB）
     *   应用区：0xC0400000 起 28MB —— 堆、大数据缓冲、LVGL 缓冲等
     *
     * 设为 Normal + Write-Through + Cacheable：
     *   1) CPU 写入立即同步到 SDRAM，LTDC 刷新时能读到最新像素（显存一致性）；
     *   2) 应用区作为 Normal 内存，允许非对齐访问、可缓存，读写性能高
     *      （若不划区，默认是 Device 内存：非对齐访问会 fault 且不可缓存）。
     * DisableExec = DISABLE：SDRAM 仅存数据，禁止在此取指执行。
     *
     * 注意：若后续启用 DMA2D 回读显存或外设直接改写 SDRAM，CPU 侧读取前需
     *       SCB_InvalidateDCache_by_Addr() 使对应缓存行失效。
     */
    MPU_InitStruct.Enable = MPU_REGION_ENABLE;
    MPU_InitStruct.BaseAddress = EXT_SDRAM_ADDR;
    MPU_InitStruct.Size = MPU_REGION_SIZE_32MB;
    MPU_InitStruct.AccessPermission = MPU_REGION_FULL_ACCESS;
    MPU_InitStruct.IsBufferable = MPU_ACCESS_NOT_BUFFERABLE;
    MPU_InitStruct.IsCacheable = MPU_ACCESS_CACHEABLE;
    MPU_InitStruct.IsShareable = MPU_ACCESS_NOT_SHAREABLE;
    MPU_InitStruct.Number = MPU_REGION_NUMBER4;
    MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL0;
    MPU_InitStruct.SubRegionDisable = 0x00;
    MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_DISABLE;
    HAL_MPU_ConfigRegion(&MPU_InitStruct);

    /*
     * Region 5：FMC NAND Flash（0x80000000，512MB）
     * -----------------------------------------------------------------------
     * 板上 NAND 挂在 FMC NAND 控制器，整片地址空间映射在此。
     * NAND 有命令/地址/数据多阶段访问时序，必须不可缓存，否则 CPU 读到的
     * 可能是 Cache 中的旧缓存行，而非芯片当前输出数据。
     */
    MPU_InitStruct.Enable = MPU_REGION_ENABLE;
    MPU_InitStruct.BaseAddress = 0x80000000;
    MPU_InitStruct.Size = MPU_REGION_SIZE_512MB;
    MPU_InitStruct.AccessPermission = MPU_REGION_FULL_ACCESS;
    MPU_InitStruct.IsBufferable = MPU_ACCESS_NOT_BUFFERABLE;
    MPU_InitStruct.IsCacheable = MPU_ACCESS_NOT_CACHEABLE;
    MPU_InitStruct.IsShareable = MPU_ACCESS_NOT_SHAREABLE;
    MPU_InitStruct.Number = MPU_REGION_NUMBER5;
    MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL0;
    MPU_InitStruct.SubRegionDisable = 0x00;
    MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_ENABLE;
    HAL_MPU_ConfigRegion(&MPU_InitStruct);

    /*
     * 使能 MPU，并打开“特权模式默认背景映射”：
     * 未被上述 Region 覆盖的地址仍按芯片默认内存属性访问（例如 SDRAM 应用区
     * 0xC0400000 之后的 28MB）。若完全关闭默认映射，未配置区域访问会 fault。
     */
    HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);
}

/**
 * @brief  使能 Cortex-M7 L1 I-Cache 与 D-Cache
 * @note   调用顺序（在 System_Init 中）必须是：
 *           1. MPU_Config()      — 先划定各段内存的 Cache 策略
 *           2. CPU_CACHE_Enable() — 再打开 Cache，之后所有读写才按 MPU 规则走
 *         若顺序颠倒，在 MPU 生效前就可能产生错误的 Cache 行，导致难以排查的故障。
 * @retval 无
 */
static void CPU_CACHE_Enable(void)
{
    /* I-Cache：加速 CPU 取指，与 MPU Region 的 DisableExec 配合可保护数据区 */
    SCB_EnableICache();

    /* D-Cache：加速数据读写；开启后外设共享内存必须依赖 MPU 或手动 Cache 维护 */
    SCB_EnableDCache();
}

/**
 * @brief  每 10ms 由 SysTick 调用一次，可放置按键扫描、蜂鸣器控制等低实时性任务
 * @retval 无
 */
void bsp_RunPer10ms(void)
{
    // bsp_KeyScan10ms();
}

/**
 * @brief  每 1ms 由 SysTick 调用一次，可放置触摸坐标扫描等周期性任务
 * @retval 无
 */
void bsp_RunPer1ms(void)
{
}

/**
 * @brief  主循环空闲钩子，可添加喂狗或 CPU 休眠操作
 * @retval 无
 */
void bsp_Idle(void)
{
    /* --- 喂狗 */

    /* --- 让CPU进入休眠，由Systick定时中断唤醒或者其他中断唤醒 */

    /* 例如 emWin 图形库，可以插入图形库需要的轮询函数 */
    // GUI_Exec();

    /* 例如 uIP 协议，可以插入uip轮询函数 */
    // TOUCH_CapScan();
}

/**
 * @brief  重定向 HAL 毫秒延迟，避免在高优先级中断内使用 SysTick 阻塞延迟死锁（仅 RTX 模式启用）
 * @retval 无
 */
/* 当前例子使用stm32h7xx_hal.c默认方式实现，未使用下面重定向的函数 */
#if USE_RTX == 1
void HAL_Delay(uint32_t Delay)
{
    delay_us(Delay * 1000);
}

HAL_StatusTypeDef HAL_InitTick(uint32_t TickPriority)
{
    return HAL_OK;
}

uint32_t HAL_GetTick(void)
{
    static uint32_t ticks = 0U;
    uint32_t i;

    if (osKernelGetState() == osKernelRunning)
    {
        return ((uint32_t)osKernelGetTickCount());
    }

#if 0
    /* 如果RTX5还没有运行，采用下面方式 */
    for (i = (SystemCoreClock >> 14U); i > 0U; i--)
    {
        __NOP(); __NOP(); __NOP(); __NOP(); __NOP(); __NOP();
        __NOP(); __NOP(); __NOP(); __NOP(); __NOP(); __NOP();
    }
    return ++ticks;
#endif
    return get_system_ms();
}
#else
/**
 * @brief This function handles System tick timer.
 */
void SysTick_Handler(void)
{
    HAL_IncTick();
}
#endif

/* end of file */
