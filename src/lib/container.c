#include "./container.h"
#include "stdio.h"

lv_obj_t *parent = NULL;  // 实际定义


void create_container(void) { //创建显示区域

    parent = lv_obj_create(lv_screen_active());
    lv_obj_set_size(parent, 640, 480);
    lv_obj_set_style_bg_color(parent, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(parent, LV_OPA_100, 0);
    lv_obj_set_style_border_width(parent, 0, 0);
    lv_obj_align(parent, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_flag(parent, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_scrollbar_mode(parent, LV_SCROLLBAR_MODE_OFF);
}

void page_back(void) {
    if (parent) {
        lv_obj_clear_flag(parent, LV_OBJ_FLAG_HIDDEN);
        lv_obj_update_layout(parent);
    }
}