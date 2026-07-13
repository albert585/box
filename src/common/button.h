#include "lvgl/lvgl.h"

extern void button(void);
extern lv_obj_t * btn_exit;
extern void create_button(int w, int h, lv_obj_t * parent, lv_event_cb_t event_cb, const char * text,void *user_data);
