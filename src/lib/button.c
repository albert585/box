#include "button.h"
#include "events.h"
#include "container.h"
#include "lvgl/src/widgets/label/lv_label.h"

void button(void)
{
    lv_obj_t * btn_label_video;
    lv_obj_t * btn_label_exit;

    lv_obj_t * btn_video = lv_btn_create(parent);

    lv_obj_add_event_cb(btn_video, event_play_video, LV_EVENT_CLICKED, NULL);

    btn_label_video = lv_label_create(btn_video);
    btn_label_exit = lv_label_create(btn_exit);

    lv_label_set_text(btn_label_video, "Video");
    lv_label_set_text(btn_label_exit, "exit");
}
