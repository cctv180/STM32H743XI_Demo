// https://github.com/zhicheng/base64 test

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bsp.h"
#include "dump_hex.h"
#include "base64.h"
#include "xxtea.h"

uint8_t *sp_key = NULL;

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

// 通用加密函数
static int encrypt(int argc, char *argv[], int is_b64)
{
    if (argc <= 3)
    {
        printError("Parameter error.");
        printUsage(argv[0], is_b64 ? "encrypt b64 string" : "encrypt str xxx string");
        return -1;
    }

    uint16_t size = strlen(argv[3]);
    size_t len;
    char *buf_b64 = 0;

    if (is_b64)
    {
        /* base64模式 */
        buf_b64 = (char *)base64_decode((const char *)argv[3], &len);
        if (buf_b64 == NULL)
        {
            printError("base64 decode failed.");
            return -1;
        }
        printf("base64 decode success. in size = %d out length = %d", size, len);
        size = len;                                 // 实际长度
        dump_hex(buf_b64, size, size > 8 ? 16 : 8); // 打印数据块
        printf("\r\n");
    }
    else
    {
        /* string模式 */
        buf_b64 = argv[3];
    }

    /* XXTEA加密 */
    char *buff = xxtea_encrypt(buf_b64, size, sp_key, &len);
    if (buff == NULL)
    {
        printError("xxtea encrypt failed.");
        if (is_b64)
        {
            free(buf_b64); // free memory
        }
        return -1;
    }
    printf("xxtea encrypt success. in size = %d out length = %d", size, len);
    size = len;                              // 实际长度
    dump_hex(buff, size, size > 8 ? 16 : 8); // 打印数据块
    printf("\r\n");

    /* base64编码 */
    if (is_b64)
    {
        free(buf_b64); // free memory
    }
    buf_b64 = base64_encode((const uint8_t *)buff, size);
    if (buf_b64 == NULL)
    {
        printError("base64 encode failed.");

        return -1;
    }
    len = strlen(buf_b64);
    printf("base64 encode success. in size = %d out length = %d", size, len);
    size = len;                                 // 实际长度
    dump_hex(buf_b64, size, size > 8 ? 16 : 8); // 打印数据块
    printf("\r\n");

    /* 释放内存 */
    free(buf_b64);
    free(buff);

    return 0;
}

// 解密
static int decrypt(int argc, char *argv[])
{
    if (argc <= 2)
    {
        printError("Parameter error.");
        printUsage(argv[0], "decrypt base64 string");
        return -1;
    }

    /* base64解码逻辑 */
    size_t size = strlen(argv[2]);
    size_t len;
    uint8_t *buff = base64_decode(argv[2], &len);
    if (buff == NULL)
    {
        printError("base64 decode failed.");
        return -1;
    }
    printf("base64 decode success. in size = %d out length = %d", size, len);
    size = len;                              // 实际长度
    dump_hex(buff, size, size > 8 ? 16 : 8); // 打印数据块
    printf("\r\n");

    /* xxtea解密*/
    char *buff_tea = xxtea_decrypt(buff, size, sp_key, &len);
    if (buff_tea == NULL)
    {
        printError("xxtea decrypt failed.");

        free(buff); // free memory
        return -1;
    }
    printf("xxtea decrypt success. in size = %d out length = %d", size, len);
    size = len;                                  // 实际长度
    dump_hex(buff_tea, size, size > 8 ? 16 : 8); // 打印数据块
    printf("\r\n");

    /* 释放内存 */
    free(buff_tea);
    free(buff);

    return 0;
}

#if defined(__SHELL_H__) && defined(DEBUG_MODE)
static int _cmd(int argc, char *argv[])
{
    if (argc < 2)
    {
        printError("Invalid command.");
        printUsage(argv[0], "set/encrypt/decrypt");
        return -1;
    }

    if (strcmp(argv[1], "encrypt") == 0)
    {
        if (argc < 3)
        {
            printError("Missing encoding type.");
            printUsage(argv[0], "encrypt str/b64");
            return -1;
        }

        if (strcmp(argv[2], "b64") == 0)
        {
            return encrypt(argc, argv, 1);
        }
        else if (strcmp(argv[2], "str") == 0)
        {
            return encrypt(argc, argv, 0);
        }
        else
        {
            printError("Invalid encoding type.");
            printUsage(argv[0], "encrypt str/b64");
            return -1;
        }
    }
    else if (strcmp(argv[1], "decrypt") == 0)
    {
        return decrypt(argc, argv);
    }
    else if (strcmp(argv[1], "set") == 0)
    {
        if (argc <= 2)
        {
            if (sp_key != NULL)
            {
                printf("key = %s\r\n\r\n", sp_key);
                return 0;
            }

            printError("Missing set key string.");
            printUsage(argv[0], "set \"key string\"");
            return -1;
        }

        size_t size = strlen(argv[2]);
        free(sp_key);
        sp_key = malloc(size + 1); // add '\0'
        if (sp_key == NULL)
        {
            printError("Key Memory allocation failed.");
            return -1;
        }
        sp_key[size] = '\0';
        memcpy(sp_key, argv[2], size);
        printf("Key set success.");
        dump_hex(sp_key, size, size > 8 ? 16 : 8); // 打印数据块
        printf("\r\n");

        return 0;
    }
    else
    {
        printError("Invalid command.");
        printUsage(argv[0], "set/encrypt/decrypt");
        return -1;
    }

    return 99;
}

// 导出到命令列表里
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), xxtea, _cmd, xxtea[set encrypt decrypt]);
#endif // #ifdef DEBUG_MODE
