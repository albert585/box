#include "./container.h"
#include "stdio.h"

lv_obj_t *parent = NULL;  // 实际定义


void create_container(void) { //创建显示区域

    parent = lv_obj_create(lv_screen_active());
    lv_obj_set_size(parent, 640, 480);
    lv_obj_set_style_bg_color(parent, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(parent, LV_OPA_100, 0);
    lv_obj_set_style_border_width(parent, 0, 0);
    lv_obj_center(parent);
    lv_obj_set_scrollbar_mode(parent, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_scroll_dir(parent, LV_DIR_HOR);

    /* Set flex layout for automatic horizontal alignment */
    lv_obj_set_layout(parent, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_ROW);
    /* 主轴左对齐(水平)，交叉轴居中(垂直)，多行左对齐 */
    lv_obj_set_flex_align(parent, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_gap(parent, 20, 0);
    lv_obj_set_style_pad_left(parent, 20, 0);
    lv_obj_set_style_pad_right(parent, 20, 0);
    lv_obj_set_style_pad_top(parent, 20, 0);
    lv_obj_set_style_pad_bottom(parent, 20, 0);
}

void page_back(void) {
    if (parent) {
        lv_obj_clear_flag(parent, LV_OBJ_FLAG_HIDDEN);
    }
}