#include <stddef.h>
#include "arch.h"

void arch_display_init(void)
{
    disp = lv_wayland_window_create(640, 480, "LVGL Demo", NULL);
}

void arch_touch_init(void) {}
void arch_lcd_open(void) {}
void arch_lcd_close(void) {}
void arch_lcd_refresh(void) {}
void arch_lcd_set_brightness(int brightness) {}
uint32_t arch_lcd_get_brightness(void) { return 25; }
void arch_lcd_detect_timeout(void) {}
void arch_touch_open(void) {}
void arch_touch_close(void) {}
