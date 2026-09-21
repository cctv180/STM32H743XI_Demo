/**
 * @file shell_port.c
 * @author Letter (NevermindZZT@gmail.com)
 * @brief
 * @version 0.1
 * @date 2019-02-22
 *
 * @copyright (c) 2019 Letter
 *
 */

#include "shell.h"
#include "bsp.h"

/* 1. 创建shell对象，开辟shell缓冲区 */
Shell shell;
/* 缓冲区被 shellInit 按 SHELL_HISTORY_MAX_NUMBER+1 等分，单条命令上限为 size/6 */
static char shellBuffer[512];

#if defined(__SHELL_H__) && DEBUG_MODE == 1
// shell导出到命令列表里
//                       权限设置                                命令类型            命令名称      命令实体      命令说明
// SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_FUNC), ef_print_env, ef_print_env, ef print env);

/* 系统信息 */
static int sys_info(int argc, char *agrv[])
{
    uint32_t temp;
    temp = HAL_GetDEVID();
    BSP_INFO("HAL_GetDEVID            ------     0x%08x", temp);
    temp = HAL_GetHalVersion();
    BSP_INFO("HAL_GetHalVersion       ------     0x%08x", temp);
    temp = HAL_GetREVID();
    BSP_INFO("HAL_GetREVID            ------     0x%08x", temp);
    temp = HAL_GetUIDw0();
    BSP_INFO("HAL_GetUIDw0            ------     0x%08x", temp);
    temp = HAL_GetUIDw1();
    BSP_INFO("HAL_GetUIDw1            ------     0x%08x", temp);
    temp = HAL_GetUIDw2();
    BSP_INFO("HAL_GetUIDw2            ------     0x%08x", temp);
    temp = SystemCoreClock;
    BSP_INFO("SystemCoreClock         ------       %08d", temp);
    temp = HAL_RCC_GetHCLKFreq();
    BSP_INFO("HAL_RCC_GetHCLKFreq     ------       %08d", temp);
    temp = HAL_RCC_GetPCLK1Freq();
    BSP_INFO("HAL_RCC_GetPCLK1Freq    ------       %08d", temp);
    temp = HAL_RCC_GetPCLK2Freq();
    BSP_INFO("HAL_RCC_GetPCLK2Freq    ------       %08d", temp);
    temp = HAL_RCC_GetSysClockFreq();
    BSP_INFO("HAL_RCC_GetSysClockFreq ------       %08d", temp);

    return 0;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN, sys_info, sys_info, sys info);
#endif

/* 系统重启 */
static int reboot(int argc, char *agrv[])
{
    printf("%dparameter(s)\r\n", argc);
    for (char i = 1; i < argc; i++)
    {
        printf("%s\r\n", agrv[i]);
    }
    HAL_NVIC_SystemReset();
    return 0;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), reboot, reboot, reboot);

/**
 * @brief 用户shell写
 *
 * @param data 数据
 * @param len 数据长度
 *
 * @return short 实际写入的数据长度
 */
static short userShellWrite(char *data, unsigned short len)
{
    comSendBuf(COM1, (uint8_t *)data, len);
    return (short)len;
}

/**
 * @brief 用户shell读
 *
 * @param data 数据
 * @param len 数据长度
 *
 * @return short 实际读取到
 */
static short userShellRead(char *data, unsigned short len)
{
    return (short)comGetBuf(COM1, (uint8_t *)data, len);
}

/**
 * @brief 用户shell初始化
 *
 */
void userInitShell(void)
{
    shell.write = userShellWrite;
    shell.read = userShellRead;
    // shell.lock = userShellLock;
    // shell.unlock = userShellUnlock;
    shellInit(&shell, shellBuffer, (unsigned short)sizeof(shellBuffer));
}
