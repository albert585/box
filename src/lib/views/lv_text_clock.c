/**
 * @file lv_text_clock.c
 * 文字时钟控件实现（LVGL 9.x 简化版）
 */

#include "lv_text_clock.h"

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

lv_obj_t * lv_text_clock_create(lv_obj_t * parent)
{
    LV_LOG_INFO("Start to create text clock\n");

    /* 创建标签对象 */
    lv_obj_t * label = lv_label_create(parent);

    /* 设置初始文本 */
    lv_label_set_text(label, "00:00:00");

    /* 设置居中对齐 */
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);

    /* 创建更新定时器（每250ms更新一次） */
    lv_timer_t * timer = lv_timer_create(lv_text_clock_update_time, 250, label);
    if(timer == NULL) {
        LV_LOG_ERROR("Failed to create timer for text clock\n");
    }

    LV_LOG_INFO("Text clock created\n");

    return label;
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

/**
 * 更新时间显示
 */
void lv_text_clock_update_time(lv_timer_t * timer)
{
    lv_obj_t * label = (lv_obj_t *)lv_timer_get_user_data(timer);

    /* 获取当前时间 */
    time_t rawtime;
    struct tm * timeinfo;

    time(&rawtime);
    timeinfo = localtime(&rawtime);

    if(timeinfo == NULL) {
        lv_label_set_text(label, "Error");
        return;
    }

    /* 格式化为"HH:MM:SS"格式 */
    char time_str[32];
    snprintf(time_str, sizeof(time_str),
             "%02d:%02d:%02d",
             timeinfo->tm_hour,
             timeinfo->tm_min,
             timeinfo->tm_sec);

    /* 更新标签文本 */
    lv_label_set_text(label, time_str);
}