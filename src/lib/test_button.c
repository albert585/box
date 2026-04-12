#include "lvgl/lvgl.h"
#include "container.h"
#include <stdio.h>

static lv_obj_t *test_btn = NULL;
static lv_obj_t *test_label = NULL;
static lv_obj_t *click_count_label = NULL;
static int click_count = 0;

static void test_btn_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_CLICKED) {
        click_count++;
        printf("[Test Button] Clicked! Count: %d\n", click_count);

        if (click_count_label) {
            char buf[32];
            snprintf(buf, sizeof(buf), "Clicks: %d", click_count);
            lv_label_set_text(click_count_label, buf);
        }

        // 改变按钮颜色
        if (test_btn) {
            lv_color_t new_color = (click_count % 2 == 0) ?
                lv_palette_main(LV_PALETTE_BLUE) :
                lv_palette_main(LV_PALETTE_GREEN);
            lv_obj_set_style_bg_color(test_btn, new_color, 0);
        }
    }
}

void create_test_button(void)
{
    extern lv_obj_t *parent;

    printf("[Test Button] Creating test button...\n");

    // 创建按钮
    test_btn = lv_btn_create(parent);
    lv_obj_set_size(test_btn, 150, 60);
    lv_obj_align(test_btn, LV_ALIGN_CENTER, 0, 0);

    // 设置按钮样式
    lv_obj_set_style_bg_color(test_btn, lv_palette_main(LV_PALETTE_BLUE), 0);
    lv_obj_set_style_radius(test_btn, 10, 0);

    // 添加事件回调
    lv_obj_add_event_cb(test_btn, test_btn_event_cb, LV_EVENT_CLICKED, NULL);

    // 创建按钮标签
    test_label = lv_label_create(test_btn);
    lv_label_set_text(test_label, "Click Me!");
    lv_obj_center(test_label);

    // 创建点击计数标签
    click_count_label = lv_label_create(parent);
    lv_label_set_text(click_count_label, "Clicks: 0");
    lv_obj_align(click_count_label, LV_ALIGN_CENTER, 0, 50);
    lv_obj_set_style_text_font(click_count_label, &lv_font_montserrat_16, 0);

    printf("[Test Button] Test button created successfully!\n");
}
