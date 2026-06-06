/**
 * @file    bsp_fmc_io.h
 * @brief   FMC 总线扩展 IO（HC574 锁存器）驱动模块头文件
 *          总线地址 = 0x6820 0000，32 位输出，当前仅 LED1-LED4 已接线
 */
#ifndef __BSP_EXT_IO_H
#define __BSP_EXT_IO_H

/* 供外部调用的函数声明 */
#ifndef GPIO_PIN_0
#define GPIO_PIN_0 ((uint16_t)0x0001)  /* Pin 0 selected */
#define GPIO_PIN_1 ((uint16_t)0x0002)  /* Pin 1 selected */
#define GPIO_PIN_2 ((uint16_t)0x0004)  /* Pin 2 selected */
#define GPIO_PIN_3 ((uint16_t)0x0008)  /* Pin 3 selected */
#define GPIO_PIN_4 ((uint16_t)0x0010)  /* Pin 4 selected */
#define GPIO_PIN_5 ((uint16_t)0x0020)  /* Pin 5 selected */
#define GPIO_PIN_6 ((uint16_t)0x0040)  /* Pin 6 selected */
#define GPIO_PIN_7 ((uint16_t)0x0080)  /* Pin 7 selected */
#define GPIO_PIN_8 ((uint16_t)0x0100)  /* Pin 8 selected */
#define GPIO_PIN_9 ((uint16_t)0x0200)  /* Pin 9 selected */
#define GPIO_PIN_10 ((uint16_t)0x0400) /* Pin 10 selected */
#define GPIO_PIN_11 ((uint16_t)0x0800) /* Pin 11 selected */
#define GPIO_PIN_12 ((uint16_t)0x1000) /* Pin 12 selected */
#define GPIO_PIN_13 ((uint16_t)0x2000) /* Pin 13 selected */
#define GPIO_PIN_14 ((uint16_t)0x4000) /* Pin 14 selected */
#define GPIO_PIN_15 ((uint16_t)0x8000) /* Pin 15 selected */
#endif

#define GPIO_PIN_16 ((uint32_t)0x00010000) /* Pin 0 selected */
#define GPIO_PIN_17 ((uint32_t)0x00020000) /* Pin 1 selected */
#define GPIO_PIN_18 ((uint32_t)0x00040000) /* Pin 2 selected */
#define GPIO_PIN_19 ((uint32_t)0x00080000) /* Pin 3 selected */
#define GPIO_PIN_20 ((uint32_t)0x00100000) /* Pin 4 selected */
#define GPIO_PIN_21 ((uint32_t)0x00200000) /* Pin 5 selected */
#define GPIO_PIN_22 ((uint32_t)0x00400000) /* Pin 6 selected */
#define GPIO_PIN_23 ((uint32_t)0x00800000) /* Pin 7 selected */
#define GPIO_PIN_24 ((uint32_t)0x01000000) /* Pin 8 selected */
#define GPIO_PIN_25 ((uint32_t)0x02000000) /* Pin 9 selected */
#define GPIO_PIN_26 ((uint32_t)0x04000000) /* Pin 10 selected */
#define GPIO_PIN_27 ((uint32_t)0x08000000) /* Pin 11 selected */
#define GPIO_PIN_28 ((uint32_t)0x10000000) /* Pin 12 selected */
#define GPIO_PIN_29 ((uint32_t)0x20000000) /* Pin 13 selected */
#define GPIO_PIN_30 ((uint32_t)0x40000000) /* Pin 14 selected */
#define GPIO_PIN_31 ((uint32_t)0x80000000) /* Pin 15 selected */

/*
 * 以下宏对应安富莱原版扩展板外设的 32 位总线位分配。
 * 当前仅 LED1-LED4 已在本板接线，其余宏保留供将来扩展复用。
 */
#define HC574_GPRS_TERM_ON GPIO_PIN_0  /* D0  - GPRS_TERM_ON */
#define HC574_GPRS_RESET GPIO_PIN_1    /* D1  - GPRS_RESET */
#define HC574_NRF24L01_CE GPIO_PIN_2   /* D2  - NRF24L01_CE */
#define HC574_NRF905_TX_EN GPIO_PIN_3  /* D3  - NRF905_TX_EN */
#define HC574_NRF905_TRX_CE GPIO_PIN_4 /* D4  - NRF905_TRX_CE / VS1053_XDCS 复用 */
#define HC574_VS1053_XDCS GPIO_PIN_4   /* D4  - VS1053_XDCS / NRF905_TRX_CE 复用 */
#define HC574_NRF905_PWR_UP GPIO_PIN_5 /* D5  - NRF905_PWR_UP */
#define HC574_ESP8266_G0 GPIO_PIN_6    /* D6  - ESP8266_G0 */
#define HC574_ESP8266_G2 GPIO_PIN_7    /* D7  - ESP8266_G2 */

#define HC574_LED1 GPIO_PIN_8        /* D8  - LED1 */
#define HC574_LED2 GPIO_PIN_9        /* D9  - LED2 */
#define HC574_LED3 GPIO_PIN_10       /* D10 - LED3 */
#define HC574_LED4 GPIO_PIN_11       /* D11 - LED4 */
#define HC574_TP_NRST GPIO_PIN_12    /* D12 - TP_NRST */
#define HC574_AD7606_OS0 GPIO_PIN_13 /* D13 - AD7606_OS0 */
#define HC574_AD7606_OS1 GPIO_PIN_14 /* D14 - AD7606_OS1 */
#define HC574_AD7606_OS2 GPIO_PIN_15 /* D15 - AD7606_OS2 */

#define HC574_Y50_0 GPIO_PIN_16 /* D16 - Y50_0 */
#define HC574_Y50_1 GPIO_PIN_17 /* D17 - Y50_1 */
#define HC574_Y50_2 GPIO_PIN_18 /* D18 - Y50_2 */
#define HC574_Y50_3 GPIO_PIN_19 /* D19 - Y50_3 */
#define HC574_Y50_4 GPIO_PIN_20 /* D20 - Y50_4 */
#define HC574_Y50_5 GPIO_PIN_21 /* D21 - Y50_5 */
#define HC574_Y50_6 GPIO_PIN_22 /* D22 - Y50_6 */
#define HC574_Y50_7 GPIO_PIN_23 /* D23 - Y50_7 */

#define HC574_AD7606_RESET GPIO_PIN_24 /* D24 - AD7606_RESET */
#define HC574_AD7606_RANGE GPIO_PIN_25 /* D25 - AD7606_RANGE */
#define HC574_Y33_2 GPIO_PIN_26        /* D26 - Y33_2 */
#define HC574_Y33_3 GPIO_PIN_27        /* D27 - Y33_3 */
#define HC574_Y33_4 GPIO_PIN_28        /* D28 - Y33_4 */
#define HC574_Y33_5 GPIO_PIN_29        /* D29 - Y33_5 */
#define HC574_Y33_6 GPIO_PIN_30        /* D30 - Y33_6 */
#define HC574_Y33_7 GPIO_PIN_31        /* D31 - Y33_7 */

void bsp_InitExtIO(void);
void HC574_SetPin(uint32_t _pin, uint8_t _value);
uint8_t HC574_GetPin(uint32_t _pin);
void HC574_TogglePin(uint32_t _pin);

extern __IO uint32_t g_HC574;

#endif

/* end of file */
