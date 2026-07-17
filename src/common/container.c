#include "./container.h"
#include "lvgl/src/display/lv_display.h"
#include "events.h"

lv_obj_t *main_page = NULL;


static void on_page_delete(lv_event_t *e) {
    PageCon *pc = lv_event_get_user_data(e);
    if (pc->on_delete) pc->on_delete(pc->user_data);
    lv_obj_clear_flag(main_page, LV_OBJ_FLAG_HIDDEN);
}

lv_obj_t *create_page(PageCon *pc) {
    lv_obj_t *obj = create_container(&pc->base);
    lv_obj_add_flag(main_page, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(obj, on_page_delete, LV_EVENT_DELETE, pc);
    if (pc->closable)
        create_close_button(obj, container_close_cb, obj);
    return obj;
}

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

lv_obj_t *create_close_button(lv_obj_t *parent, lv_event_cb_t cb, void *user_data) {
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_set_size(btn, 30, 30);
    lv_obj_align(btn, LV_ALIGN_TOP_RIGHT, -5, 5);
    lv_label_set_text(lv_label_create(btn), "x");
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, user_data);
    return btn;
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
