/*
*********************************************************************************************************
*
*    模块名称 : 独立按键驱动模块 (外部输入IO)
*    文件名称 : bsp_key.c
*    版    本 : V1.3
*    说    明 : 扫描独立按键，具有软件滤波机制，具有按键FIFO。可以检测如下事件：
*                (1) 按键按下
*                (2) 按键弹起
*                (3) 长按键
*                (4) 长按时自动连发
*
*    修改记录 :
*        版本号  日期        作者     说明
*        V1.0    2013-02-01 armfly  正式发布
*        V1.1    2013-06-29 armfly  增加1个读指针，用于bsp_Idle() 函数读取系统控制组合键（截屏）
*                                   增加 K1 K2 组合键 和 K2 K3 组合键，用于系统控制
*        V1.2    2016-01-25 armfly  针对P02工控板更改. 调整gpio定义方式，更加简洁
*        V1.3    2018-11-26 armfly  s_tBtn结构赋初值0
*
*    Copyright (C), 2016-2020, 安富莱电子 www.armfly.com
*
*********************************************************************************************************
*/

#include "bsp.h"
#include "bsp_key.h"
#include "multi_button.h"
#include "ring_buffer.h"

#define HARD_KEY_NUM 8               /* 实体按键个数 */
#define KEY_COUNT (HARD_KEY_NUM + 2) /* 8个独立建 + 2个组合按键 */

/* 使能GPIO时钟 */
#define ALL_KEY_GPIO_CLK_ENABLE()     \
    {                                 \
        __HAL_RCC_GPIOB_CLK_ENABLE(); \
        __HAL_RCC_GPIOC_CLK_ENABLE(); \
        __HAL_RCC_GPIOG_CLK_ENABLE(); \
        __HAL_RCC_GPIOH_CLK_ENABLE(); \
        __HAL_RCC_GPIOI_CLK_ENABLE(); \
    };

/* 依次定义GPIO */
typedef struct
{
    GPIO_TypeDef *gpio;
    uint16_t pin;
    uint8_t ActiveLevel; /* 激活电平 */
} X_GPIO_T;

/* GPIO和PIN定义 */
static const X_GPIO_T s_gpio_list[HARD_KEY_NUM] = {
    {GPIOI, GPIO_PIN_8, 0},  /* K1 */
    {GPIOC, GPIO_PIN_13, 0}, /* K2 */
    {GPIOH, GPIO_PIN_4, 0},  /* K3 */
    {GPIOG, GPIO_PIN_2, 0},  /* JOY_U */
    {GPIOB, GPIO_PIN_0, 0},  /* JOY_D */
    {GPIOG, GPIO_PIN_3, 0},  /* JOY_L */
    {GPIOG, GPIO_PIN_7, 0},  /* JOY_R */
    {GPIOI, GPIO_PIN_11, 0}, /* JOY_OK */
};

/* 定义一个宏函数简化后续代码
    判断GPIO引脚是否有效按下
*/
static Button s_tButton[KEY_COUNT] = {0};
static uint8_t s_buf[KEY_FIFO_SIZE] = {0};
static RINGBUFF_T s_key_kfifo;

static void bsp_InitKeyVar(void);
static void bsp_InitKeyHard(void);
static void button_callback(void *button);

/*
*********************************************************************************************************
*    函 数 名: KeyPinActive
*    功能说明: 判断按键是否按下
*    形    参: 无
*    返 回 值: 返回值1 表示按下(导通），0表示未按下（释放）
*********************************************************************************************************
*/
static uint8_t KeyPinActive(uint8_t _id)
{
    uint8_t level;

    if ((s_gpio_list[_id].gpio->IDR & s_gpio_list[_id].pin) == 0)
    {
        level = 0;
    }
    else
    {
        level = 1;
    }

    if (level == s_gpio_list[_id].ActiveLevel)
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

/*
*********************************************************************************************************
*    函 数 名: IsKeyDownFunc
*    功能说明: 判断按键是否按下。单键和组合键区分。单键事件不允许有其他键按下。
*    形    参: 无
*    返 回 值: 返回值1 表示按下(导通），0表示未按下（释放）
*********************************************************************************************************
*/
static uint8_t IsKeyDownFunc(uint8_t _id)
{
    /* 实体单键 */
    if (_id < HARD_KEY_NUM)
    {
        uint8_t value;
        value = KeyPinActive(_id);

        /* 屏蔽同时按下 K1 K2 */
        if ((value == 1) && (_id == KID_K1) && (1 == KeyPinActive(KID_K2)))
        {
            return 0;
        }
        /* 屏蔽同时按下 K2 K3 */
        if ((value == 1) && (_id == KID_K3) && (1 == KeyPinActive(KID_K2)))
        {
            return 0;
        }

        return value;
    }

    /* 组合键 K1K2 */
    if (_id == HARD_KEY_NUM + 0)
    {
        if (KeyPinActive(KID_K1) && KeyPinActive(KID_K2))
        {
            return 1;
        }
        else
        {
            return 0;
        }
    }

    /* 组合键 K2K3 */
    if (_id == HARD_KEY_NUM + 1)
    {
        if (KeyPinActive(KID_K2) && KeyPinActive(KID_K3))
        {
            return 1;
        }
        else
        {
            return 0;
        }
    }

    return 0;
}

/*
*********************************************************************************************************
*    函 数 名: bsp_InitKey
*    功能说明: 初始化按键. 该函数被 bsp_Init() 调用。
*    形    参:  无
*    返 回 值: 无
*********************************************************************************************************
*/
void bsp_InitKey(void)
{
    bsp_InitKeyHard(); /* 初始化按键硬件 */
    bsp_InitKeyVar();  /* 初始化按键变量 */
}

/*
*********************************************************************************************************
*    函 数 名: bsp_InitKeyHard
*    功能说明: 配置按键对应的GPIO
*    形    参:  无
*    返 回 值: 无
*********************************************************************************************************
*/
static void bsp_InitKeyHard(void)
{
    GPIO_InitTypeDef gpio_init;

    /* 第1步：打开GPIO时钟 */
    ALL_KEY_GPIO_CLK_ENABLE();

    /* 第2步：配置所有的按键GPIO为浮动输入模式(实际上CPU复位后就是输入状态) */
    gpio_init.Mode = GPIO_MODE_INPUT;            /* 设置输入 */
    gpio_init.Pull = GPIO_NOPULL;                /* 上下拉电阻不使能 */
    gpio_init.Speed = GPIO_SPEED_FREQ_VERY_HIGH; /* GPIO速度等级 */

    for (uint8_t i = 0; i < HARD_KEY_NUM; i++)
    {
        gpio_init.Pin = s_gpio_list[i].pin;
        HAL_GPIO_Init(s_gpio_list[i].gpio, &gpio_init);
    }
}

/*
*********************************************************************************************************
*    函 数 名: bsp_InitKeyVar
*    功能说明: 初始化按键变量
*    形    参:  无
*    返 回 值: 无
*********************************************************************************************************
*/
static void bsp_InitKeyVar(void)
{
    /* 初始化按键FIFO */
    ringbuffer_init(&s_key_kfifo, &s_buf[0], KEY_FIFO_SIZE);
    /* 给每个按键结构体成员变量赋一组缺省值 */
    for (uint8_t i = 0; i < KEY_COUNT; i++)
    {
        button_init(&s_tButton[i], IsKeyDownFunc, 1, i);
        button_attach(&s_tButton[i], PRESS_DOWN, button_callback);
        button_attach(&s_tButton[i], PRESS_UP, button_callback);
        button_attach(&s_tButton[i], PRESS_REPEAT, button_callback);
        button_attach(&s_tButton[i], SINGLE_CLICK, button_callback);
        button_attach(&s_tButton[i], DOUBLE_CLICK, button_callback);
        button_attach(&s_tButton[i], LONG_PRESS_START, button_callback);
        // button_attach(&s_tButton[i], LONG_PRESS_HOLD, button_callback);
        button_start(&s_tButton[i]);
    }
}

/*
*********************************************************************************************************
*    函 数 名: bsp_PutKey
*    功能说明: 将1个键值压入按键FIFO缓冲区。可用于模拟一个按键。
*    形    参:  _KeyCode : 按键代码
*    返 回 值: 无
*********************************************************************************************************
*/
void bsp_PutKey(uint8_t _KeyCode)
{
    ringbuffer_put_force(&s_key_kfifo, &_KeyCode, 1);
}

/*
*********************************************************************************************************
*    函 数 名: bsp_GetKey
*    功能说明: 从按键FIFO缓冲区读取一个键值。
*    形    参: 无
*    返 回 值: 按键代码
*********************************************************************************************************
*/
uint8_t bsp_GetKey(void)
{
    uint8_t key = 255;
    ringbuffer_getchar(&s_key_kfifo, &key);
    return key;
}

/*
*********************************************************************************************************
*    函 数 名: bsp_GetKeyState
*    功能说明: 读取按键的状态
*    形    参:  _ucKeyID : 按键ID，从0开始
*    返 回 值: 1 表示按下， 0 表示未按下
*********************************************************************************************************
*/
uint8_t bsp_GetKeyState(KEY_ID_E _ucKeyID)
{
    return IsKeyDownFunc(_ucKeyID);
}

/*
*********************************************************************************************************
*    函 数 名: bsp_ClearKey
*    功能说明: 清空按键FIFO缓冲区
*    形    参：无
*    返 回 值: 按键代码
*********************************************************************************************************
*/
void bsp_ClearKey(void)
{
    ringbuffer_reset(&s_key_kfifo);
}

/*
*********************************************************************************************************
*    函 数 名: button_callback
*    功能说明: 按键回调函数
*    形    参: struct Button btn
*    返 回 值: 无
*********************************************************************************************************
*/
static void button_callback(void *button)
{
    uint8_t btn_val;
    uint8_t ret = KEY_NONE;
    struct Button *btn = (struct Button *)button;

    btn_val = get_button_event(btn);
    switch (btn_val)
    {
    case PRESS_DOWN:
        ret = (btn->key_id) * number_of_event + PRESS_DOWN;
        break;

    case PRESS_UP:
        ret = (btn->key_id) * number_of_event + PRESS_UP;
        break;

    case PRESS_REPEAT:
        ret = (btn->key_id) * number_of_event + PRESS_REPEAT;
        break;

    case SINGLE_CLICK:
        ret = (btn->key_id) * number_of_event + SINGLE_CLICK;
        break;

    case DOUBLE_CLICK:
        ret = (btn->key_id) * number_of_event + DOUBLE_CLICK;
        break;

    case LONG_PRESS_START:
        ret = (btn->key_id) * number_of_event + LONG_PRESS_START;
        break;

    case LONG_PRESS_HOLD:
        ret = (btn->key_id) * number_of_event + LONG_PRESS_HOLD;
        break;
    }
    if (ret != KEY_NONE)
    {
        ringbuffer_put_force(&s_key_kfifo, &ret, 1);
    }
}

/**
 * [bsp_key_test 按键测试程序]
 *
 * @param   void  void  [void description]
 *
 * @return  void        [return description]
 */
void bsp_key_test(void)
{
    uint8_t KeyCode;
    KeyCode = bsp_GetKey();
    if (KEY_NONE != KeyCode)
    {
        switch (KeyCode)
        {
        case KEY_PRESS_DOWN(KID_K1):
            bsp_LedToggle(1);
            break;
        case KEY_PRESS_UP(KID_K1):
            bsp_LedToggle(1);
            break;
        case KEY_SINGLE_CLICK(KID_K1):
            bsp_LedToggle(2);
            break;

        case KEY_PRESS_DOWN(KID_K2):
            bsp_LedToggle(3);
            break;
        case KEY_PRESS_UP(KID_K2):
            bsp_LedToggle(3);
            break;
        case KEY_SINGLE_CLICK(KID_K2):
            bsp_LedToggle(4);
            break;

        default:
            break;
        }
    }
}
/***************************** 安富莱电子 www.armfly.com (END OF FILE) *********************************/
