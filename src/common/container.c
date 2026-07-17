#include "./container.h"
#include "lvgl/src/display/lv_display.h"

lv_obj_t *main_page = NULL;

lv_obj_t* create_container(Con *con) {
    lv_obj_t *obj = lv_obj_create(con->object);
    lv_obj_set_size(obj, con->width, con->height);
    if (con->color) lv_obj_set_style_bg_color(obj, lv_color_hex(con->color), 0);
    lv_obj_set_style_bg_opa(obj, con->opa, 0);
    lv_obj_set_style_border_width(obj, 0, 0);
    if (con->align) lv_obj_align(obj, con->align, 0, 0);
    lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_scrollbar_mode(obj, con->scrollbar_mode);
    return obj;
}




void create_main_page(void) {
    Con con = {
        .object = lv_screen_active(),
        .width  = lv_display_get_horizontal_resolution(lv_display_get_default()),
        .height = lv_display_get_vertical_resolution(lv_display_get_default()),
        .color  = 0,
        .opa    = LV_OPA_COVER,
        .align  = LV_ALIGN_CENTER,
        .scrollbar_mode = LV_SCROLLBAR_MODE_OFF,
    };
    main_page = create_container(&con);
}

void page_back(void) {
    if (main_page) {
        lv_obj_clear_flag(main_page, LV_OBJ_FLAG_HIDDEN);
        lv_obj_update_layout(main_page);
    }
}
