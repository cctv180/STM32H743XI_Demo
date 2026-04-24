/**
 *********************************************************************************************************
 * @file    bsp_sdio_sd.h
 * @author  cctv180
 * @brief   STM32H7 SDMMC1 (SD 卡) 底层驱动接口
 *********************************************************************************************************
 *
 *  功能：
 *    - SD 卡插拔检测（multi_button 消抖）
 *    - HAL_SD 初始化封装；可选 100 MHz overclock 开关（见 .c 内 SD_OVERCLOCK_100MHZ）
 *    - 块级读写（IDMA，阻塞至卡 TRANSFER）
 *    - 字节级读写（自动处理非对齐首尾与非对齐 buffer）
 *    - Shell 调试命令（status / init / deinit / info / read / write / erase / bench）
 *
 *  依赖：
 *    - HAL_SD、HAL_RCC
 *    - multi_button 库（工程需周期调用 button_ticks()）
 *
 *  使用流程：
 *    bsp_Init_SD();              // 一次性初始化（检测 GPIO + multi_button 注册）
 *    // 插卡后 multi_button 长按会自动触发 bsp_SD_HardwareInit();
 *    // 也可通过 shell: sd init 主动触发；或程序里 bsp_SD_HardwareInit();
 *
 *    bsp_SD_ReadBlocks(...);     // 块对齐场景
 *    bsp_SD_Read(...);           // 任意字节地址/大小
 *
 *    bsp_SD_GetStatus();         // 轮询是否 READY / 热拔出消费
 *
 *********************************************************************************************************
 */

#ifndef __BSP_SDIO_SD_H
#define __BSP_SDIO_SD_H

#ifdef __cplusplus
extern "C"
{
#endif

    /* Includes -------------------------------------------------------------- */
#include "bsp.h"

    /* Exported constants ---------------------------------------------------- */

    /** SD 标准块大小（字节） */
#define SD_BLOCK_SIZE 512u

    /* Exported types -------------------------------------------------------- */

    /**
     * @brief SD 卡综合状态机。
     *
     *   ABSENT ─插入─> INSERTED_NOT_READY ─init OK─> READY
     *                                      ─init NG─> INIT_FAILED ─重试OK─> READY
     *                                                              ─拔出──> ABSENT
     *   READY ─拔出─> EJECTED ─上层消费后─> ABSENT
     */
    typedef enum
    {
        BSP_SD_STATUS_ABSENT = 0,         /**< 未插入 */
        BSP_SD_STATUS_INSERTED_NOT_READY, /**< 已插入，尚未尝试初始化 */
        BSP_SD_STATUS_READY,              /**< HAL_SD 已初始化，可块读写 */
        BSP_SD_STATUS_INIT_FAILED,        /**< 已插入，HAL_SD_Init 失败（可重试） */
        BSP_SD_STATUS_EJECTED,            /**< 热拔出（之前为 READY），上层消费后自动转 ABSENT */
    } bsp_SD_Status_t;

    /* Exported functions ---------------------------------------------------- */

    /* ---- 生命周期 ----------------------------------------------------------
     *   bsp_Init_SD          初始化检测 GPIO + multi_button
     *   bsp_DeInit_SD        释放 SDMMC (卡仍在位回到 INSERTED_NOT_READY)
     *   bsp_SD_HardwareInit  执行 HAL_SD_Init（插卡后、或 shell 手动调用）
     */

    bsp_SD_Status_t bsp_Init_SD(void);
    bsp_SD_Status_t bsp_DeInit_SD(void);
    bsp_SD_Status_t bsp_SD_HardwareInit(void);

    /* ---- 状态查询 ----------------------------------------------------------
     *   bsp_SD_GetStatus     综合状态（含热拔出一次性消费）
     *   bsp_SD_GetCardState  查询卡当前是否处于 TRANSFER（就绪可读写）
     *   bsp_SD_IsHwReady     1 = HAL_SD 已 init
     *   bsp_SD_GetCardInfo   获取容量、版本等（仅 READY 时有效）
     */

    bsp_SD_Status_t bsp_SD_GetStatus(void);
    bsp_SD_Status_t bsp_SD_GetCardState(void);
    uint8_t bsp_SD_IsHwReady(void);
    void bsp_SD_GetCardInfo(HAL_SD_CardInfoTypeDef *CardInfo);

    /* ---- 块级读写（IDMA，阻塞直到卡回到 TRANSFER） -------------------------
     *   pData 建议 32 字节对齐以获得最佳性能；未 32 字节对齐请经字节接口或
     *   自行提供对齐缓冲。成功返回 READY，其他为失败原因（可据此重试）。
     */

    bsp_SD_Status_t bsp_SD_ReadBlocks(uint32_t *pData, uint32_t BlockAddr, uint32_t NumOfBlocks);
    bsp_SD_Status_t bsp_SD_WriteBlocks(uint32_t *pData, uint32_t BlockAddr, uint32_t NumOfBlocks);
    bsp_SD_Status_t bsp_SD_Erase(uint32_t BlockStart, uint32_t BlockEnd);

    /* ---- 字节级读写（任意地址 / 任意大小） ---------------------------------
     *   内部自动处理首尾非 512 对齐部分（RMW），以及非 32 字节对齐 buffer
     *   （逐块经 g_sd_buf 中转保证 D-Cache 安全）。
     */

    bsp_SD_Status_t bsp_SD_Read(uint8_t *pBuffer, uint32_t ReadAddr, uint32_t Size);
    bsp_SD_Status_t bsp_SD_Write(const uint8_t *pBuffer, uint32_t WriteAddr, uint32_t Size);

#ifdef __cplusplus
}
#endif

#endif /* __BSP_SDIO_SD_H */

/************************ (C) COPYRIGHT cctv180 *****END OF FILE****/
