/**
 * @file    bsp_user_lib.c
 * @brief   通用工具函数库（字符串操作、大小端转换、CRC16-Modbus）
 */

#include "bsp.h"

// CRC 高位字节值表
static const uint8_t s_CRCHi[] = {
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0,
    0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
    0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0,
    0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40,
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1,
    0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41,
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1,
    0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0,
    0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40,
    0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1,
    0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40,
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0,
    0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40,
    0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0,
    0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40,
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0,
    0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
    0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0,
    0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
    0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0,
    0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40,
    0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1,
    0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0,
    0x80, 0x41, 0x00, 0xC1, 0x81, 0x40};
// CRC 低位字节值表
const uint8_t s_CRCLo[] = {
    0x00, 0xC0, 0xC1, 0x01, 0xC3, 0x03, 0x02, 0xC2, 0xC6, 0x06,
    0x07, 0xC7, 0x05, 0xC5, 0xC4, 0x04, 0xCC, 0x0C, 0x0D, 0xCD,
    0x0F, 0xCF, 0xCE, 0x0E, 0x0A, 0xCA, 0xCB, 0x0B, 0xC9, 0x09,
    0x08, 0xC8, 0xD8, 0x18, 0x19, 0xD9, 0x1B, 0xDB, 0xDA, 0x1A,
    0x1E, 0xDE, 0xDF, 0x1F, 0xDD, 0x1D, 0x1C, 0xDC, 0x14, 0xD4,
    0xD5, 0x15, 0xD7, 0x17, 0x16, 0xD6, 0xD2, 0x12, 0x13, 0xD3,
    0x11, 0xD1, 0xD0, 0x10, 0xF0, 0x30, 0x31, 0xF1, 0x33, 0xF3,
    0xF2, 0x32, 0x36, 0xF6, 0xF7, 0x37, 0xF5, 0x35, 0x34, 0xF4,
    0x3C, 0xFC, 0xFD, 0x3D, 0xFF, 0x3F, 0x3E, 0xFE, 0xFA, 0x3A,
    0x3B, 0xFB, 0x39, 0xF9, 0xF8, 0x38, 0x28, 0xE8, 0xE9, 0x29,
    0xEB, 0x2B, 0x2A, 0xEA, 0xEE, 0x2E, 0x2F, 0xEF, 0x2D, 0xED,
    0xEC, 0x2C, 0xE4, 0x24, 0x25, 0xE5, 0x27, 0xE7, 0xE6, 0x26,
    0x22, 0xE2, 0xE3, 0x23, 0xE1, 0x21, 0x20, 0xE0, 0xA0, 0x60,
    0x61, 0xA1, 0x63, 0xA3, 0xA2, 0x62, 0x66, 0xA6, 0xA7, 0x67,
    0xA5, 0x65, 0x64, 0xA4, 0x6C, 0xAC, 0xAD, 0x6D, 0xAF, 0x6F,
    0x6E, 0xAE, 0xAA, 0x6A, 0x6B, 0xAB, 0x69, 0xA9, 0xA8, 0x68,
    0x78, 0xB8, 0xB9, 0x79, 0xBB, 0x7B, 0x7A, 0xBA, 0xBE, 0x7E,
    0x7F, 0xBF, 0x7D, 0xBD, 0xBC, 0x7C, 0xB4, 0x74, 0x75, 0xB5,
    0x77, 0xB7, 0xB6, 0x76, 0x72, 0xB2, 0xB3, 0x73, 0xB1, 0x71,
    0x70, 0xB0, 0x50, 0x90, 0x91, 0x51, 0x93, 0x53, 0x52, 0x92,
    0x96, 0x56, 0x57, 0x97, 0x55, 0x95, 0x94, 0x54, 0x9C, 0x5C,
    0x5D, 0x9D, 0x5F, 0x9F, 0x9E, 0x5E, 0x5A, 0x9A, 0x9B, 0x5B,
    0x99, 0x59, 0x58, 0x98, 0x88, 0x48, 0x49, 0x89, 0x4B, 0x8B,
    0x8A, 0x4A, 0x4E, 0x8E, 0x8F, 0x4F, 0x8D, 0x4D, 0x4C, 0x8C,
    0x44, 0x84, 0x85, 0x45, 0x87, 0x47, 0x46, 0x86, 0x82, 0x42,
    0x43, 0x83, 0x41, 0x81, 0x80, 0x40};

/**
 * @brief  计算以 '\0' 结尾的字符串长度
 * @param  _str  字符串指针
 * @retval 字符串长度（不含结束符）
 */
int str_len(char *_str)
{
    int len = 0;

    while (*_str++)
        len++;
    return len;
}

/**
 * @brief  复制字符串（含结束符 '\0'）
 * @param  _tar  目标缓冲区
 * @param  _src  源缓冲区
 * @retval 无
 */
void str_cpy(char *_tar, char *_src)
{
    do
    {
        *_tar++ = *_src;
    } while (*_src++);
}

/**
 * @brief  比较两个字符串
 * @param  s1  字符串 1
 * @param  s2  字符串 2
 * @retval 0 = 相等，非 0 = 不等
 */
int str_cmp(char *s1, char *s2)
{
    while ((*s1 != 0) && (*s2 != 0) && (*s1 == *s2))
    {
        s1++;
        s2++;
    }
    return *s1 - *s2;
}

/**
 * @brief  填充内存区域（类似 memset）
 * @param  _tar   目标缓冲区
 * @param  _data  填充字节值
 * @param  _len   填充长度
 * @retval 无
 */
void mem_set(char *_tar, char _data, int _len)
{
    while (_len--)
    {
        *_tar++ = _data;
    }
}

/**
 * @brief  将整数转换为定长 ASCII 字符串（支持负数，不足左侧补空格）
 * @param  _iNumber  输入整数（支持负数）
 * @param  _pBuf     输出缓冲区（_len+1 字节，以 '\0' 结束）
 * @param  _len      字符个数（不含结束符）
 * @retval 无
 */
void int_to_str(int _iNumber, char *_pBuf, unsigned char _len)
{
    unsigned char i;
    int iTemp;

    if (_iNumber < 0) /* 负数 */
    {
        iTemp = -_iNumber; /* 转为正数 */
    }
    else
    {
        iTemp = _iNumber;
    }

    mem_set(_pBuf, ' ', _len);

    /* 将整数转换为ASCII字符串 */
    for (i = 0; i < _len; i++)
    {
        _pBuf[_len - 1 - i] = (iTemp % 10) + '0';
        iTemp = iTemp / 10;
        if (iTemp == 0)
        {
            break;
        }
    }
    _pBuf[_len] = 0;

    if (_iNumber < 0) /* 负数 */
    {
        for (i = 0; i < _len; i++)
        {
            if ((_pBuf[i] == ' ') && (_pBuf[i + 1] != ' '))
            {
                _pBuf[i] = '-';
                break;
            }
        }
    }
}

/**
 * @brief  将 ASCII 数字字符串转换为整数（支持负号，遇小数点自动跳过）
 * @param  _pStr  待转换字符串（以非数字字符或 '\0' 结束）
 * @retval 对应的整数值
 */
int str_to_int(char *_pStr)
{
    unsigned char flag;
    char *p;
    int ulInt;
    unsigned char i;
    unsigned char ucTemp;

    p = _pStr;
    if (*p == '-')
    {
        flag = 1; /* 负数 */
        p++;
    }
    else
    {
        flag = 0;
    }

    ulInt = 0;
    for (i = 0; i < 15; i++)
    {
        ucTemp = *p;
        if (ucTemp == '.') /* 遇到小数点，自动跳过1个字节 */
        {
            p++;
            ucTemp = *p;
        }
        if ((ucTemp >= '0') && (ucTemp <= '9'))
        {
            ulInt = ulInt * 10 + (ucTemp - '0');
            p++;
        }
        else
        {
            break;
        }
    }

    if (flag == 1)
    {
        return -ulInt;
    }
    return ulInt;
}

/**
 * @brief  大端 2 字节数组转 uint16_t（高字节在前）
 * @param  _pBuf  2 字节输入数组
 * @retval uint16_t 整数值
 */
uint16_t BEBufToUint16(uint8_t *_pBuf)
{
    return (((uint16_t)_pBuf[0] << 8) | _pBuf[1]);
}

/**
 * @brief  小端 2 字节数组转 uint16_t（低字节在前）
 * @param  _pBuf  2 字节输入数组
 * @retval uint16_t 整数值
 */
uint16_t LEBufToUint16(uint8_t *_pBuf)
{
    return (((uint16_t)_pBuf[1] << 8) | _pBuf[0]);
}

/**
 * @brief  大端 4 字节数组转 uint32_t（高字节在前）
 * @param  _pBuf  4 字节输入数组
 * @retval uint32_t 整数值
 */
uint32_t BEBufToUint32(uint8_t *_pBuf)
{
    return (((uint32_t)_pBuf[0] << 24) | ((uint32_t)_pBuf[1] << 16) | ((uint32_t)_pBuf[2] << 8) | _pBuf[3]);
}

/**
 * @brief  小端 4 字节数组转 uint32_t（低字节在前）
 * @param  _pBuf  4 字节输入数组
 * @retval uint32_t 整数值
 */
uint32_t LEBufToUint32(uint8_t *_pBuf)
{
    return (((uint32_t)_pBuf[3] << 24) | ((uint32_t)_pBuf[2] << 16) | ((uint32_t)_pBuf[1] << 8) | _pBuf[0]);
}

/**
 * @brief  计算 Modbus CRC-16（查表法，内部已交换高低字节，返回值可直接附加到报文）
 * @param  _pBuf   参与校验的数据缓冲区
 * @param  _usLen  数据长度（字节）
 * @retval CRC16 值（高字节在高位）
 */
uint16_t CRC16_Modbus(uint8_t *_pBuf, uint16_t _usLen)
{
    uint8_t ucCRCHi = 0xFF; /* 高CRC字节初始化 */
    uint8_t ucCRCLo = 0xFF; /* 低CRC 字节初始化 */
    uint16_t usIndex;       /* CRC循环中的索引 */

    while (_usLen--)
    {
        usIndex = ucCRCHi ^ *_pBuf++; /* 计算CRC */
        ucCRCHi = ucCRCLo ^ s_CRCHi[usIndex];
        ucCRCLo = s_CRCLo[usIndex];
    }
    return ((uint16_t)ucCRCHi << 8 | ucCRCLo);
}

/**
 * @brief  线性插值：根据两点直线方程求给定 x 对应的 y 值
 * @param  x1, y1  第一个已知点坐标
 * @param  x2, y2  第二个已知点坐标
 * @param  x       输入 x 值
 * @retval x 对应的 y 值（int32_t）
 */
int32_t CaculTwoPoint(int32_t x1, int32_t y1, int32_t x2, int32_t y2, int32_t x)
{
    return y1 + ((int64_t)(y2 - y1) * (x - x1)) / (x2 - x1);
}

/**
 * @brief  将单个 BCD 半字节（0–15）转换为对应 ASCII 字符（'0'–'9'，'A'–'F'）
 * @param  _bcd  输入值（必须 < 16）
 * @retval 对应 ASCII 字符，超范围返回 0
 */
char BcdToChar(uint8_t _bcd)
{
    if (_bcd < 10)
    {
        return _bcd + '0';
    }
    else if (_bcd < 16)
    {
        return _bcd + 'A';
    }
    else
    {
        return 0;
    }
}

/**
 * @brief  将二进制字节数组转换为十六进制 ASCII 字符串（每字节 2 字符 + 1 空格，末尾 '\0'）
 * @param  _pHex      输入二进制数组
 * @param  _pAscii    输出 ASCII 缓冲区（需 3*_BinBytes 字节）
 * @param  _BinBytes  输入字节数
 * @retval 无
 */
void HexToAscll(uint8_t *_pHex, char *_pAscii, uint16_t _BinBytes)
{
    uint16_t i;

    if (_BinBytes == 0)
    {
        _pAscii[0] = 0;
    }
    else
    {
        for (i = 0; i < _BinBytes; i++)
        {
            _pAscii[3 * i] = BcdToChar(_pHex[i] >> 4);
            _pAscii[3 * i + 1] = BcdToChar(_pHex[i] & 0x0F);
            _pAscii[3 * i + 2] = ' ';
        }
        _pAscii[3 * (i - 1) + 2] = 0;
    }
}

/**
 * @brief  将 ASCII 字符串转换为 uint32_t（支持十进制和 "0x" 前缀十六进制）
 * @param  pAscii  输入字符串（以空格或 '\0' 结束）
 * @retval 对应的 uint32_t 整数值
 */
uint32_t AsciiToUint32(char *pAscii)
{
    char i;
    char bTemp;
    char bIsHex;
    char bLen;
    char bZeroLen;
    uint32_t lResult;
    uint32_t lBitValue;

    /* 判断是否是16进制数 */
    bIsHex = 0;
    if ((pAscii[0] == '0') && ((pAscii[1] == 'x') || (pAscii[1] == 'X')))
    {
        bIsHex = 1;
    }

    lResult = 0;
    // 最大数值为 4294967295, 10位+2字符"0x" //
    if (bIsHex == 0)
    { // 十进制 //
        // 求长度 //
        lBitValue = 1;

        /* 前导去0 */
        for (i = 0; i < 8; i++)
        {
            bTemp = pAscii[i];
            if (bTemp != '0')
                break;
        }
        bZeroLen = i;

        for (i = 0; i < 10; i++)
        {
            if ((pAscii[i] < '0') || (pAscii[i] > '9'))
                break;
            lBitValue = lBitValue * 10;
        }
        bLen = i;
        lBitValue = lBitValue / 10;
        if (lBitValue == 0)
            lBitValue = 1;
        for (i = bZeroLen; i < bLen; i++)
        {
            lResult += (pAscii[i] - '0') * lBitValue;
            lBitValue /= 10;
        }
    }
    else
    { /* 16进制 */
        /* 求长度 */
        lBitValue = 1;

        /* 前导去0 */
        for (i = 0; i < 8; i++)
        {
            bTemp = pAscii[i + 2];
            if (bTemp != '0')
                break;
        }
        bZeroLen = i;
        for (; i < 8; i++)
        {
            bTemp = pAscii[i + 2];
            if (((bTemp >= 'A') && (bTemp <= 'F')) ||
                ((bTemp >= 'a') && (bTemp <= 'f')) ||
                ((bTemp >= '0') && (bTemp <= '9')))
            {
                lBitValue = lBitValue * 16;
            }
            else
            {
                break;
            }
        }
        lBitValue = lBitValue / 16;
        if (lBitValue == 0)
            lBitValue = 1;
        bLen = i;
        for (i = bZeroLen; i < bLen; i++)
        {
            bTemp = pAscii[i + 2];
            if ((bTemp >= 'A') && (bTemp <= 'F'))
            {
                bTemp -= 0x37;
            }
            else if ((bTemp >= 'a') && (bTemp <= 'f'))
            {
                bTemp -= 0x57;
            }
            else if ((bTemp >= '0') && (bTemp <= '9'))
            {
                bTemp -= '0';
            }
            lResult += bTemp * lBitValue;
            lBitValue /= 16;
        }
    }
    return lResult;
}

/**
 * @brief  带成功校验的 sscanf 封装
 * @param  count  期望成功转换的字段数
 * @param  buf    输入字符串
 * @param  fmt    格式字符串
 * @retval 1 = 转换成功，0 = 转换失败
 */
uint8_t checked_sscanf(int count, const char *buf, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int rc = vsscanf(buf, fmt, ap);
    va_end(ap);
    return rc == count;
}

/**
 * @brief  BCD 码转十进制（如 0x58 → 58）
 * @param  _bcd  压缩 BCD 字节（输入值须合法）
 * @retval 十进制值
 */
uint8_t lib_bcd2dec(uint8_t _bcd)
{
    return (((_bcd >> 4) & 0x0F) * 10) + (_bcd & 0x0F);
}

/**
 * @brief  十进制转 BCD 码（如 99 → 0x99），超出 99 自动截断
 * @param  _dec  十进制值（0–99）
 * @retval 压缩 BCD 字节
 */
uint8_t lib_dec2bcd(uint8_t _dec)
{
    return (_dec % 10) + ((_dec / 10) % 10) * 16;
}

/* end of file */
