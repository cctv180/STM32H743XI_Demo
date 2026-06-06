/**
 * @file    bsp_beep.c
 * @brief   蜂鸣器驱动模块（无源 TIM PWM / 可选有源 GPIO 直驱，支持静音）
 */

#include "bsp.h"

// #define BEEP_HAVE_POWER        /* 定义此行表示有源蜂鸣器，直接通过GPIO驱动, 无需PWM */

#ifdef BEEP_HAVE_POWER /* 有源蜂鸣器 */

/* PA8 */
#define GPIO_RCC_BEEP RCC_AHB1Periph_GPIOA
#define GPIO_PORT_BEEP GPIOA
#define GPIO_PIN_BEEP GPIO_PIN_8

#define BEEP_ENABLE() GPIO_PORT_BEEP->BSRRL = GPIO_PIN_BEEP  /* 使能蜂鸣器鸣叫 */
#define BEEP_DISABLE() GPIO_PORT_BEEP->BSRRH = GPIO_PIN_BEEP /* 禁止蜂鸣器鸣叫 */

#else /* 无源蜂鸣器 */
/* PA0 ---> TIM5_CH1 */

/* 1500表示频率1.5KHz，5000表示50.00%的占空比 */
#define BEEP_ENABLE() bsp_SetTIMOutPWM(GPIOA, GPIO_PIN_0, TIM5, 1, 1500, 5000);

/* 禁止蜂鸣器鸣叫 */
#define BEEP_DISABLE() bsp_SetTIMOutPWM(GPIOA, GPIO_PIN_0, TIM5, 1, 1500, 0);
#endif // BEEP_HAVE_POWER

BEEP_T g_tBeep; /* 定义蜂鸣器全局结构体变量 */

/**
 * @brief  初始化蜂鸣器硬件（无源：TIM5 CH1 PWM；有源：GPIO 直驱）
 * @retval 无
 */
void BEEP_InitHard(void)
{
#ifdef BEEP_HAVE_POWER /* 有源蜂鸣器 */
    GPIO_InitTypeDef GPIO_InitStructure;

    /* 打开 GPIO 时钟 */
    RCC_AHB1PeriphClockCmd(GPIO_RCC_BEEP, ENABLE);

    BEEP_DISABLE();

    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;     /* 设为输出口 */
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;    /* 设为推挽模式 */
    GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;  /* 上下拉电阻不使能 */
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz; /* IO口最大速度 */

    GPIO_InitStructure.GPIO_Pin = GPIO_PIN_BEEP;
    GPIO_Init(GPIO_PORT_BEEP, &GPIO_InitStructure);
#endif

    g_tBeep.ucMute = 0; /* 关闭静音 */
}

/**
 * @brief  启动蜂鸣器
 * @param  _usBeepTime  蜂鸣持续时间（单位 10ms），0 表示不鸣叫
 * @param  _usStopTime  间隔停止时间（单位 10ms），0 表示连续鸣叫
 * @param  _usCycle     鸣叫次数，0 表示持续鸣叫
 * @retval 无
 */
void BEEP_Start(uint16_t _usBeepTime, uint16_t _usStopTime, uint16_t _usCycle)
{
    if (_usBeepTime == 0 || g_tBeep.ucMute == 1)
    {
        return;
    }

    g_tBeep.usBeepTime = _usBeepTime;
    g_tBeep.usStopTime = _usStopTime;
    g_tBeep.usCycle = _usCycle;
    g_tBeep.usCount = 0;
    g_tBeep.usCycleCount = 0;
    g_tBeep.ucState = 0;
    g_tBeep.ucEnalbe = 1; /* 设置完全局参数后再使能发声标志 */

    BEEP_ENABLE(); /* 开始发声 */
}

#if defined(__SHELL_H__) && defined(DEBUG_MODE)
// 导出到命令列表里
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_FUNC), beep, BEEP_Start, beep t0 t1 cycle);
#endif // ifdef DEBUG_MODE

/**
 * @brief  停止蜂鸣器
 * @retval 无
 */
void BEEP_Stop(void)
{
    g_tBeep.ucEnalbe = 0;

    if ((g_tBeep.usStopTime == 0) || (g_tBeep.usCycle == 0))
    {
        BEEP_DISABLE(); /* 必须在清控制标志后再停止发声，避免停止后在中断中又开启 */
    }
}

/**
 * @brief  临时静音蜂鸣器（如因 TIM 冲突），通过 BEEP_Resume() 恢复
 * @retval 无
 */
void BEEP_Pause(void)
{
    BEEP_Stop();

    g_tBeep.ucMute = 1; /* 静音 */
}

/**
 * @brief  恢复蜂鸣器静音前的正常功能
 * @retval 无
 */
void BEEP_Resume(void)
{
    BEEP_Stop();

    g_tBeep.ucMute = 0; /* 静音 */
}

/**
 * @brief  发出按键提示音（50ms 鸣叫，1 次）
 * @retval 无
 */
void BEEP_KeyTone(void)
{
    BEEP_Start(5, 1, 1); /* 鸣叫50ms，停10ms， 1次 */
}

/**
 * @brief  蜂鸣器状态机，每 10ms 由定时器中断调用一次
 * @retval 无
 */
void BEEP_Pro(void)
{
    if ((g_tBeep.ucEnalbe == 0) || (g_tBeep.usStopTime == 0) || (g_tBeep.ucMute == 1))
    {
        return;
    }

    if (g_tBeep.ucState == 0)
    {
        if (g_tBeep.usStopTime > 0) /* 间断发声 */
        {
            if (++g_tBeep.usCount >= g_tBeep.usBeepTime)
            {
                BEEP_DISABLE(); /* 停止发声 */
                g_tBeep.usCount = 0;
                g_tBeep.ucState = 1;
            }
        }
        else
        {
            ; /* 不做任何处理，连续发声 */
        }
    }
    else if (g_tBeep.ucState == 1)
    {
        if (++g_tBeep.usCount >= g_tBeep.usStopTime)
        {
            /* 连续发声时，直到调用stop停止为止 */
            if (g_tBeep.usCycle > 0)
            {
                if (++g_tBeep.usCycleCount >= g_tBeep.usCycle)
                {
                    /* 循环次数到，停止发声 */
                    g_tBeep.ucEnalbe = 0;
                }

                if (g_tBeep.ucEnalbe == 0)
                {
                    g_tBeep.usStopTime = 0;
                    return;
                }
            }

            g_tBeep.usCount = 0;
            g_tBeep.ucState = 0;

            BEEP_ENABLE(); /* 开始发声 */
        }
    }
}

/* end of file */
