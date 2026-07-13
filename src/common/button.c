#include "button.h"
#include "events.h"
#include "container.h"

lv_obj_t * btn_exit = NULL;

void create_button(int w, int h, lv_obj_t * parent, lv_event_cb_t event_cb, const char * text,void *user_data){
    lv_obj_t * button=lv_btn_create(parent);
    lv_obj_set_size(button, w, h);
    if(event_cb){lv_obj_add_event_cb(button,event_cb,LV_EVENT_CLICKED,NULL);}
    lv_obj_t * label = lv_label_create(button);
    lv_label_set_text(label, text);
    lv_obj_center(label);
    lv_obj_add_flag(button, LV_OBJ_FLAG_CLICKABLE);
}

void button(void)
{
    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(parent, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_column(parent, 10, 0);
    create_button(160, 60, parent, event_print_test, "Print",NULL);
    create_button(160, 60, parent, event_btn_test, "Test",NULL);
    create_button(160, 60, parent, event_open_bird, "Bird",NULL);
}
