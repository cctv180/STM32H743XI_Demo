/*
*********************************************************************************************************
*
*	模块名称 : SD卡驱动模块
*	文件名称 : bsp_sdio_sd.h
*	说    明 : SD卡底层驱动。根据stm32h743i_eval_sd.c文件修改。
*
*********************************************************************************************************
*/

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __STM32H743I_EVAL_SD_H
#define __STM32H743I_EVAL_SD_H

#ifdef __cplusplus
extern "C"
{
#endif

/* Includes ------------------------------------------------------------------*/
#include "bsp.h"

/**
 * @brief  SD status structure definition
 */
#define MSD_OK ((uint8_t)0x00)                   // SD操作成功
#define MSD_ERROR ((uint8_t)0x01)                // SD操作错误
#define MSD_ERROR_SD_NOT_PRESENT ((uint8_t)0x02) // SD卡未插入

/**
 * @brief  SD transfer state definition
 */
#define SD_TRANSFER_OK ((uint8_t)0x00)
#define SD_TRANSFER_BUSY ((uint8_t)0x01)

/** @defgroup STM32H743I_EVAL_SD_Exported_Constants SD Exported Constants */
#define SD_PRESENT ((uint8_t)0x01)     // SD卡插入
#define SD_NOT_PRESENT ((uint8_t)0x00) // SD卡未插入

#define SD_DATATIMEOUT ((uint32_t)100000000)

#ifndef USE_SD_TRANSCEIVER
#define USE_SD_TRANSCEIVER 0
#endif /* USE_SD_TRANSCEIVER */

   /** @defgroup STM32H743I_EVAL_SD_Exported_Functions SD Exported Functions */
   uint8_t bsp_Init_SD(void);
   uint8_t bsp_DeInit_SD(void);
   uint8_t bsp_SD_IsDetected(void);

   uint8_t bsp_SD_ReadBlocks(uint32_t *pData, uint32_t ReadAddr, uint32_t NumOfBlocks, uint32_t Timeout);
   uint8_t bsp_SD_WriteBlocks(uint32_t *pData, uint32_t WriteAddr, uint32_t NumOfBlocks, uint32_t Timeout);
   uint8_t bsp_SD_ReadBlocks_DMA(uint32_t *pData, uint32_t ReadAddr, uint32_t NumOfBlocks);
   uint8_t bsp_SD_WriteBlocks_DMA(uint32_t *pData, uint32_t WriteAddr, uint32_t NumOfBlocks);
   uint8_t bsp_SD_Erase(uint32_t StartAddr, uint32_t EndAddr);
   uint8_t bsp_SD_GetCardState(void);
   void bsp_SD_GetCardInfo(HAL_SD_CardInfoTypeDef *CardInfo);

#ifdef __cplusplus
}
#endif

#endif /* __STM32H743I_EVAL_SD_H */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
