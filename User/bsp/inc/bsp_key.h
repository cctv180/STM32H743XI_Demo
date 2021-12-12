/*
*********************************************************************************************************
*
*    模块名称 : 按键驱动模块
*    文件名称 : bsp_key.h
*    版    本 : V1.0
*    说    明 : 头文件
*
*    Copyright (C), 2013-2014, 安富莱电子 www.armfly.com
*
*********************************************************************************************************
*/

#ifndef __BSP_KEY_H
#define __BSP_KEY_H
#include "multi_button.h"

/* 根据应用程序的功能重命名按键宏 */

/* 按键ID, 主要用于bsp_KeyState()函数的入口参数 */
typedef enum
{
    KID_K1 = 0,
    KID_K2,
    KID_K3,
    KID_JOY_U,
    KID_JOY_D,
    KID_JOY_L,
    KID_JOY_R,
    KID_JOY_OK,
    KID_K1_K2,
    KID_K2_K3
} KEY_ID_E;

/* 按键FIFO用到变量 */
#define KEY_FIFO_SIZE 10

/* 按键值宏 */
#define KEY_NONE                        255                                             /* 无按键 */
#define KEY_PRESS_DOWN(key_id)          (key_id * number_of_event + PRESS_DOWN)         /* 按键按下 */
#define KEY_PRESS_UP(key_id)            (key_id * number_of_event + PRESS_UP)           /* 按键弹起 */
#define KEY_PRESS_REPEAT(key_id)        (key_id * number_of_event + PRESS_REPEAT)       /* 重复按下触发 */
#define KEY_SINGLE_CLICK(key_id)        (key_id * number_of_event + SINGLE_CLICK)       /* 单击按键事件 */
#define KEY_DOUBLE_CLICK(key_id)        (key_id * number_of_event + DOUBLE_CLICK)       /* 双击按键事件 */
#define KEY_LONG_PRESS_START(key_id)    (key_id * number_of_event + LONG_PRESS_START)   /* 达到长按时间阈值时触发一次 */
#define KEY_LONG_PRESS_HOLD(key_id)     (key_id * number_of_event + LONG_PRESS_HOLD)    /* 长按期间一直触发 */

/* 供外部调用的函数声明 */
void bsp_InitKey(void);
void bsp_PutKey(uint8_t _KeyCode);
uint8_t bsp_GetKey(void);
uint8_t bsp_GetKeyState(KEY_ID_E _ucKeyID);
void bsp_ClearKey(void);

/*
*********************************************************************************************************
*    函 数 名: bsp_KeyScan5ms
*    功能说明: 扫描所有按键。非阻塞，被systick中断周期性的调用，5ms一次
*    形    参: 无
*    返 回 值: 无
*********************************************************************************************************
*/
inline void bsp_KeyScan5ms(void)
{
    button_ticks();
}
#endif

/***************************** 安富莱电子 www.armfly.com (END OF FILE) *********************************/
