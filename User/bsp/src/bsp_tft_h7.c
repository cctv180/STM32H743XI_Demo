/**
 ******************************************************************************
 * @file    bsp_tft_h7.c
 * @brief   STM32H7 LTDC TFT 驱动实现
 *
 * @details
 *   - 使用 STM32 HAL LTDC 驱动 RGB 并行 TFT。
 *   - 显存位于外部 SDRAM (BANK1)，由 @c bsp_fmc_sdram 模块在启动时配置；
 *     默认 Layer 0 = TFT_LAYER0_FB_ADDR (SDRAM_LCD_BUF1)，
 *          Layer 1 = TFT_LAYER1_FB_ADDR (SDRAM_LCD_BUF2)。
 *     framebuffer 地址在 bsp_tft_h7.h 中以宏形式集中暴露，方便覆盖。
 *   - 背光使用 TIM1_CH1 / PA8 输出 PWM，由 @c bsp_tim_pwm 模块封装。
 *   - 通过 @ref bsp_SelectTFT 在初始化前选择面板，运行期支持
 *     @ref bsp_DeInitTFT / @ref bsp_InitTFT 切换面板。
 *
 * @section msp_compat 与 CubeMX 的兼容性
 *   本文件中的 @c HAL_LTDC_MspInit / @c HAL_LTDC_MspDeInit 在结构上、PLL3
 *   时钟参数与 GPIO 引脚分组上 ，与 CubeMX 生成的
 *   @c Src/stm32h7xx_hal_msp.c 完全对齐。这样设计的目的：
 *   - @c Src/stm32h7xx_hal_msp.c 在 .uvprojx 中被设置为
 *     @c IncludeInBuild=0 ，**始终不参与构建**，仅作为 CubeMX 重新生成后
 *     与 BSP 进行人工 diff / 移植的参考源；
 *   - 当 CubeMX 重新生成 MSP 后，开发者只需把 @c HAL_LTDC_MspInit /
 *     @c HAL_LTDC_MspDeInit 的函数体直接复制到本文件对应位置即可，无需
 *     重新设计 BSP 接口；
 *   - 整个工程中 @c HAL_LTDC_MspInit / @c HAL_LTDC_MspDeInit 只有本文件
 *     一份强符号，不会与 CubeMX 文件产生重复定义。
 *
 * @verbatim
 *   LCD_TFT 同步时序 (整理自 ST 官方文档):
 *
 *                                                Total Width
 *                            <--------------------------------------------------->
 *                      Hsync width HBP             Active Width                HFP
 *                            <---><--><--------------------------------------><-->
 *                        ____    ____|_______________________________________|____
 *                            |___|   |                                       |    |
 *                                    |                                       |    |
 *                            VBP     |          Active Display Area          |    |
 *                            VFP     |_______________________________________|    |
 *
 *   每个面板都有自己的同步时序值: HSW / HBP / Active W / HFP, VSW / VBP /
 *   Active H / VFP, 详见 lcd_cfg_list[]。
 *
 *   LTDC 寄存器层面的窗口起止 = 累加值, HAL 层使用 -1 表示半开区间。
 * @endverbatim
 *
 * @copyright (C) 2026, Project Contributors
 ******************************************************************************
 */

/* Includes ------------------------------------------------------------------*/
#include "bsp.h"
#include "bsp_tft_h7.h"
#include "bsp_fmc_sdram.h"
#include "bsp_tim_pwm.h"

/* Private constants ---------------------------------------------------------*/

/**
 * @brief 各面板时序表。
 *
 * @note 像素时钟由 PLL3 产生:
 *       PLL3VCO = HSE / PLL3M * PLL3N = 25 MHz / 5 * 192 = 960 MHz
 *       LCDCLK  = PLL3VCO / PLL3R
 *       当前 MspInit 固定使用 PLL3M=5/N=192/P=2/Q=20/R=20，对应 48 MHz 像素时钟。
 *       若需切换像素时钟，请同步修改 @ref HAL_LTDC_MspInit 中的 PLL3R。
 */
static const tft_cfg_t s_lcd_cfg_list[TFT_PANEL_NUM] = {
    [TFT_PANEL_LCD7_1024X600_48M] = {"LCD7.0 1024X600 48MHz", 1024, 600, 20, 3, 140, 20, 160, 12},
    [TFT_PANEL_LCD43_480X272_10M] = {"LCD4.3 480X272 10MHz", 480, 272, 1, 1, 40, 8, 5, 8},
    [TFT_PANEL_LCD7_800X480_20M] = {"LCD7.0 800X480 20MHz", 800, 480, 1, 1, 46, 23, 210, 22},
    [TFT_PANEL_LCD7_800X480_30M] = {"LCD7.0 800X480 30MHz", 800, 480, 88, 40, 48, 32, 13, 3},
    [TFT_PANEL_LCD10_1280X800_48M] = {"LCD10.0 1280X800 48MHz", 1280, 800, 140, 10, 10, 10, 10, 3},
};

/* Private state -------------------------------------------------------------*/

static struct
{
    LTDC_HandleTypeDef hltdc;
    tft_panel_id_t panel;
    uint8_t initialized;
    uint8_t backlight; /* 0..100 */
} s_tft = {
    .panel = TFT_PANEL_DEFAULT,
    .initialized = 0,
    .backlight = TFT_BACKLIGHT_DEFAULT,
};

#define TFT_CFG() (&s_lcd_cfg_list[s_tft.panel])

/* Private function prototypes -----------------------------------------------*/
static tft_status_t MX_LTDC_Init(void);
static void tft_layer_config(uint32_t layer_idx, uint32_t fb_addr, uint8_t alpha);

/* ===========================================================================
 * HAL MSP — kept structurally identical to CubeMX-generated
 * Src/stm32h7xx_hal_msp.c (PLL3 clock params + GPIO groups + AF), so that
 * when CubeMX is re-run later the regenerated function bodies can be
 * dropped in here verbatim. Src/stm32h7xx_hal_msp.c is excluded from the
 * build (IncludeInBuild=0), this file owns the only strong definition.
 * ===========================================================================*/

/**
 * @brief LTDC MSP Initialization
 * @param hltdc: LTDC handle pointer
 */
void HAL_LTDC_MspInit(LTDC_HandleTypeDef *hltdc)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};

    if (hltdc->Instance != LTDC)
    {
        return;
    }

    /* PLL3R -> LCD pixel clock = 48 MHz (PLL3Q=20 -> 48 MHz USB friendly) */
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

    __HAL_RCC_LTDC_CLK_ENABLE();
    __HAL_RCC_GPIOK_CLK_ENABLE();
    __HAL_RCC_GPIOJ_CLK_ENABLE();
    __HAL_RCC_GPIOI_CLK_ENABLE();

    /**LTDC GPIO Configuration
       PK0..7  -> LTDC_G5/G6/G7/B4/B5/B6/B7/DE
       PJ0..15 -> LTDC_R0..R7 / G0..G4 / B0..B3
       PI12..15-> LTDC_HSYNC / VSYNC / CLK / R0
    */
    GPIO_InitStruct.Pin = GPIO_PIN_5 | GPIO_PIN_4 | GPIO_PIN_6 | GPIO_PIN_3 |
                          GPIO_PIN_7 | GPIO_PIN_2 | GPIO_PIN_0 | GPIO_PIN_1;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF14_LTDC;
    HAL_GPIO_Init(GPIOK, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_15 | GPIO_PIN_14 | GPIO_PIN_12 | GPIO_PIN_13 |
                          GPIO_PIN_11 | GPIO_PIN_10 | GPIO_PIN_9 | GPIO_PIN_0 |
                          GPIO_PIN_8 | GPIO_PIN_7 | GPIO_PIN_6 | GPIO_PIN_1 |
                          GPIO_PIN_5 | GPIO_PIN_2 | GPIO_PIN_3 | GPIO_PIN_4;
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
}

/**
 * @brief LTDC MSP De-Initialization
 * @param hltdc: LTDC handle pointer
 */
void HAL_LTDC_MspDeInit(LTDC_HandleTypeDef *hltdc)
{
    if (hltdc->Instance != LTDC)
    {
        return;
    }

    __HAL_RCC_LTDC_CLK_DISABLE();

    HAL_GPIO_DeInit(GPIOK, GPIO_PIN_5 | GPIO_PIN_4 | GPIO_PIN_6 | GPIO_PIN_3 |
                               GPIO_PIN_7 | GPIO_PIN_2 | GPIO_PIN_0 | GPIO_PIN_1);

    HAL_GPIO_DeInit(GPIOJ, GPIO_PIN_15 | GPIO_PIN_14 | GPIO_PIN_12 | GPIO_PIN_13 |
                               GPIO_PIN_11 | GPIO_PIN_10 | GPIO_PIN_9 | GPIO_PIN_0 |
                               GPIO_PIN_8 | GPIO_PIN_7 | GPIO_PIN_6 | GPIO_PIN_1 |
                               GPIO_PIN_5 | GPIO_PIN_2 | GPIO_PIN_3 | GPIO_PIN_4);

    HAL_GPIO_DeInit(GPIOI, GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15);
}

/* ===========================================================================
 * Private helpers
 * ===========================================================================*/

static void tft_layer_config(uint32_t layer_idx, uint32_t fb_addr, uint8_t alpha)
{
    const tft_cfg_t *cfg = TFT_CFG();
    LTDC_LayerCfgTypeDef pLayerCfg = {0};

    pLayerCfg.WindowX0 = 0;
    pLayerCfg.WindowX1 = cfg->pwidth;
    pLayerCfg.WindowY0 = 0;
    pLayerCfg.WindowY1 = cfg->pheight;
    pLayerCfg.PixelFormat = TFT_PIXEL_FORMAT;
    pLayerCfg.Alpha = alpha;
    pLayerCfg.Alpha0 = 0;
    pLayerCfg.BlendingFactor1 = LTDC_BLENDING_FACTOR1_PAxCA;
    pLayerCfg.BlendingFactor2 = LTDC_BLENDING_FACTOR2_PAxCA;
    pLayerCfg.FBStartAdress = fb_addr;
    pLayerCfg.ImageWidth = cfg->pwidth;
    pLayerCfg.ImageHeight = cfg->pheight;
    pLayerCfg.Backcolor.Red = 0;
    pLayerCfg.Backcolor.Green = 0;
    pLayerCfg.Backcolor.Blue = 0;

    if (HAL_LTDC_ConfigLayer(&s_tft.hltdc, &pLayerCfg, layer_idx) != HAL_OK)
    {
        ERROR_HANDLER();
    }
}

/**
 * @brief LTDC 控制器初始化 (与 CubeMX MX_LTDC_Init 等价的可重入版本)。
 */
static tft_status_t MX_LTDC_Init(void)
{
    const tft_cfg_t *cfg = TFT_CFG();

    /* pwidth==0 表示该面板槽位未启用 */
    if (cfg->pwidth == 0 || cfg->pheight == 0)
    {
        return TFT_ERR;
    }

    s_tft.hltdc.Instance = LTDC;
    s_tft.hltdc.Init.HSPolarity = LTDC_HSPOLARITY_AL;
    s_tft.hltdc.Init.VSPolarity = LTDC_VSPOLARITY_AL;
    s_tft.hltdc.Init.DEPolarity = LTDC_DEPOLARITY_AL;
    s_tft.hltdc.Init.PCPolarity = LTDC_PCPOLARITY_IPC;

    s_tft.hltdc.Init.HorizontalSync = cfg->hsw - 1;
    s_tft.hltdc.Init.VerticalSync = cfg->vsw - 1;
    s_tft.hltdc.Init.AccumulatedHBP = cfg->hsw + cfg->hbp - 1;
    s_tft.hltdc.Init.AccumulatedVBP = cfg->vsw + cfg->vbp - 1;
    s_tft.hltdc.Init.AccumulatedActiveW = cfg->hsw + cfg->hbp + cfg->pwidth - 1;
    s_tft.hltdc.Init.AccumulatedActiveH = cfg->vsw + cfg->vbp + cfg->pheight - 1;
    s_tft.hltdc.Init.TotalWidth = cfg->hsw + cfg->hbp + cfg->pwidth + cfg->hfp - 1;
    s_tft.hltdc.Init.TotalHeigh = cfg->vsw + cfg->vbp + cfg->pheight + cfg->vfp - 1;

    s_tft.hltdc.Init.Backcolor.Red = 255;
    s_tft.hltdc.Init.Backcolor.Green = 255;
    s_tft.hltdc.Init.Backcolor.Blue = 255;

    if (HAL_LTDC_Init(&s_tft.hltdc) != HAL_OK)
    {
        ERROR_HANDLER();
        return TFT_ERR;
    }

    /* Layer 0 (background) - 全屏 framebuffer，不透明 */
    tft_layer_config(0U, TFT_LAYER0_FB_ADDR, 255U);

#if (TFT_LAYER_COUNT >= 2)
    /* Layer 1 (foreground) - 透明 overlay */
    tft_layer_config(1U, TFT_LAYER1_FB_ADDR, 0U);
#endif

    return TFT_OK;
}

/* ===========================================================================
 * Public API
 * ===========================================================================*/

void bsp_InitTFT(void)
{
    if (s_tft.initialized)
    {
        return;
    }

    if (MX_LTDC_Init() != TFT_OK)
    {
        return;
    }

    s_tft.initialized = 1U;

    TFT_SetBacklight(s_tft.backlight);
}

tft_status_t bsp_DeInitTFT(void)
{
    if (!s_tft.initialized)
    {
        return TFT_ERR;
    }

    /* 只关闭背光硬件，不修改 s_tft.backlight，便于 init 后恢复原亮度 */
    bsp_SetTIMOutPWM(TFT_BACKLIGHT_GPIO_PORT, TFT_BACKLIGHT_GPIO_PIN,
                     TFT_BACKLIGHT_TIM, (uint8_t)TFT_BACKLIGHT_CHANNEL,
                     (uint32_t)TFT_BACKLIGHT_FREQ, 0U);

    if (HAL_LTDC_DeInit(&s_tft.hltdc) != HAL_OK)
    {
        return TFT_ERR;
    }

    s_tft.initialized = 0U;
    return TFT_OK;
}

tft_status_t bsp_SelectTFT(tft_panel_id_t id)
{
    if ((uint32_t)id >= (uint32_t)TFT_PANEL_NUM)
    {
        return TFT_ERR;
    }
    /* 已初始化时禁止切换；调用者应先 DeInit */
    if (s_tft.initialized)
    {
        return TFT_ERR;
    }
    s_tft.panel = id;
    return TFT_OK;
}

tft_panel_id_t bsp_GetTFTPanelId(void)
{
    return s_tft.panel;
}

LTDC_HandleTypeDef *TFT_GetLtdcHandle(void)
{
    return &s_tft.hltdc;
}

const tft_cfg_t *TFT_GetCfg(void)
{
    return TFT_CFG();
}

const char *TFT_GetDescribe(void)
{
    return TFT_CFG()->name;
}

uint16_t TFT_GetWidth(void)
{
    return TFT_CFG()->pwidth;
}

uint16_t TFT_GetHeight(void)
{
    return TFT_CFG()->pheight;
}

uint32_t TFT_GetFrameBuffer(tft_layer_id_t layer)
{
    switch (layer)
    {
    case TFT_LAYER_BG:
        return (uint32_t)TFT_LAYER0_FB_ADDR;
    case TFT_LAYER_FG:
        return (uint32_t)TFT_LAYER1_FB_ADDR;
    default:
        return 0U;
    }
}

void TFT_DispOn(void)
{
    if (!s_tft.initialized)
    {
        return;
    }
    __HAL_LTDC_ENABLE(&s_tft.hltdc);
    /* 若 backlight 当前为 0，恢复到默认亮度 */
    TFT_SetBacklight(s_tft.backlight ? s_tft.backlight : (uint8_t)TFT_BACKLIGHT_DEFAULT);
}

void TFT_DispOff(void)
{
    if (!s_tft.initialized)
    {
        return;
    }
    /* 直接关 PWM 输出，不改 s_tft.backlight，便于 On 时恢复关屏前亮度 */
    bsp_SetTIMOutPWM(TFT_BACKLIGHT_GPIO_PORT, TFT_BACKLIGHT_GPIO_PIN,
                     TFT_BACKLIGHT_TIM, (uint8_t)TFT_BACKLIGHT_CHANNEL,
                     (uint32_t)TFT_BACKLIGHT_FREQ, 0U);
    __HAL_LTDC_DISABLE(&s_tft.hltdc);
}

void TFT_SetBacklight(uint8_t percent)
{
    if (percent > 100U)
    {
        percent = 100U;
    }
    /* bsp_SetTIMOutPWM 占空比范围 0..10000 (0.01% 分辨率) */
    bsp_SetTIMOutPWM(TFT_BACKLIGHT_GPIO_PORT, TFT_BACKLIGHT_GPIO_PIN,
                     TFT_BACKLIGHT_TIM, (uint8_t)TFT_BACKLIGHT_CHANNEL,
                     (uint32_t)TFT_BACKLIGHT_FREQ,
                     (uint32_t)percent * 100U);
    s_tft.backlight = percent;
}

/* --- Layer control --------------------------------------------------------*/

static tft_status_t tft_layer_check(tft_layer_id_t layer)
{
    if (!s_tft.initialized)
    {
        return TFT_ERR;
    }
    if ((uint32_t)layer > 1U)
    {
        return TFT_ERR;
    }
    return TFT_OK;
}

tft_status_t TFT_LayerSetVisible(tft_layer_id_t layer, uint8_t visible)
{
    if (tft_layer_check(layer) != TFT_OK)
    {
        return TFT_ERR;
    }
    if (visible)
    {
        __HAL_LTDC_LAYER_ENABLE(&s_tft.hltdc, (uint32_t)layer);
    }
    else
    {
        __HAL_LTDC_LAYER_DISABLE(&s_tft.hltdc, (uint32_t)layer);
    }
    /* IMR/SRCR reload to apply LEN change */
    __HAL_LTDC_RELOAD_IMMEDIATE_CONFIG(&s_tft.hltdc);
    return TFT_OK;
}

tft_status_t TFT_LayerSetAlpha(tft_layer_id_t layer, uint8_t alpha)
{
    if (tft_layer_check(layer) != TFT_OK)
    {
        return TFT_ERR;
    }
    return (HAL_LTDC_SetAlpha(&s_tft.hltdc, alpha, (uint32_t)layer) == HAL_OK)
               ? TFT_OK
               : TFT_ERR;
}

tft_status_t TFT_LayerSetAddress(tft_layer_id_t layer, uint32_t addr)
{
    if (tft_layer_check(layer) != TFT_OK)
    {
        return TFT_ERR;
    }
    return (HAL_LTDC_SetAddress(&s_tft.hltdc, addr, (uint32_t)layer) == HAL_OK)
               ? TFT_OK
               : TFT_ERR;
}

tft_status_t TFT_LayerSetWindowPos(tft_layer_id_t layer, uint16_t x0, uint16_t y0)
{
    if (tft_layer_check(layer) != TFT_OK)
    {
        return TFT_ERR;
    }
    return (HAL_LTDC_SetWindowPosition(&s_tft.hltdc, x0, y0, (uint32_t)layer) == HAL_OK)
               ? TFT_OK
               : TFT_ERR;
}

tft_status_t TFT_LayerSetWindowSize(tft_layer_id_t layer, uint16_t width, uint16_t height)
{
    if (tft_layer_check(layer) != TFT_OK)
    {
        return TFT_ERR;
    }
    return (HAL_LTDC_SetWindowSize(&s_tft.hltdc, width, height, (uint32_t)layer) == HAL_OK)
               ? TFT_OK
               : TFT_ERR;
}

/* ===========================================================================
 * Shell command (letter_shell)
 * ===========================================================================*/
#if defined(__SHELL_H__) && defined(DEBUG_MODE)

static int _tft_cmd(int argc, char *argv[])
{
    static const char *help_info[] = {
        "info                       - print panel info",
        "init                       - initialise LTDC + backlight",
        "deinit                     - de-initialise LTDC + backlight off",
        "panel <id>                 - select panel id (must DeInit first)",
        "list                       - list available panels",
        "bl <0..100>                - set backlight percent",
        "on                         - display on",
        "off                        - display off",
        "layer <id> show|hide       - enable/disable layer 0/1",
        "layer <id> alpha <0..255>  - set per-layer alpha",
        "layer <id> addr  <0xADDR>  - set framebuffer address",
        "layer <id> pos   <x> <y>   - set window position",
        "layer <id> size  <w> <h>   - set window size",
    };

    if (argc < 2)
    {
        printf("Usage:\r\n");
        for (uint32_t i = 0; i < sizeof(help_info) / sizeof(help_info[0]); i++)
        {
            printf("  %s %s\r\n", argv[0], help_info[i]);
        }
        return -1;
    }

    if (strcmp(argv[1], "info") == 0)
    {
        const tft_cfg_t *cfg = TFT_GetCfg();
        printf("panel id : %u\r\n", (unsigned)bsp_GetTFTPanelId());
        printf("name     : %s\r\n", cfg->name);
        printf("size     : %u x %u\r\n", cfg->pwidth, cfg->pheight);
        printf("buf0     : 0x%08lX\r\n", (unsigned long)TFT_GetFrameBuffer(TFT_LAYER_BG));
        printf("buf1     : 0x%08lX\r\n", (unsigned long)TFT_GetFrameBuffer(TFT_LAYER_FG));
        printf("init     : %u\r\n", (unsigned)s_tft.initialized);
        printf("backlight: %u %%\r\n", (unsigned)s_tft.backlight);
        return 0;
    }

    if (strcmp(argv[1], "list") == 0)
    {
        for (uint32_t i = 0; i < (uint32_t)TFT_PANEL_NUM; i++)
        {
            printf("  [%lu] %s (%u x %u)\r\n",
                   (unsigned long)i, s_lcd_cfg_list[i].name,
                   s_lcd_cfg_list[i].pwidth, s_lcd_cfg_list[i].pheight);
        }
        return 0;
    }

    if (strcmp(argv[1], "init") == 0)
    {
        bsp_InitTFT();
        return 0;
    }

    if (strcmp(argv[1], "deinit") == 0)
    {
        if (bsp_DeInitTFT() != TFT_OK)
        {
            printf("deinit failed (already de-initialised?)\r\n");
            return -1;
        }
        return 0;
    }

    if (strcmp(argv[1], "panel") == 0 && argc >= 3)
    {
        if (bsp_SelectTFT((tft_panel_id_t)atoi(argv[2])) != TFT_OK)
        {
            printf("invalid panel id, or LTDC currently running\r\n");
            return -1;
        }
        return 0;
    }

    if (strcmp(argv[1], "bl") == 0 && argc >= 3)
    {
        TFT_SetBacklight((uint8_t)atoi(argv[2]));
        return 0;
    }

    if (strcmp(argv[1], "on") == 0)
    {
        TFT_DispOn();
        return 0;
    }

    if (strcmp(argv[1], "off") == 0)
    {
        TFT_DispOff();
        return 0;
    }

    /* layer <id> <op> [args...] ------------------------------------------ */
    if (strcmp(argv[1], "layer") == 0 && argc >= 4)
    {
        tft_layer_id_t lid = (tft_layer_id_t)atoi(argv[2]);
        const char *op = argv[3];
        tft_status_t rc = TFT_ERR;

        if (strcmp(op, "show") == 0)
        {
            rc = TFT_LayerSetVisible(lid, 1U);
        }
        else if (strcmp(op, "hide") == 0)
        {
            rc = TFT_LayerSetVisible(lid, 0U);
        }
        else if (strcmp(op, "alpha") == 0 && argc >= 5)
        {
            rc = TFT_LayerSetAlpha(lid, (uint8_t)atoi(argv[4]));
        }
        else if (strcmp(op, "addr") == 0 && argc >= 5)
        {
            rc = TFT_LayerSetAddress(lid, (uint32_t)strtoul(argv[4], NULL, 0));
        }
        else if (strcmp(op, "pos") == 0 && argc >= 6)
        {
            rc = TFT_LayerSetWindowPos(lid,
                                       (uint16_t)atoi(argv[4]),
                                       (uint16_t)atoi(argv[5]));
        }
        else if (strcmp(op, "size") == 0 && argc >= 6)
        {
            rc = TFT_LayerSetWindowSize(lid,
                                        (uint16_t)atoi(argv[4]),
                                        (uint16_t)atoi(argv[5]));
        }
        else
        {
            printf("layer op: show|hide|alpha|addr|pos|size\r\n");
            return -1;
        }

        if (rc != TFT_OK)
        {
            printf("layer op failed (id range / not initialised?)\r\n");
            return -1;
        }
        return 0;
    }

    printf("unknown sub-command: %s\r\n", argv[1]);
    return -1;
}

SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), tft, _tft_cmd, tft[info list init deinit panel bl layer on off]);
#endif /* __SHELL_H__ && DEBUG_MODE */

/*************************************** (END OF FILE) ****************************************/
