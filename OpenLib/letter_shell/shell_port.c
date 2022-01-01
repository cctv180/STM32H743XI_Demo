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
char shellBuffer[512];

#ifdef DEBUG_MODE
// shell导出到命令列表里
//                       权限设置                                命令类型            命令名称      命令实体      命令说明
// SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_FUNC), ef_print_env, ef_print_env, ef print env);

int test2(int i1, int i2, int i3, int i4, int i5, int i6, int i7, int i8)
{
    printf("input int: %d, %d, %d, %d, %d, %d, %d, %d\r\n", i1, i2, i3, i4, i5, i6, i7, i8);
    return 0;
}
//导出到命令列表里
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_FUNC), test2, test2, test2);

/* SHELL_USING_CMD_EXPORT == 1需要把这个打开 */
int testfunc(int argc, char *agrv[])
{
    printf("%dparameter(s)\r\n", argc);
    for (char i = 1; i < argc; i++)
    {
        printf("%s\r\n", agrv[i]);
    }
    return 0;
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), testfunc, testfunc, testfunc);

/* 系统信息 */
int sys_info(int argc, char *agrv[])
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

/* 生成随机数 */
// int rand(int argc, char* agrv[])
//{
//     extern RNG_HandleTypeDef hrng;
//     uint32_t temp;
//
//     temp = HAL_RNG_GetRandomNumber(&hrng);
//     printf("Randon Number = 0x%08x\r\n", temp);
//     return 0;
// }
// SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN, rand, rand, rand);

#endif

/* 系统重启 */
int reboot(int argc, char *agrv[])
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
short userShellWrite(char *data, unsigned short len)
{

    comSendBuf(COM1, (uint8_t *)data, len);
    return len;
}

/**
 * @brief 用户shell读
 *
 * @param data 数据
 * @param len 数据长度
 *
 * @return short 实际读取到
 */
short userShellRead(char *data, unsigned short len)
{
    return comGetBuf(COM1, (uint8_t *)data, len);
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
    shellInit(&shell, shellBuffer, 512);
}