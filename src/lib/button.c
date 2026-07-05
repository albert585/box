#include "button.h"
#include "events.h"
#include "container.h"
#include "lvgl/src/widgets/label/lv_label.h"

void creat_button(void event){
    lv_obj_t * button=lv_btn_create(parent);
    lv_obj_add_event_cb(button,event,LV_EVENT_CLICKED,NULL);
    lv_label_create(button);
    lv_label_set_text(button, "button");

}

void button(void)
{
    creat_button(event_play_video());
}
