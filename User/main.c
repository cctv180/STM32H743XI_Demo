/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2020 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under BSD 3-Clause license,
  * the "License"; You may not use this file except in compliance with the
  * License. You may obtain a copy of the License at:
  *                        opensource.org/licenses/BSD-3-Clause
  *
  ******************************************************************************
  */
/* Includes ------------------------------------------------------------------*/
#include "bsp.h"

/* Private includes ----------------------------------------------------------*/
#include "multi_button.h"

/* Private typedef -----------------------------------------------------------*/

/* Private define ------------------------------------------------------------*/

/* Private macro -------------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/
struct Button btn1;

/* Private function prototypes -----------------------------------------------*/

/* Private user code ---------------------------------------------------------*/
uint8_t read_button1_GPIO(uint8_t key_id)
{
    if(255 == key_id)
    {
        return HAL_GPIO_ReadPin(GPIOI, GPIO_PIN_8);
    }
    return 1;
}
//按键1按下事件回调函数
void btn1_press_down_Handler(void *btn)
{
    bsp_LedOn(1);
}

//按键1松开事件回调函数
void btn1_press_up_Handler(void *btn)
{
    bsp_LedOff(1);
}

//按键双击事件回调函数
void btn1_press_double_Handler(void *btn)
{
    bsp_LedToggle(2);
}

//按键单击事件回调函数
void btn1_press_single_Handler(void *btn)
{
    bsp_LedToggle(3);
}
/**
* @brief  The application entry point.
* @retval int
*/
int main(void)
{
    GPIO_InitTypeDef gpio_init_structure;
    /* HAL库，MPU，Cache，时钟等系统初始化 */
    System_Init();
    bsp_Init();

    __HAL_RCC_GPIOI_CLK_ENABLE();
    /* 设置 GPIOD 相关的IO为复用推挽输出 */
    gpio_init_structure.Mode = GPIO_MODE_INPUT;
    gpio_init_structure.Pull = GPIO_NOPULL;
    gpio_init_structure.Speed = GPIO_SPEED_FREQ_LOW;
    gpio_init_structure.Pin = GPIO_PIN_8;
    HAL_GPIO_Init(GPIOI, &gpio_init_structure);
    //初始化按键对象
    button_init(&btn1, read_button1_GPIO, 0, 255);
    //注册按钮事件回调函数
    button_attach(&btn1, PRESS_DOWN, btn1_press_down_Handler);
    button_attach(&btn1, PRESS_UP, btn1_press_up_Handler);
    button_attach(&btn1, DOUBLE_CLICK, btn1_press_double_Handler);
    button_attach(&btn1, SINGLE_CLICK, btn1_press_single_Handler);
    //启动按键
    button_start(&btn1);
    while (1)
    {
        button_ticks();
        HAL_Delay(5);
    }
}

#ifdef USE_FULL_ASSERT
/**
* @brief  Reports the name of the source file and the source line number
*         where the assert_param error has occurred.
* @param  file: pointer to the source file name
* @param  line: assert_param error line source number
* @retval None
*/
void assert_failed(uint8_t *file, uint32_t line)
{
    /* USER CODE BEGIN 6 */
    /* User can add his own implementation to report the file name and line number,
     tex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
    /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
