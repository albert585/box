#ifndef CONTAINER_H
#define CONTAINER_H

#include "lvgl/lvgl.h"

typedef void (*page_delete_cb_t)(void *data);

typedef struct {
    lv_obj_t *object;
    int32_t   width;
    int32_t   height;
    uint32_t  color;
    lv_opa_t  opa;
    lv_align_t align;
    lv_scrollbar_mode_t scrollbar_mode;
} Con;

typedef struct {
    Con   base;
    page_delete_cb_t on_delete;
    void *user_data;
    bool  closable;
} PageCon;


extern lv_obj_t *main_page;
lv_obj_t *create_page(PageCon *pc);
extern lv_obj_t *create_container(Con *con);
extern lv_obj_t *create_close_button(lv_obj_t *parent, lv_event_cb_t cb, void *user_data);
extern void create_main_page(void);
extern void page_back(void);

#endif
