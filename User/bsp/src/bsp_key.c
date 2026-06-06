/**
 * @file    bsp_key.c
 * @brief   独立按键驱动模块（multi_button 库封装，支持单击/双击/长按/FIFO）
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

/**
 * @brief  读取指定 ID 按键的 GPIO 电平，判断是否处于激活状态
 * @param  _id  按键索引（0..HARD_KEY_NUM-1）
 * @retval 1 = 按下（激活），0 = 未按下（释放）
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

/**
 * @brief  multi_button 库的按键状态读取回调，区分单键与组合键
 * @note   单键激活时屏蔽同时按下的其他键（避免误触发组合键）
 * @param  _id  按键索引（0..KEY_COUNT-1）
 * @retval 1 = 按下，0 = 未按下
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
        /* 屏蔽同时按下 K2 K1 K3 */
        if ((value == 1) && (_id == KID_K2) && ((1 == KeyPinActive(KID_K3)) || (1 == KeyPinActive(KID_K1))))
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

/**
 * @brief  初始化按键模块（GPIO 硬件 + multi_button 变量），由 bsp_Init() 调用
 * @retval 无
 */
void bsp_InitKey(void)
{
    bsp_InitKeyHard(); /* 初始化按键硬件 */
    bsp_InitKeyVar();  /* 初始化按键变量 */
}

/**
 * @brief  配置所有按键 GPIO 为浮空输入
 * @retval 无
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

/**
 * @brief  初始化按键 FIFO 和 multi_button 结构体，注册所有事件回调
 * @retval 无
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

/**
 * @brief  将一个键值写入按键 FIFO（可用于软件模拟按键）
 * @param  _KeyCode 按键代码
 * @retval 无
 */
void bsp_PutKey(uint8_t _KeyCode)
{
    ringbuffer_put_force(&s_key_kfifo, &_KeyCode, 1);
}

/**
 * @brief  从按键 FIFO 读取一个键值
 * @retval 键值，无按键时返回 KEY_NONE (255)
 */
uint8_t bsp_GetKey(void)
{
    uint8_t key = 255;
    ringbuffer_getchar(&s_key_kfifo, &key);
    return key;
}

/**
 * @brief  查询指定按键当前是否处于按下状态
 * @param  _ucKeyID  按键 ID（KEY_ID_E 枚举）
 * @retval 1 = 按下，0 = 未按下
 */
uint8_t bsp_GetKeyState(KEY_ID_E _ucKeyID)
{
    return IsKeyDownFunc(_ucKeyID);
}

/**
 * @brief  清空按键 FIFO 缓冲区
 * @retval 无
 */
void bsp_ClearKey(void)
{
    ringbuffer_reset(&s_key_kfifo);
}

/**
 * @brief  按键扫描（非阻塞），每 5ms 由 SysTick 调用一次
 * @retval 无
 */
inline void bsp_KeyScan5ms(void)
{
    button_ticks();
}

/**
 * @brief  multi_button 事件回调，将键值编码后写入 FIFO
 * @param  button  触发事件的 Button 结构体指针
 * @retval 无
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
        ret = (btn->button_id) * number_of_event + PRESS_DOWN;
        break;

    case PRESS_UP:
        ret = (btn->button_id) * number_of_event + PRESS_UP;
        break;

    case PRESS_REPEAT:
        ret = (btn->button_id) * number_of_event + PRESS_REPEAT;
        break;

    case SINGLE_CLICK:
        ret = (btn->button_id) * number_of_event + SINGLE_CLICK;
        break;

    case DOUBLE_CLICK:
        ret = (btn->button_id) * number_of_event + DOUBLE_CLICK;
        break;

    case LONG_PRESS_START:
        ret = (btn->button_id) * number_of_event + LONG_PRESS_START;
        break;

    case LONG_PRESS_HOLD:
        ret = (btn->button_id) * number_of_event + LONG_PRESS_HOLD;
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
        BSP_INFO("keyCode = %2d, Event= %d,key= %d", KeyCode, KeyCode % 7, KeyCode / 7);
    }
}
/* end of file */
