#include "button.h"
#include "events.h"
#include "container.h"

lv_obj_t * btn_exit = NULL;

void create_button(lv_event_cb_t  event_cb, const char * text){
    lv_obj_t * button=lv_btn_create(parent);
    lv_obj_set_size(button, 160, 60);
    if(event_cb){lv_obj_add_event_cb(button,event_cb,LV_EVENT_CLICKED,NULL);}
    lv_obj_t * label = lv_label_create(button);
    lv_label_set_text(label, text);
    lv_obj_center(label);
    lv_obj_center(button);
    lv_obj_add_flag(button, LV_OBJ_FLAG_CLICKABLE);
}

void button(void)
{
    create_button(event_btn_test, "Test");
}
