/*
*********************************************************************************************************
*
*    模块名称 : LED指示灯驱动模块
*    文件名称 : bsp_led.c
*    版    本 : V1.0
*    说    明 : 驱动LED指示灯
*
*    修改记录 :
*        版本号  日期        作者     说明
*        V1.0    2018-09-05 armfly  正式发布
*
*    Copyright (C), 2015-2030, 安富莱电子 www.armfly.com
*
*********************************************************************************************************
*/
#include "bsp.h"

/*
    STM32-H7 开发板的LED指示灯是由74HC574驱动的，不是用CPU的IO直接驱动。
    74HC574是一个8路并口缓冲器，挂在FMC总线上。
    74HC574的驱动程序为 : bsp_fmc_io.c
*/

/*
*********************************************************************************************************
*    函 数 名: bsp_InitLed
*    功能说明: 配置LED指示灯相关的GPIO,  该函数被 bsp_Init() 调用。
*    形    参:  无
*    返 回 值: 无
*********************************************************************************************************
*/
void bsp_InitLed(void)
{
    bsp_LedOff(1);
    bsp_LedOff(2);
    bsp_LedOff(3);
    bsp_LedOff(4);
}

/*
*********************************************************************************************************
*    函 数 名: bsp_LedOn
*    功能说明: 点亮指定的LED指示灯。
*    形    参:  _no : 指示灯序号，范围 1 - 4
*    返 回 值: 无
*********************************************************************************************************
*/
void bsp_LedOn(uint8_t _no)
{
    if (_no == 1)
    {
        HC574_SetPin(LED1, 0);
    }
    else if (_no == 2)
    {
        HC574_SetPin(LED2, 0);
    }
    else if (_no == 3)
    {
        HC574_SetPin(LED3, 0);
    }
    else if (_no == 4)
    {
        HC574_SetPin(LED4, 0);
    }
}

/*
*********************************************************************************************************
*    函 数 名: bsp_LedOff
*    功能说明: 熄灭指定的LED指示灯。
*    形    参:  _no : 指示灯序号，范围 1 - 4
*    返 回 值: 无
*********************************************************************************************************
*/
void bsp_LedOff(uint8_t _no)
{
    if (_no == 1)
    {
        HC574_SetPin(LED1, 1);
    }
    else if (_no == 2)
    {
        HC574_SetPin(LED2, 1);
    }
    else if (_no == 3)
    {
        HC574_SetPin(LED3, 1);
    }
    else if (_no == 4)
    {
        HC574_SetPin(LED4, 1);
    }
}

/*
*********************************************************************************************************
*    函 数 名: bsp_LedToggle
*    功能说明: 翻转指定的LED指示灯。
*    形    参:  _no : 指示灯序号，范围 1 - 4
*    返 回 值: 按键代码
*********************************************************************************************************
*/
void bsp_LedToggle(uint8_t _no)
{
    uint32_t pin;

    if (_no == 1)
    {
        pin = LED1;
    }
    else if (_no == 2)
    {
        pin = LED2;
    }
    else if (_no == 3)
    {
        pin = LED3;
    }
    else if (_no == 4)
    {
        pin = LED4;
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

/*
*********************************************************************************************************
*    函 数 名: bsp_IsLedOn
*    功能说明: 判断LED指示灯是否已经点亮。
*    形    参:  _no : 指示灯序号，范围 1 - 4
*    返 回 值: 1表示已经点亮，0表示未点亮
*********************************************************************************************************
*/
uint8_t bsp_IsLedOn(uint8_t _no)
{
    uint32_t pin;

    if (_no == 1)
    {
        pin = LED1;
    }
    else if (_no == 2)
    {
        pin = LED2;
    }
    else if (_no == 3)
    {
        pin = LED3;
    }
    else if (_no == 4)
    {
        pin = LED4;
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

/***************************** 安富莱电子 www.armfly.com (END OF FILE) *********************************/
