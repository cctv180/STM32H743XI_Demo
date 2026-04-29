/**
 ******************************************************************************
 * @file    bsp_tft_h7.h
 * @brief   STM32H7 LTDC TFT 驱动 (头文件)
 *
 * @details
 *  - 基于 STM32 HAL LTDC，驱动 RGB 并行 TFT 面板。
 *  - 显存位于外部 SDRAM (BANK1)，由 bsp_fmc_sdram 模块负责初始化。
 *  - 背光通过 TIM PWM 输出，可通过 @ref TFT_SetBacklight 调节亮度。
 *
 * @copyright (C) 2026, Project Contributors
 ******************************************************************************
 */
#ifndef BSP_TFT_H7_H
#define BSP_TFT_H7_H

#ifdef __cplusplus
extern "C"
{
#endif

/* Includes ------------------------------------------------------------------*/
#include <stdint.h>
#include "stm32h7xx_hal.h"
#include "bsp_fmc_sdram.h" /* SDRAM_LCD_BUF1 / SDRAM_LCD_BUF2 默认地址 */

/* Exported configuration ----------------------------------------------------*/

/**
 * @brief 默认面板 ID。可在工程级宏中覆盖。
 */
#ifndef TFT_PANEL_DEFAULT
#define TFT_PANEL_DEFAULT TFT_PANEL_LCD7_1024X600_48M
#endif

/**
 * @brief LTDC 像素格式 (默认 RGB565)。可在工程级宏中覆盖为
 *        @c LTDC_PIXEL_FORMAT_ARGB8888 等。
 */
#ifndef TFT_PIXEL_FORMAT
#define TFT_PIXEL_FORMAT LTDC_PIXEL_FORMAT_RGB565
#endif

/* Framebuffer 地址 (位于外部 SDRAM) ----------------------------------------
 *  - 默认指向 bsp_fmc_sdram.h 里规划的 LCD 显存区域
 *      SDRAM_LCD_BUF1 = 0xC0000000  (Layer 0, 2MB)
 *      SDRAM_LCD_BUF2 = 0xC0200000  (Layer 1, 2MB)
 *  - 如果需要把 framebuffer 放到自定义位置 (例如双缓冲翻页 / 多缓存),
 *    在 bsp.h 之前 #define 这两个宏即可覆盖, 无须修改 bsp_fmc_sdram.h.
 */
#ifndef TFT_LAYER0_FB_ADDR
#define TFT_LAYER0_FB_ADDR ((uint32_t)SDRAM_LCD_BUF1)
#endif
#ifndef TFT_LAYER1_FB_ADDR
#define TFT_LAYER1_FB_ADDR ((uint32_t)SDRAM_LCD_BUF2)
#endif

/**
 * @brief 启用的 LTDC layer 个数 (1 或 2)。
 *
 *        默认跟随 bsp_fmc_sdram.h 的 SDRAM_LCD_LAYER。设为 1 时只配置
 *        Layer 0，可用作单 layer + 双 framebuffer 翻页方案。
 */
#ifndef TFT_LAYER_COUNT
#define TFT_LAYER_COUNT SDRAM_LCD_LAYER
#endif

/* 背光: TIM1_CH1 / PA8, 20kHz */
#ifndef TFT_BACKLIGHT_GPIO_PORT
#define TFT_BACKLIGHT_GPIO_PORT GPIOA
#endif
#ifndef TFT_BACKLIGHT_GPIO_PIN
#define TFT_BACKLIGHT_GPIO_PIN GPIO_PIN_8
#endif
#ifndef TFT_BACKLIGHT_TIM
#define TFT_BACKLIGHT_TIM TIM1
#endif
#ifndef TFT_BACKLIGHT_CHANNEL
#define TFT_BACKLIGHT_CHANNEL 1U
#endif
#ifndef TFT_BACKLIGHT_FREQ
#define TFT_BACKLIGHT_FREQ 20000U
#endif
#ifndef TFT_BACKLIGHT_DEFAULT
#define TFT_BACKLIGHT_DEFAULT 50U /* 0..100 % */
#endif

    /* Exported types ------------------------------------------------------------*/

    /**
     * @brief BSP TFT 操作返回值。
     */
    typedef enum
    {
        TFT_OK = 0,
        TFT_ERR = -1,
    } tft_status_t;

    /**
     * @brief 预置 TFT 面板 ID。
     */
    typedef enum
    {
        TFT_PANEL_LCD7_1024X600_48M = 0, /*!< 7寸 1024x600 @ 48 MHz   */
        TFT_PANEL_LCD43_480X272_10M,     /*!< 4.3寸 480x272 @ 10 MHz */
        TFT_PANEL_LCD7_800X480_20M,      /*!< 7寸 800x480  @ 20 MHz  */
        TFT_PANEL_LCD7_800X480_30M,      /*!< 7寸 800x480  @ 30 MHz  */
        TFT_PANEL_LCD10_1280X800_48M,    /*!< 10寸 1280x800 @ 48 MHz */
        TFT_PANEL_NUM,
    } tft_panel_id_t;

    /**
     * @brief LTDC 显示层。
     */
    typedef enum
    {
        TFT_LAYER_BG = 0, /*!< 背景层 (LTDC layer 0) */
        TFT_LAYER_FG = 1, /*!< 前景层 (LTDC layer 1) */
    } tft_layer_id_t;

    /**
     * @brief LCD 面板时序 / 几何参数。
     *
     * @note  时序定义参考 ST 官方 LTDC 文档。
     */
    typedef struct
    {
        const char *name; /*!< 面板名称字符串                    */
        uint16_t pwidth;  /*!< 面板像素宽度  (Active Width)      */
        uint16_t pheight; /*!< 面板像素高度  (Active Height)     */
        uint16_t hsw;     /*!< 水平同步宽度  (Hsync width)       */
        uint16_t vsw;     /*!< 垂直同步宽度  (Vsync width)       */
        uint16_t hbp;     /*!< 水平后廊      (HBP)               */
        uint16_t vbp;     /*!< 垂直后廊      (VBP)               */
        uint16_t hfp;     /*!< 水平前廊      (HFP)               */
        uint16_t vfp;     /*!< 垂直前廊      (VFP)               */
    } tft_cfg_t;

    /* Exported functions --------------------------------------------------------*/

    /**
     * @brief 初始化 LCD: LTDC 时钟、GPIO、时序，2 层 framebuffer 与背光。
     * @note  内部已调用 @ref bsp_SetTIMOutPWM 配置背光 PWM。
     */
    void bsp_InitTFT(void);

    /**
     * @brief 关闭 LCD: 关背光、复位 LTDC。
     */
    tft_status_t bsp_DeInitTFT(void);

    /**
     * @brief 选择面板。必须在 @ref bsp_InitTFT 之前 (或 DeInit 之后) 调用。
     *
     * @param[in] id  目标面板 ID。
     */
    tft_status_t bsp_SelectTFT(tft_panel_id_t id);

    /** @brief 获取当前面板 ID。 */
    tft_panel_id_t bsp_GetTFTPanelId(void);

    /** @brief 获取 LTDC HAL 句柄。 */
    LTDC_HandleTypeDef *TFT_GetLtdcHandle(void);

    /** @brief 获取当前面板的完整时序配置。 */
    const tft_cfg_t *TFT_GetCfg(void);

    /** @brief 获取当前面板名称字符串，用于显示 / 调试。 */
    const char *TFT_GetDescribe(void);

    /** @brief 获取当前面板宽度像素。 */
    uint16_t TFT_GetWidth(void);

    /** @brief 获取当前面板高度像素。 */
    uint16_t TFT_GetHeight(void);

    /**
     * @brief 获取指定显示层 framebuffer 的起始地址。
     *
     * @param[in] layer  显示层。
     * @return            framebuffer 起始地址（位于外部 SDRAM）。
     */
    uint32_t TFT_GetFrameBuffer(tft_layer_id_t layer);

    /** @brief 打开显示 (使能 LTDC + 恢复背光)。 */
    void TFT_DispOn(void);

    /** @brief 关闭显示 (背光关 + 关 LTDC)。 */
    void TFT_DispOff(void);

    /**
     * @brief 设置背光亮度。
     *
     * @param[in] percent  0..100，0 关闭背光，100 全亮。
     */
    void TFT_SetBacklight(uint8_t percent);

    /* --- 层控制 (LTDC layer 0/1) ----------------------------------------------*/

    /**
     * @brief 显示 / 隐藏指定显示层 (LTDC_LxCR.LEN bit)。
     *
     * @param[in] layer    显示层。
     * @param[in] visible  非 0 显示，0 隐藏 (整层关闭，与 alpha=0 不同)。
     */
    tft_status_t TFT_LayerSetVisible(tft_layer_id_t layer, uint8_t visible);

    /**
     * @brief 设置指定显示层 alpha 值，调用后会触发 LTDC 立即 reload。
     *
     * @param[in] layer  显示层。
     * @param[in] alpha  0 完全透明，255 完全不透明。
     */
    tft_status_t TFT_LayerSetAlpha(tft_layer_id_t layer, uint8_t alpha);

    /**
     * @brief 切换显示层的 framebuffer 起始地址 (双缓冲 / 翻页常用)。
     *
     * @param[in] layer  显示层。
     * @param[in] addr   新 framebuffer 起始地址 (位于 SDRAM)。
     */
    tft_status_t TFT_LayerSetAddress(tft_layer_id_t layer, uint32_t addr);

    /**
     * @brief 设置显示层窗口位置 (相对面板左上角的偏移)。
     *
     * @param[in] layer  显示层。
     * @param[in] x0     窗口左上角 x 坐标。
     * @param[in] y0     窗口左上角 y 坐标。
     */
    tft_status_t TFT_LayerSetWindowPos(tft_layer_id_t layer, uint16_t x0, uint16_t y0);

    /**
     * @brief 设置显示层窗口尺寸。
     *
     * @param[in] layer    显示层。
     * @param[in] width    窗口宽度像素。
     * @param[in] height   窗口高度像素。
     */
    tft_status_t TFT_LayerSetWindowSize(tft_layer_id_t layer, uint16_t width, uint16_t height);

#ifdef __cplusplus
}
#endif

#endif /* BSP_TFT_H7_H */

/*************************************** (END OF FILE) ****************************************/
