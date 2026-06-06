/**
 * @file    bsp_beep.h
 * @brief   蜂鸣器驱动模块头文件
 */

#ifndef __BSP_BEEP_H
#define __BSP_BEEP_H

typedef struct _BEEP_T
{
    uint8_t ucEnalbe;
    uint8_t ucState;
    uint16_t usBeepTime;
    uint16_t usStopTime;
    uint16_t usCycle;
    uint16_t usCount;
    uint16_t usCycleCount;
    uint8_t ucMute; /* 1表示静音 */
} BEEP_T;

/* 供外部调用的函数声明 */
void BEEP_InitHard(void);
void BEEP_Start(uint16_t _usBeepTime, uint16_t _usStopTime, uint16_t _usCycle);
void BEEP_Stop(void);
void BEEP_KeyTone(void);
void BEEP_Pro(void);

void BEEP_Pause(void);
void BEEP_Resume(void);

#endif

/* end of file */
