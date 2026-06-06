/**
 * @file    bsp_led.c
 * @brief   LED 指示灯驱动模块（通过 FMC 扩展 IO 控制）
 */
#include "bsp.h"

/*
    STM32-H7 开发板的LED指示灯是由74HC574驱动的，不是用CPU的IO直接驱动。
    74HC574是一个8路并口缓冲器，挂在FMC总线上。
    74HC574的驱动程序为 : bsp_fmc_io.c
*/

/**
 * @brief  初始化 LED（全部熄灭），由 bsp_Init() 调用
 * @retval 无
 */
void bsp_InitLed(void)
{
    bsp_LedOff(1);
    bsp_LedOff(2);
    bsp_LedOff(3);
    bsp_LedOff(4);
}

/**
 * @brief  点亮指定 LED
 * @param  _no  LED 序号（1-4）
 * @retval 无
 */
void bsp_LedOn(uint8_t _no)
{
    if (_no == 1)
    {
        HC574_SetPin(HC574_LED1, 0);
    }
    else if (_no == 2)
    {
        HC574_SetPin(HC574_LED2, 0);
    }
    else if (_no == 3)
    {
        HC574_SetPin(HC574_LED3, 0);
    }
    else if (_no == 4)
    {
        HC574_SetPin(HC574_LED4, 0);
    }
}

/**
 * @brief  熄灭指定 LED
 * @param  _no  LED 序号（1-4）
 * @retval 无
 */
void bsp_LedOff(uint8_t _no)
{
    if (_no == 1)
    {
        HC574_SetPin(HC574_LED1, 1);
    }
    else if (_no == 2)
    {
        HC574_SetPin(HC574_LED2, 1);
    }
    else if (_no == 3)
    {
        HC574_SetPin(HC574_LED3, 1);
    }
    else if (_no == 4)
    {
        HC574_SetPin(HC574_LED4, 1);
    }
}

/**
 * @brief  翻转指定 LED 状态
 * @param  _no  LED 序号（1-4）
 * @retval 无
 */
void bsp_LedToggle(uint8_t _no)
{
    uint32_t pin;

    if (_no == 1)
    {
        pin = HC574_LED1;
    }
    else if (_no == 2)
    {
        pin = HC574_LED2;
    }
    else if (_no == 3)
    {
        pin = HC574_LED3;
    }
    else if (_no == 4)
    {
        pin = HC574_LED4;
    }
    else
    {
        return;
    }

    if (HC574_GetPin(pin))
    {
        HC574_SetPin(pin, 0);
    }
    else
    {
        HC574_SetPin(pin, 1);
    }
}

/**
 * @brief  查询指定 LED 是否点亮
 * @param  _no  LED 序号（1-4）
 * @retval 1 = 亮，0 = 灭
 */
uint8_t bsp_IsLedOn(uint8_t _no)
{
    uint32_t pin;

    if (_no == 1)
    {
        pin = HC574_LED1;
    }
    else if (_no == 2)
    {
        pin = HC574_LED2;
    }
    else if (_no == 3)
    {
        pin = HC574_LED3;
    }
    else if (_no == 4)
    {
        pin = HC574_LED4;
    }
    else
    {
        return 0;
    }

    if (HC574_GetPin(pin))
    {
        return 0; /* 灭 */
    }
    else
    {
        return 1; /* 亮 */
    }
}

/* end of file */
