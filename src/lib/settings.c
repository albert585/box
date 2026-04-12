#include "settings.h"
#include "./container.h"
#include "./events.h"
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pthread.h>
#include <time.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <string.h>

lv_obj_t *setting;
lv_obj_t *subpage;
lv_obj_t *mainpage;
lv_obj_t *slider_label;
lv_obj_t *slider;
lv_obj_t *line;
lv_obj_t *label;
int dispd = 0;
unsigned int arg[3];
unsigned int bl;

static void backlight_slider_create(lv_obj_t *parent_page);

void settings(void)
{
    setting = lv_menu_create(parent);
    lv_obj_set_size(setting, 600, 440);
    lv_obj_center(setting);
    lv_obj_add_event_cb(setting, event_close_manager, LV_EVENT_CLICKED, setting);

    /* 创建主页面 */
    mainpage = lv_menu_page_create(setting, "Settings");

    /* 创建背光调节菜单项 */
    line = lv_menu_cont_create(mainpage);
    label = lv_label_create(line);
    lv_label_set_text(label, LV_SYMBOL_SETTINGS " BackLight 背光调节");

    /* 创建关于菜单项 */
    line = lv_menu_cont_create(mainpage);
    label = lv_label_create(line);
    lv_label_set_text(label, LV_SYMBOL_WARNING " About");

    /* 创建子页面（背光调节） */
    subpage = lv_menu_page_create(setting, "BackLight");

    /* 在子页面中创建背光滑块 */
    backlight_slider_create(subpage);

    /* 点击背光菜单项跳转到子页面 */
    lv_obj_t *backlight_item = lv_obj_get_child(mainpage, 0);
    if (backlight_item) {
        lv_menu_set_load_page_event(setting, backlight_item, subpage);
    }

    /* 显示主页面 */
    lv_menu_set_page(setting, mainpage);
}

static void backlight_slider_create(lv_obj_t *parent_page)
{
    dispd = open("/dev/disp", O_RDWR);
    arg[0] = 0;
    bl = ioctl(dispd, 0x103u, arg);

    /* 创建滑块容器 */
    lv_obj_t *slider_cont = lv_obj_create(parent_page);
    lv_obj_set_size(slider_cont, LV_PCT(90), LV_SIZE_CONTENT);
    lv_obj_center(slider_cont);
    lv_obj_set_style_pad_all(slider_cont, 20, 0);

    /* 创建标签 */
    slider_label = lv_label_create(slider_cont);
    //lv_label_set_text(slider_label, "亮度: %d", bl);
    lv_obj_align(slider_label, LV_ALIGN_TOP_MID, 0, 0);

    /* 创建滑块 */
    slider = lv_slider_create(slider_cont);
    lv_obj_set_width(slider, LV_PCT(80));
    lv_obj_align_to(slider, slider_label, LV_ALIGN_OUT_BOTTOM_MID, 0, 20);
    lv_obj_add_event_cb(slider, slider_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_slider_set_value(slider, bl, LV_ANIM_OFF);
    lv_slider_set_range(slider, 0, 255);
}

void slider_event_cb(lv_event_t * e)
{
    lv_obj_t * slider_obj = lv_event_get_target(e);
    bl = (unsigned int)lv_slider_get_value(slider_obj);

    /* 更新标签显示 */
    if (slider_label) {
        //lv_label_set_text_fmt(slider_label, "亮度: %d", bl);
    }

    arg[0] = 0;
    arg[1] = bl;
    if (dispd >= 0) {
        ioctl(dispd, 0x102u, arg);
    }
}

void settings_close(void)
{
    if (dispd >= 0) {
        close(dispd);
        dispd = -1;
    }
}
