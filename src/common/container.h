#include "lvgl/lvgl.h"

typedef struct {
    lv_obj_t *object;
    int32_t   width;
    int32_t   height;
    uint32_t  color;
    lv_opa_t  opa;
    lv_align_t align;
    lv_scrollbar_mode_t scrollbar_mode;
} Con;

extern lv_obj_t *main_page;

extern lv_obj_t *create_container(Con *con);
extern void create_main_page(void);
extern void page_back(void);
