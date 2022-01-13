/*
*********************************************************************************************************
*
*    模块名称 : 字符串操作\数值转换
*    文件名称 : bsp_user_lib.h
*    版    本 : V1.2
*    说    明 : 头文件
*
*********************************************************************************************************
*/

#ifndef __BSP_USER_LIB_H
#define __BSP_USER_LIB_H

int str_len(char *_str);
void str_cpy(char *_tar, char *_src);
int str_cmp(char *s1, char *s2);
void mem_set(char *_tar, char _data, int _len);

void int_to_str(int _iNumber, char *_pBuf, unsigned char _len);
int str_to_int(char *_pStr);

uint16_t BEBufToUint16(uint8_t *_pBuf);
uint16_t LEBufToUint16(uint8_t *_pBuf);

uint32_t BEBufToUint32(uint8_t *_pBuf);
uint32_t LEBufToUint32(uint8_t *_pBuf);

uint16_t CRC16_Modbus(uint8_t *_pBuf, uint16_t _usLen);
int32_t CaculTwoPoint(int32_t x1, int32_t y1, int32_t x2, int32_t y2, int32_t x);

char BcdToChar(uint8_t _bcd);
void HexToAscll(uint8_t *_pHex, char *_pAscii, uint16_t _BinBytes);
uint32_t AsciiToUint32(char *pAscii);

uint8_t checked_sscanf(int count, const char *buf, const char *fmt, ...);

uint8_t lib_bcd2dec(uint8_t _bcd);
uint8_t lib_dec2bcd(uint8_t _dec);

static __inline__ uint32_t roundup_pow_of_two(uint32_t _num) /* 找出最接近 最大2的指数次幂 */
{
#if 1
    //启用 STM32 硬件提供的计算前导零指令 CLZ
    if (_num != 0)
    {
        // return (0x80000000UL >> (__clz(_num) - 1)); //向上取整为2次幂
        return (0x80000000UL >> __clz(_num)); //向下取整为2次幂
    }
    return 0;
#else
    if (_num <= 0)
        return 1;
    if ((_num & (_num - 1)) == 0)
        return _num;
    _num |= _num >> 1;
    _num |= _num >> 2;
    _num |= _num >> 4;
    _num |= _num >> 8;
    _num |= _num >> 16;
    // return _num + 1; //向上取整为2次幂
    return (_num + 1) >> 1; //向下取整为2次幂
#endif
}

#ifndef USER_ALIGN
#define USER_ALIGN(size, align) (((size) + (align)-1) & ~((align)-1)) /* size 向上取align的整数倍 */
#endif

#ifndef USER_ALIGN_DOWN
#define USER_ALIGN_DOWN(size, align) ((size) & ~((align)-1)) /* size 向下取align的整数倍 */
#endif

#endif
/***************************** 安富莱电子 www.armfly.com (END OF FILE) *********************************/
