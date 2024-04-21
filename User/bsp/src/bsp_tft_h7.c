/*
*********************************************************************************************************
*
*    模块名称 : STM32H7内部LCD驱动程序
*    文件名称 : bsp_tft_h7.c
*    版    本 : V1.0
*    说    明 : STM32F429 内部LCD接口的硬件配置程序。
*    修改记录 :
*        版本号  日期       作者    说明
*        V1.0    2014-05-05 armfly 增加 STM32F429 内部LCD接口； 基于ST的例子更改，不要背景层和前景层定义，直接
*                                  用 LTDC_Layer1 、 LTDC_Layer2, 这是2个结构体指针
*        V1.1    2015-11-19 armfly
*                        1. 绘图函数替换为DMA2D硬件驱动，提高绘图效率
*                        2. 统一多种面板的配置函数，自动识别面板类型
*
*    Copyright (C), 2015-2020, 安富莱电子 www.armfly.com
*
*   LCD_TFT 同步时序配置（整理自官方做的一个截图，言简意赅）：
*   ----------------------------------------------------------------------------
*
*                                                 Total Width
*                             <--------------------------------------------------->
*                       Hsync width HBP             Active Width                HFP
*                             <---><--><--------------------------------------><-->
*                         ____    ____|_______________________________________|____
*                             |___|   |                                       |    |
*                                     |                                       |    |
*                         __|         |                                       |    |
*            /|\    /|\  |            |                                       |    |
*             | VSYNC|   |            |                                       |    |
*             |Width\|/  |__          |                                       |    |
*             |     /|\     |         |                                       |    |
*             |  VBP |      |         |                                       |    |
*             |     \|/_____|_________|_______________________________________|    |
*             |     /|\     |         | / / / / / / / / / / / / / / / / / / / |    |
*             |      |      |         |/ / / / / / / / / / / / / / / / / / / /|    |
*    Total    |      |      |         |/ / / / / / / / / / / / / / / / / / / /|    |
*    Heigh    |      |      |         |/ / / / / / / / / / / / / / / / / / / /|    |
*             |Active|      |         |/ / / / / / / / / / / / / / / / / / / /|    |
*             |Heigh |      |         |/ / / / / / Active Display Area / / / /|    |
*             |      |      |         |/ / / / / / / / / / / / / / / / / / / /|    |
*             |      |      |         |/ / / / / / / / / / / / / / / / / / / /|    |
*             |      |      |         |/ / / / / / / / / / / / / / / / / / / /|    |
*             |      |      |         |/ / / / / / / / / / / / / / / / / / / /|    |
*             |      |      |         |/ / / / / / / / / / / / / / / / / / / /|    |
*             |     \|/_____|_________|_______________________________________|    |
*             |     /|\     |                                                      |
*             |  VFP |      |                                                      |
*            \|/    \|/_____|______________________________________________________|
*
*
*     每个LCD设备都有自己的同步时序值：
*     Horizontal Synchronization (Hsync)
*     Horizontal Back Porch (HBP)
*     Active Width
*     Horizontal Front Porch (HFP)
*
*     Vertical Synchronization (Vsync)
*     Vertical Back Porch (VBP)
*     Active Heigh
*     Vertical Front Porch (VFP)
*
*     LCD_TFT 窗口水平和垂直的起始以及结束位置 :
*     ----------------------------------------------------------------
*
*     HorizontalStart = (Offset_X + Hsync + HBP);
*     HorizontalStop  = (Offset_X + Hsync + HBP + Window_Width - 1);
*     VarticalStart   = (Offset_Y + Vsync + VBP);
*     VerticalStop    = (Offset_Y + Vsync + VBP + Window_Heigh - 1);
*********************************************************************************************************
*/

/* 包含头文件 ----------------------------------------------------------------*/
#include "bsp.h"
#include "bsp_tft_h7.h"
#include "bsp_fmc_sdram.h"

/* 私有类型定义 --------------------------------------------------------------*/

/* 私有宏定义 ----------------------------------------------------------------*/
#undef THIS
#define THIS (lcd_cfg_list[lcd_type])

/* 偏移地址计算公式:
   Maximum width x Maximum Length x Maximum Pixel size (RGB565)，单位字节
   => 800 x 480 x 2 =  768000 */
#define BUFFER_OFFSET SDRAM_LCD_SIZE // (uint32_t)(g_LcdHeight * g_LcdWidth * 2)

#define LCD_FRAME_BUFFER SDRAM_LCD_BUF1

/* 私有变量 ------------------------------------------------------------------*/
static LTDC_HandleTypeDef hltdc = {0};
static uint8_t lcd_type = 0;
const tft_cfg_t lcd_cfg_list[] = {
    {"LCD7.0 1024X600 48MHz", 1024, 600, 20, 3, 140, 20, 160, 12}, // PLL3 M5 N192 P*2 Q20 R20 =48M
    {"LCD4.3 480X272 10MHz", 480, 272, 1, 1, 40, 8, 5, 8},         // PLL3 M5 N192 P*2 Q20 R96 =10M
    {"LCD7.0 800X480 20MHz", 800, 480, 1, 1, 46, 23, 210, 22},     // PLL3 M5 N192 P*2 Q20 R48 =20M
    {"LCD7.0 800X480 30MHz", 800, 480, 88, 40, 48, 32, 13, 3},     // PLL3 M5 N192 P*2 Q20 R32 =30M
    {"LCD10.0 1280X800 48MHz", 1280, 800, 140, 10, 10, 10, 10, 3}  // PLL3 M5 N192 P*2 Q20 R20 =48M

};

/* 扩展变量 ------------------------------------------------------------------*/

/* 私有函数原形 --------------------------------------------------------------*/
static void MX_LTDC_Init(void);

/* 函数体 --------------------------------------------------------------------*/

/**
 * @brief LTDC MSP Initialization
 * This function configures the hardware resources used in this example
 * @param hltdc: LTDC handle pointer
 * @retval None
 */
void HAL_LTDC_MspInit(LTDC_HandleTypeDef *hltdc)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};
    if (hltdc->Instance == LTDC)
    {
        /* USER CODE BEGIN LTDC_MspInit 0 */

        /* USER CODE END LTDC_MspInit 0 */

        /** Initializes the peripherals clock
         */
        PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_LTDC;
        PeriphClkInitStruct.PLL3.PLL3M = 5;
        PeriphClkInitStruct.PLL3.PLL3N = 192;
        PeriphClkInitStruct.PLL3.PLL3P = 2;
        PeriphClkInitStruct.PLL3.PLL3Q = 20;
        PeriphClkInitStruct.PLL3.PLL3R = 20;
        PeriphClkInitStruct.PLL3.PLL3RGE = RCC_PLL3VCIRANGE_2;
        PeriphClkInitStruct.PLL3.PLL3VCOSEL = RCC_PLL3VCOWIDE;
        PeriphClkInitStruct.PLL3.PLL3FRACN = 0;
        if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
        {
            ERROR_HANDLER();
        }

        /* Peripheral clock enable */
        __HAL_RCC_LTDC_CLK_ENABLE();

        __HAL_RCC_GPIOK_CLK_ENABLE();
        __HAL_RCC_GPIOJ_CLK_ENABLE();
        __HAL_RCC_GPIOI_CLK_ENABLE();
        /**LTDC GPIO Configuration
        PK5     ------> LTDC_B6
        PK4     ------> LTDC_B5
        PJ15     ------> LTDC_B3
        PK6     ------> LTDC_B7
        PK3     ------> LTDC_B4
        PK7     ------> LTDC_DE
        PJ14     ------> LTDC_B2
        PJ12     ------> LTDC_B0
        PJ13     ------> LTDC_B1
        PI12     ------> LTDC_HSYNC
        PI13     ------> LTDC_VSYNC
        PI14     ------> LTDC_CLK
        PK2     ------> LTDC_G7
        PK0     ------> LTDC_G5
        PK1     ------> LTDC_G6
        PJ11     ------> LTDC_G4
        PJ10     ------> LTDC_G3
        PJ9     ------> LTDC_G2
        PJ0     ------> LTDC_R1
        PJ8     ------> LTDC_G1
        PJ7     ------> LTDC_G0
        PJ6     ------> LTDC_R7
        PI15     ------> LTDC_R0
        PJ1     ------> LTDC_R2
        PJ5     ------> LTDC_R6
        PJ2     ------> LTDC_R3
        PJ3     ------> LTDC_R4
        PJ4     ------> LTDC_R5
        */
        GPIO_InitStruct.Pin = GPIO_PIN_5 | GPIO_PIN_4 | GPIO_PIN_6 | GPIO_PIN_3 | GPIO_PIN_7 | GPIO_PIN_2 | GPIO_PIN_0 | GPIO_PIN_1;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        GPIO_InitStruct.Alternate = GPIO_AF14_LTDC;
        HAL_GPIO_Init(GPIOK, &GPIO_InitStruct);

        GPIO_InitStruct.Pin = GPIO_PIN_15 | GPIO_PIN_14 | GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_11 | GPIO_PIN_10 | GPIO_PIN_9 | GPIO_PIN_0 | GPIO_PIN_8 | GPIO_PIN_7 | GPIO_PIN_6 | GPIO_PIN_1 | GPIO_PIN_5 | GPIO_PIN_2 | GPIO_PIN_3 | GPIO_PIN_4;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        GPIO_InitStruct.Alternate = GPIO_AF14_LTDC;
        HAL_GPIO_Init(GPIOJ, &GPIO_InitStruct);

        GPIO_InitStruct.Pin = GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        GPIO_InitStruct.Alternate = GPIO_AF14_LTDC;
        HAL_GPIO_Init(GPIOI, &GPIO_InitStruct);

        /* USER CODE BEGIN LTDC_MspInit 1 */

        /* USER CODE END LTDC_MspInit 1 */
    }
}

/**
 * @brief LTDC MSP De-Initialization
 * This function freeze the hardware resources used in this example
 * @param hltdc: LTDC handle pointer
 * @retval None
 */
void HAL_LTDC_MspDeInit(LTDC_HandleTypeDef *hltdc)
{
    if (hltdc->Instance == LTDC)
    {
        /* USER CODE BEGIN LTDC_MspDeInit 0 */

        /* USER CODE END LTDC_MspDeInit 0 */
        /* Peripheral clock disable */
        __HAL_RCC_LTDC_CLK_DISABLE();

        /**LTDC GPIO Configuration
        PK5     ------> LTDC_B6
        PK4     ------> LTDC_B5
        PJ15     ------> LTDC_B3
        PK6     ------> LTDC_B7
        PK3     ------> LTDC_B4
        PK7     ------> LTDC_DE
        PJ14     ------> LTDC_B2
        PJ12     ------> LTDC_B0
        PJ13     ------> LTDC_B1
        PI12     ------> LTDC_HSYNC
        PI13     ------> LTDC_VSYNC
        PI14     ------> LTDC_CLK
        PK2     ------> LTDC_G7
        PK0     ------> LTDC_G5
        PK1     ------> LTDC_G6
        PJ11     ------> LTDC_G4
        PJ10     ------> LTDC_G3
        PJ9     ------> LTDC_G2
        PJ0     ------> LTDC_R1
        PJ8     ------> LTDC_G1
        PJ7     ------> LTDC_G0
        PJ6     ------> LTDC_R7
        PI15     ------> LTDC_R0
        PJ1     ------> LTDC_R2
        PJ5     ------> LTDC_R6
        PJ2     ------> LTDC_R3
        PJ3     ------> LTDC_R4
        PJ4     ------> LTDC_R5
        */
        HAL_GPIO_DeInit(GPIOK, GPIO_PIN_5 | GPIO_PIN_4 | GPIO_PIN_6 | GPIO_PIN_3 | GPIO_PIN_7 | GPIO_PIN_2 | GPIO_PIN_0 | GPIO_PIN_1);

        HAL_GPIO_DeInit(GPIOJ, GPIO_PIN_15 | GPIO_PIN_14 | GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_11 | GPIO_PIN_10 | GPIO_PIN_9 | GPIO_PIN_0 | GPIO_PIN_8 | GPIO_PIN_7 | GPIO_PIN_6 | GPIO_PIN_1 | GPIO_PIN_5 | GPIO_PIN_2 | GPIO_PIN_3 | GPIO_PIN_4);

        HAL_GPIO_DeInit(GPIOI, GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15);

        /* USER CODE BEGIN LTDC_MspDeInit 1 */

        /* USER CODE END LTDC_MspDeInit 1 */
    }
}

/**
 * @brief LTDC Initialization Function
 * @param None
 * @retval None
 */
static void MX_LTDC_Init(void)
{

    /* USER CODE BEGIN LTDC_Init 0 */

    /* USER CODE END LTDC_Init 0 */

    LTDC_LayerCfgTypeDef pLayerCfg = {0};
    LTDC_LayerCfgTypeDef pLayerCfg1 = {0};

    /* USER CODE BEGIN LTDC_Init 1 */

    /* USER CODE END LTDC_Init 1 */
    hltdc.Instance = LTDC;
    hltdc.Init.HSPolarity = LTDC_HSPOLARITY_AL;  // 水平同步极性
    hltdc.Init.VSPolarity = LTDC_VSPOLARITY_AL;  // 垂直同步极性
    hltdc.Init.DEPolarity = LTDC_DEPOLARITY_AL;  // 数据使能极性
    hltdc.Init.PCPolarity = LTDC_PCPOLARITY_IPC; // 像素时钟极性

    hltdc.Init.HorizontalSync = THIS.hsw - 1;                                  // 19;   水平同步宽度
    hltdc.Init.VerticalSync = THIS.vsw - 1;                                    // 2;    垂直同步宽度
    hltdc.Init.AccumulatedHBP = THIS.hsw + THIS.hbp - 1;                       // 159;  水平同步后沿宽度
    hltdc.Init.AccumulatedVBP = THIS.vsw + THIS.vbp - 1;                       // 22;   垂直同步后沿高度
    hltdc.Init.AccumulatedActiveW = THIS.hsw + THIS.hbp + THIS.pwidth - 1;     // 1183; 有效宽度
    hltdc.Init.AccumulatedActiveH = THIS.vsw + THIS.vbp + THIS.pheight - 1;    // 622;  有效高度
    hltdc.Init.TotalWidth = THIS.hsw + THIS.hbp + THIS.pwidth + THIS.hfp - 1;  // 1343; 总宽度
    hltdc.Init.TotalHeigh = THIS.vsw + THIS.vbp + THIS.pheight + THIS.vfp - 1; // 634;  总高度

    hltdc.Init.Backcolor.Red = 255;   // 屏幕背景层红色部分
    hltdc.Init.Backcolor.Green = 255; // 屏幕背景层绿色部分
    hltdc.Init.Backcolor.Blue = 255;  // 屏幕背景色蓝色部分
    if (HAL_LTDC_Init(&hltdc) != HAL_OK)
    {
        ERROR_HANDLER();
    }

    pLayerCfg.WindowX0 = 0;
    pLayerCfg.WindowX1 = 300;
    pLayerCfg.WindowY0 = 0;
    pLayerCfg.WindowY1 = 200;
    pLayerCfg.PixelFormat = LTDC_PIXEL_FORMAT_RGB565;
    pLayerCfg.Alpha = 255;
    pLayerCfg.Alpha0 = 0;
    pLayerCfg.BlendingFactor1 = LTDC_BLENDING_FACTOR1_PAxCA;
    pLayerCfg.BlendingFactor2 = LTDC_BLENDING_FACTOR2_PAxCA;
    pLayerCfg.FBStartAdress = SDRAM_LCD_BUF1;
    pLayerCfg.ImageWidth = 1024;
    pLayerCfg.ImageHeight = 600;
    pLayerCfg.Backcolor.Blue = 0;
    pLayerCfg.Backcolor.Green = 0;
    pLayerCfg.Backcolor.Red = 0;
    if (HAL_LTDC_ConfigLayer(&hltdc, &pLayerCfg, 0) != HAL_OK)
    {
        ERROR_HANDLER();
    }

    pLayerCfg1.WindowX0 = 200;
    pLayerCfg1.WindowX1 = 600;
    pLayerCfg1.WindowY0 = 200;
    pLayerCfg1.WindowY1 = 600;
    pLayerCfg1.PixelFormat = LTDC_PIXEL_FORMAT_RGB565;
    pLayerCfg1.Alpha = 255;
    pLayerCfg1.Alpha0 = 0;
    pLayerCfg1.BlendingFactor1 = LTDC_BLENDING_FACTOR1_PAxCA;
    pLayerCfg1.BlendingFactor2 = LTDC_BLENDING_FACTOR2_PAxCA;
    pLayerCfg1.FBStartAdress = SDRAM_LCD_BUF2;
    pLayerCfg1.ImageWidth = 1024;
    pLayerCfg1.ImageHeight = 600;
    pLayerCfg1.Backcolor.Blue = 0;
    pLayerCfg1.Backcolor.Green = 0;
    pLayerCfg1.Backcolor.Red = 0;
    if (HAL_LTDC_ConfigLayer(&hltdc, &pLayerCfg1, 1) != HAL_OK)
    {
        ERROR_HANDLER();
    }

    /* USER CODE BEGIN LTDC_Init 2 */

    /* USER CODE END LTDC_Init 2 */
}

/*
*********************************************************************************************************
*    函 数 名: bsp_ltdc_clk
*    功能说明: 设置 LTDC 时钟
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void bsp_ltdc_clk(uint16_t _clk)
{
    /* LCD 时钟配置 */
    /* PLL3_VCO Input       = HSE_VALUE / PLL3M         = 25MHz / 5 = 5MHz */
    /* PLL3_VCO Output      = PLL3_VCO Input * PLL3N    = 5MHz * 192 = 960MHz */
    /* PLLLCDCLK            = PLL3_VCO Output / PLL3R   = 960MHz / 20 = 48MHz */
    /* LTDC clock frequency = PLLLCDCLK                 = 48MHz */
    /* 当前这个配置方便用户使用 PLL3Q 输出的 48MHz 时钟供 USB使用 */
    RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};
    /* Initializes the peripherals clock */
    PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_LTDC;
    PeriphClkInitStruct.PLL3.PLL3M = 5;
    PeriphClkInitStruct.PLL3.PLL3N = 192;
    PeriphClkInitStruct.PLL3.PLL3P = 2;
    PeriphClkInitStruct.PLL3.PLL3Q = 20;
    PeriphClkInitStruct.PLL3.PLL3R = 20;
    PeriphClkInitStruct.PLL3.PLL3RGE = RCC_PLL3VCIRANGE_2;
    PeriphClkInitStruct.PLL3.PLL3VCOSEL = RCC_PLL3VCOWIDE;
    PeriphClkInitStruct.PLL3.PLL3FRACN = 0;
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
    {
        ERROR_HANDLER();
    }
}

/*
*********************************************************************************************************
*    函 数 名: bsp_InitTFT
*    功能说明: 初始化LCD
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void bsp_InitTFT(void)
{
    MX_LTDC_Init();
    bsp_SetTIMOutPWM(GPIOA, GPIO_PIN_8, TIM1, 1, 20000, (50 * 10000) / 255);
}

/*
*********************************************************************************************************
*    函 数 名: TFT_GetDescribe
*    功能说明: 读取LCD的描述符号，用于显示
*    形    参: 无
*    返 回 值: char* 描述符字符串指针
*********************************************************************************************************
*/
char *TFT_GetDescribe(void)
{
    return THIS.name;
}

/*
*********************************************************************************************************
*    函 数 名: TFT_DispOn
*    功能说明: 打开显示
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void TFT_DispOn(void)
{
}

/*
*********************************************************************************************************
*    函 数 名: TFT_DispOff
*    功能说明: 关闭显示
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
void TFT_DispOff(void)
{
}

#if defined(__SHELL_H__) && defined(DEBUG_MODE)
static int _cmd(int argc, char *argv[])
{
    const char *help_info[] = {"set init/deinit",
                               "test 1/2"};
    if (argc < 2)
    {
        printf("Error:Missing command parameters.\r\nUsage:\r\n");
        for (uint32_t i = 0; i < sizeof(help_info) / sizeof(char *); i++)
        {
            printf("%s ", argv[0]);
            printf("%s\r\n", help_info[i]);
        }
        printf("\r\n");
        return -1;
    }

    if (strcmp(argv[1], "set") == 0)
    {
        if (argc < 3)
        {
            printf("Missing 'set' command parameters.\r\n");
            printf("%s ", argv[0]);
            printf("%s\r\n", help_info[0]);
            return -1;
        }

        if (strcmp(argv[2], "init") == 0)
        {
            bsp_InitTFT();
            return 0;
        }
        else if (strcmp(argv[2], "deinit") == 0)
        {
            if (hltdc.State == HAL_LTDC_STATE_RESET)
            {
                printf("Error:Do not repeatedly deinit LTDC.\r\n");
                return -1;
            }

            return HAL_LTDC_DeInit(&hltdc);
        }
        else
        {
            printf("Invalid 'set' command parameter.\r\n");
            printf("%s ", argv[0]);
            printf("%s\r\n", help_info[0]);
            return -1;
        }
    }
    else if (strcmp(argv[1], "test") == 0)
    {
#if 0
        if (argc < 3)
        {
            printf("Missing 'test' command parameters.\r\n");
            printf("%s ", argv[0]);
            printf("%s\r\n", help_info[1]);
            return -1;
        }
#endif
        printf("Error:Unrealized function.\r\n");
        return 0;
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
        return -1;
    }

    return 0;
}

// 导出到命令列表里
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), tft, _cmd, tft[set test read write]);
#endif // #ifdef DEBUG_MODE

/***************************** 安富莱电子 www.armfly.com (END OF FILE) *********************************/
