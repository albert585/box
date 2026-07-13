/**
 * @file lv_text_clock.h
 * 文字时钟控件
 */

#ifndef LV_TEXT_CLOCK_H
#define LV_TEXT_CLOCK_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/
#include "lvgl/lvgl.h"
#include <stdio.h>
#include <time.h>
#include <string.h>

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/* 时钟对象类型（LVGL 9.x 直接使用 lv_obj_t） */
typedef lv_obj_t lv_text_clock_t;

/**********************
 * GLOBAL PROTOTYPES
 **********************/

/**
 * 创建文字时钟对象
 * @param parent 父对象
 * @return 指向新时钟对象的指针
 */
lv_obj_t * lv_text_clock_create(lv_obj_t * parent);

/**
 * 更新时间显示（定时器回调函数）
 * @param timer 定时器对象
 */
void lv_text_clock_update_time(lv_timer_t * timer);

/**********************
 *      MACROS
 **********************/

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /*LV_TEXT_CLOCK_H*/