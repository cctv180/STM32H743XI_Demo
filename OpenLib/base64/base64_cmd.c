// https://github.com/zhicheng/base64 test

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bsp.h"
#include "dump_hex.h"
#include "base64.h"

// 显示用法说明
static void printUsage(const char *programName, const char *cmd)
{
    printf("Error: Invalid command.\r\nUsage:\r\n");
    printf("%s %s\r\n", programName, cmd);
    printf("\r\n");
}

// 显示错误消息
static void printError(const char *message)
{
    printf("%s\r\n", message);
}

// 通用编码函数
static int encode(int argc, char *argv[], int isHex)
{
    if (argc <= 3)
    {
        printError("Parameter error.");
        printUsage(argv[0], isHex ? "encode hex \"0x** 0x** ...\"" : "encode str xxx string");
        return -1;
    }

    uint16_t size = strlen(argv[3]);

    if (isHex)
    {
        /* HEX base64编码逻辑 */
        uint8_t spaceCount = 0;

        /* 计算空格数 */
        for (size_t i = 0; i < size; i++)
        {
            if (argv[3][i] == ' ')
            {
                spaceCount++;
            }
        }
        spaceCount += 2; // 至少2个数用于申请内存
        // printf("spaceCount = %d\r\n", spaceCount);

        /* 解码hex字符串 */
        char *buff_hex = malloc(spaceCount); // 申请内存
        if (!buff_hex)
        {
            printf("buff_hex Low memory! size = %d\r\n", spaceCount);

            return -1;
        }
        char *p_end = argv[3];
        char *p_old = 0;
        size = 0; // 初始化hex长度
        for (size_t i = 0; i < spaceCount; i++)
        {
            // 使用strtol函数将字符串转换为整数，并存储到buff_hex中
            buff_hex[i] = strtol(p_end, &p_end, 0);

            if (p_old != p_end)
            {
                p_old = p_end;
                size++; // 更新hex长度
            }
            else
            {
                printf("\r\nhex decode success.");
                dump_hex(buff_hex, size, size > 8 ? 16 : 8); // 打印数据块
                break;
            }
        }

        /* base64编码 */
        char *buff = malloc(BASE64_ENCODE_OUT_SIZE(size)); // 申请内存
        if (!buff)
        {
            printf("buff Low memory! size = %d\r\n", size);
            /* 释放内存 */
            free(buff_hex);

            return -1;
        }
        uint16_t len;
        len = base64_encode((const uint8_t *)buff_hex, size, buff);
        printf("\r\nbase64 encode buff success.\r\n");
        printf("in size = %d out length = %d", size, len);
        size = len;                              // 实际长度
        dump_hex(buff, size, size > 8 ? 16 : 8); // 打印数据块
        printf("\r\n");

        /* 释放内存 */
        free(buff_hex);
        free(buff);
    }
    else
    {
        /* 字符串base64编码逻辑 */
        char *buff = malloc(BASE64_ENCODE_OUT_SIZE(size)); // 申请内存
        if (!buff)
        {
            printf("buff Low memory! size = %d\r\n", size);

            return -1;
        }
        uint16_t len;
        len = base64_encode((const uint8_t *)argv[3], size, buff);
        printf("\r\nbase64 encode buff success.\r\n");
        printf("in size = %d out length = %d", size, len);
        size = len;                              // 实际长度
        dump_hex(buff, size, size > 8 ? 16 : 8); // 打印数据块
        printf("\r\n");

        /* 释放内存 */
        free(buff);
    }

    return 0;
}

// 解码
static int decode(int argc, char *argv[])
{
    if (argc <= 2)
    {
        printError("Parameter error.");
        printUsage(argv[0], "decode base64 string");
        return -1;
    }

    /* base64解码逻辑 */
    uint16_t size = strlen(argv[2]);
    char *buff = malloc(BASE64_DECODE_OUT_SIZE(size));
    if (!buff)
    {
        printf("Low memory! size = %d\r\n", size);

        return -1;
    }

    uint16_t len = base64_decode((const char *)argv[2], size, (uint8_t *)buff);
    if (!len)
    {
        printf("base64 decode failed.\r\n");

        /* 释放内存 */
        free(buff);

        return -1;
    }

    printf("base64 decode buff success. in size = %d out length = %d\r\n", size, len);
    size = len;                              // 实际长度
    dump_hex(buff, size, size > 8 ? 16 : 8); // 打印数据块

    /* 释放内存 */
    free(buff);

    return 0;
}

#if defined(__SHELL_H__) && defined(DEBUG_MODE)
static int _cmd(int argc, char *argv[])
{
    if (argc < 2)
    {
        printError("Invalid command.");
        printUsage(argv[0], "encode/decode");
        return -1;
    }

    if (strcmp(argv[1], "encode") == 0)
    {
        if (argc < 3)
        {
            printError("Missing encoding type.");
            printUsage(argv[0], "encode hex/str");
            return -1;
        }

        if (strcmp(argv[2], "hex") == 0)
        {
            return encode(argc, argv, 1);
        }
        else if (strcmp(argv[2], "str") == 0)
        {
            return encode(argc, argv, 0);
        }
        else
        {
            printError("Invalid encoding type.");
            printUsage(argv[0], "encode hex/str");
            return -1;
        }
    }
    else if (strcmp(argv[1], "decode") == 0)
    {
        return decode(argc, argv);
    }
    else
    {
        printError("Invalid command.");
        printUsage(argv[0], "encode/decode");
        return -1;
    }

    return 0;
}

// 导出到命令列表里
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), base64, _cmd, base64[encode decode]);
#endif // #ifdef DEBUG_MODE
