/*

 * Copyright (c) 2006-2018, RT-Thread Development Team
    *
 * SPDX-License-Identifier: Apache-2.0
    *
 * Change Logs:
 * Date           Author       Notes
    */
#ifndef _RING_BUFFER_H
#define _RING_BUFFER_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "stdint.h"
#include "stdio.h"
#include "string.h"

#ifndef RT_ASSERT
#if 0 // 1 启用断言 0 禁用断言
#define RT_ASSERT(EX)                                                                                   \
    if (!(EX))                                                                                          \
    {                                                                                                   \
        printf("(%s) assertion failed at function:%s, line number:%d \n", #EX, __FUNCTION__, __LINE__); \
    }
#else
#define RT_ASSERT(EX)
#endif //#if *
#endif // RT_ASSERT END

#ifndef RTM_EXPORT
#define RTM_EXPORT(symbol)
#endif // RTM_EXPORT END

#ifndef RT_ALIGN_DOWN
#define RT_ALIGN_DOWN(size, align) ((size) & ~((align)-1))
#endif

#ifndef RT_ALIGN_SIZE
#define RT_ALIGN_SIZE 4
#endif

    /* ring buffer */
    typedef struct ringbuffer
    {
        uint8_t *buffer_ptr;
        /* use the msb of the {read,write}_index as mirror bit. You can see this as
         * if the buffer adds a virtual mirror and the pointers point either to the
         * normal or to the mirrored buffer. If the write_index has the same value
         * with the read_index, but in a different mirror, the buffer is full.
         * While if the write_index and the read_index are the same and within the
         * same mirror, the buffer is empty. The ASCII art of the ringbuffer is:
         *
         *          mirror = 0                    mirror = 1
         * +---+---+---+---+---+---+---+|+~~~+~~~+~~~+~~~+~~~+~~~+~~~+
         * | 0 | 1 | 2 | 3 | 4 | 5 | 6 ||| 0 | 1 | 2 | 3 | 4 | 5 | 6 | Full
         * +---+---+---+---+---+---+---+|+~~~+~~~+~~~+~~~+~~~+~~~+~~~+
         *  read_idx-^                   write_idx-^
         *
         * +---+---+---+---+---+---+---+|+~~~+~~~+~~~+~~~+~~~+~~~+~~~+
         * | 0 | 1 | 2 | 3 | 4 | 5 | 6 ||| 0 | 1 | 2 | 3 | 4 | 5 | 6 | Empty
         * +---+---+---+---+---+---+---+|+~~~+~~~+~~~+~~~+~~~+~~~+~~~+
         * read_idx-^ ^-write_idx
         *
         * The tradeoff is we could only use 32KiB of buffer for 16 bit of index.
         * But it should be enough for most of the cases.
         *
         * Ref: http://en.wikipedia.org/wiki/Circular_buffer#Mirroring */
        uint16_t read_mirror : 1;
        uint16_t read_index : 15;
        uint16_t write_mirror : 1;
        uint16_t write_index : 15;
        /* as we use msb of index as mirror bit, the size should be signed and
         * could only be positive. */
        uint16_t buffer_size;
    }RINGBUFF_T;

    enum ringbuffer_state
    {
        RT_RINGBUFFER_EMPTY,
        RT_RINGBUFFER_FULL,
        /* half full is neither full nor empty */
        RT_RINGBUFFER_HALFFULL,
    };

    /**
     * RingBuffer for DeviceDriver
     *
     * Please note that the ring buffer implementation of RT-Thread
     * has no thread wait or resume feature.
     */
    void ringbuffer_init(struct ringbuffer *rb, uint8_t *pool, uint16_t size);
    void ringbuffer_reset(struct ringbuffer *rb);
    unsigned long ringbuffer_put(struct ringbuffer *rb, const uint8_t *ptr, uint16_t length);
    unsigned long ringbuffer_put_force(struct ringbuffer *rb, const uint8_t *ptr, uint16_t length);
    unsigned long ringbuffer_putchar(struct ringbuffer *rb, const uint8_t ch);
    unsigned long ringbuffer_putchar_force(struct ringbuffer *rb, const uint8_t ch);
    unsigned long ringbuffer_get(struct ringbuffer *rb, uint8_t *ptr, uint16_t length);
    unsigned long ringbuffer_getchar(struct ringbuffer *rb, uint8_t *ch);
    unsigned long ringbuffer_data_len(struct ringbuffer *rb);

    static __inline uint16_t ringbuffer_get_size(struct ringbuffer *rb)
    {
        RT_ASSERT(rb != NULL);
        return rb->buffer_size;
    }

/** return the size of empty space in rb */
#define ringbuffer_space_len(rb) ((rb)->buffer_size - ringbuffer_data_len(rb))

#ifdef __cplusplus
}
#endif

#endif //_RING_BUFFER_H
