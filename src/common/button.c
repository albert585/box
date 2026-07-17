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
    const char *text[]={"Print","Test","Bird"};
    lv_event_cb_t cbs[] = {event_print_test, event_btn_test, event_open_bird};

    int i;
    Con con = {
        .object = main_page,
        .width  = 640,
        .height = 60,
        .color  = 0,
        .opa    = LV_OPA_TRANSP,
        .align  = LV_ALIGN_CENTER,
        .scrollbar_mode = LV_SCROLLBAR_MODE_OFF,
    };
    lv_obj_t* button_container=create_container(&con);
    lv_obj_set_flex_flow(button_container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(button_container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    for(i=0;i<sizeof(text)/sizeof(text[0]);i++){
        create_button(160, 60, button_container, cbs[i], text[i],NULL);
    }
    
}
