#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include "lvgl/lvgl.h"
#include "lvgl/src/drivers/display/fb/lv_linux_fbdev.h"
#include "arch.h"

static const char *getenv_default(const char *name, const char *default_val)
{
    const char *value = getenv(name);
    return value ? value : default_val;
}

void arch_display_init(void)
{
    const char *device = getenv_default("LV_LINUX_FBDEV_DEVICE", "/dev/fb0");
    disp = lv_linux_fbdev_create();
    lv_linux_fbdev_set_file(disp, device);
    lv_display_set_resolution(disp, 640, 480);
}

void arch_touch_init(void)
{
    lv_indev_t *touch = lv_evdev_create(LV_INDEV_TYPE_POINTER, "/dev/input/event0");
    lv_indev_set_display(touch, disp);
}

void arch_lcd_open(void)
{
    int buffer[8] = {0};
    buffer[1] = 1;
    ioctl(disphd, 0xFu, buffer);
    printf("[lcd]opened\n");
}

void arch_lcd_close(void)
{
    char buffer[24] = {0};
    ioctl(disphd, 0xFu, buffer);
    printf("[lcd]closed\n");
}

void arch_lcd_refresh(void)
{
    int buffer[8] = {0};
    ioctl(fbd, 0x4606u, buffer);
}

void arch_lcd_set_brightness(int brightness) {}
uint32_t arch_lcd_get_brightness(void) { return 25; }
void arch_lcd_detect_timeout(void) {}

void arch_touch_open(void)
{
    int tpd = open("/proc/sprocomm_tpInfo", 526338);
    write(tpd, "1", 1u);
    close(tpd);
    printf("[tp]opened\n");
}

void arch_touch_close(void)
{
    int tpd = open("/proc/sprocomm_tpInfo", 526338);
    write(tpd, "0", 1u);
    close(tpd);
    printf("[tp]closed\n");
}
