/**
 * @file timer_port.c
 * @author 0x1abin
 * @brief
 * @version 0.1
 * @date 2019-02-22
 *
 * @copyright (c) 2019
 *
 */
#include "MultiTimer.h"
#include "timer_port.h"
#include "bsp.h"

static MultiTimer Timer5ms;
static MultiTimer Timer10ms;

static uint64_t PlatformTicksGetFunc(void)
{
    return (uint64_t)get_system_ms();
}

static void Timer5msCallback(MultiTimer *timer, void *userData)
{
    MultiTimerStart(timer, 5, Timer5msCallback, userData);
    bsp_KeyScan5ms();
}

static void Timer10msCallback(MultiTimer *timer, void *userData)
{
    MultiTimerStart(timer, 10, Timer10msCallback, userData);
    BEEP_Pro();
}

void userInitMultiTime(void)
{
    MultiTimerInstall(PlatformTicksGetFunc); //配置系统时间基准接口
    MultiTimerStart(&Timer5ms, 10, Timer5msCallback, "5ms CYCLE timer");
    MultiTimerStart(&Timer10ms, 10, Timer10msCallback, "10ms CYCLE timer");
}
