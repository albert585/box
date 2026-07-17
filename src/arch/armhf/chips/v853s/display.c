#define _GNU_SOURCE
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <pthread.h>
#include "lvgl/lvgl.h"
#include "lvgl/src/drivers/display/fb/lv_linux_fbdev.h"
#include "arch/arch.h"
#include "v853.h"

extern uint32_t custom_tick_get(void);

static uint32_t lcdBrightness = 25;
static bool isScreenTimeout = false;
static volatile int vsync_flag = 0;

static const char *getenv_default(const char *name, const char *default_val)
{
    const char *value = getenv(name);
    return value ? value : default_val;
}

static void *vsync_thread_func(void *arg)
{
    (void)arg;
    while (1) {
        while (!vsync_flag) usleep(5000);
        vsync_flag = 0;
        int buf[8] = {0};
        ioctl(fbd, 0x4606u, buf);
    }
    return NULL;
}

static void on_flush_finish(lv_event_t *e)
{
    (void)e;
    vsync_flag = 1;
}

void arch_display_init(void)
{
    const char *device = getenv_default("LV_LINUX_FBDEV_DEVICE", "/dev/fb0");
    disp = lv_linux_fbdev_create();
    lv_linux_fbdev_set_file(disp, device);
    lv_display_set_resolution(disp, 1280, 768);
    lv_display_add_event_cb(disp, on_flush_finish, LV_EVENT_FLUSH_FINISH, NULL);
    pthread_create(&(pthread_t){0}, NULL, vsync_thread_func, NULL);
}

void arch_touch_init(void)
{
    lv_indev_t *touch = lv_evdev_create(LV_INDEV_TYPE_POINTER, "/dev/input/event6");
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

static void lcdSetBrightnessInner(int brightness)
{
    int buffer[8] = {0};
    buffer[1] = brightness;
    ioctl(disphd, 0x102u, buffer);
}

void arch_lcd_set_brightness(int brightness)
{
    lcdBrightness = brightness;
    lcdSetBrightnessInner(brightness);
}

uint32_t arch_lcd_get_brightness(void)
{
    return lcdBrightness;
}

void arch_lcd_detect_timeout(void)
{
    if(dontTimeoutEnabled){
        isScreenTimeout = false;
        lastInputTs = custom_tick_get();
        return;
    }

    uint32_t timeout_ms = custom_tick_get() - lastInputTs;
    if(timeout_ms < 30000){
        if(isScreenTimeout) {
            isScreenTimeout = false;
            lcdSetBrightnessInner(lcdBrightness);
        }
    } else if(!isScreenTimeout) {
        isScreenTimeout = true;
        lcdSetBrightnessInner(lcdBrightness / 5);
    } else if(timeout_ms > 35000) {
        deepSleep = false;
        isScreenTimeout = false;
        sleepTs = custom_tick_get();
        arch_touch_close();
        arch_lcd_close();
    }
}

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

uint32_t arch_timer_handler(void)
{
    lv_timer_handler();
    arch_lcd_detect_timeout();
    return 5;
}
