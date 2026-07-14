#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include "lvgl/lvgl.h"
#include "arch/arch.h"
#include "common/container.h"
#include "common/button.h"
#include "main.h"

char homepath[PATH_MAX_LENGTH] = {0};
int32_t  sleepTs     = -1;
uint32_t homeClickTs = -1;
uint32_t backgroundTs = -1;
bool     deepSleep  = false;
uint8_t  dontDeepSleepEnabled = 0;
uint8_t  dontTimeoutEnabled = 0;

lv_display_t *disp = NULL;

extern uint32_t custom_tick_get(void);
extern uint32_t tick_get(void);

void sysSleep(void)
{
    deepSleep = false;
    sleepTs = custom_tick_get();
    arch_touch_close();
    arch_lcd_close();
}

void sysWake(void)
{
    deepSleep = false;
    sleepTs = -1;
    arch_cpu_restore();
    arch_touch_open();
    arch_lcd_open();
}

void setDontDeepSleep(bool b)
{
    dontDeepSleepEnabled += (b ? 1 : -1);
}

void setDontTimeout(bool b)
{
    dontTimeoutEnabled += (b ? 1 : -1);
}

void switchBackground(void)
{
    if(backgroundTs != -1) return;
    backgroundTs = custom_tick_get();
    sleepTs = -1;
}

void switchRobot(void)
{
    switchBackground();
    chdir(homepath);
    close(disphd);
    close(fbd);
    close(powerd);
    system("switch_robot");
}

void switchForeground(void)
{
    if(backgroundTs == -1) return;
    chdir(homepath);
    system("chmod 777 switch_foreground");
    system("sh ./switch_foreground &");
}

int main(int argc, char *argv[])
{
    bool isDaemonMode = false;
    system("killall dlamInit");
    system("killall ST03_app");

    for(int i = 0; i < argc; i++) {
        char *arg = argv[i];
        printf("argv[%d] = %s\n", i, arg);
        if(strcmp(arg, "-d") == 0) {
            isDaemonMode = false;
        }
        if(strcmp(arg, "-w") == 0) {
            daemon(1, 0);
            switchBackground();
            while(1) {
                usleep(25000);
                arch_read_key_home();
            }
        }
        if(strcmp(arg, "-h") == 0 || strcmp(arg, "--help") == 0) {
            printf(
                "lvglsim [-d] [-w] [-h]\n"
                "  (no args)  daemon mode, start GUI\n"
                "  -d          run in foreground\n"
                "  -w          watchdog mode: background + home key only\n"
                "  -h          print this help\n"
            );
            return 0;
        }
    }

    arch_device_init();
    getcwd(homepath, PATH_MAX_LENGTH);
    setenv("TZ", "CST-8", 1);
    tzset();

    if(isDaemonMode) daemon(1, 0);

    arch_lcd_refresh();
    lv_init();
    arch_display_init();
    arch_lcd_open();
    printf("display OK!\n");
    arch_touch_init();
    printf("init OK\n");

    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(0xFFFFFF), 0);
    create_container();
    button();

    while(1) {
        arch_read_key_home();
        if(backgroundTs == -1) {
            arch_read_key_power();
            if(sleepTs == -1) {
                lv_timer_handler();
                arch_lcd_detect_timeout();
                usleep(5000);
            } else {
                if(dontDeepSleepEnabled)
                    sleepTs = tick_get();
                else if(!deepSleep && tick_get() - sleepTs >= 60000)
                    arch_deep_sleep();
                usleep(25000);
            }
        } else {
            usleep(25000);
        }
    }

    arch_deinit();
    return 0;
}

uint32_t custom_tick_get(void)
{
    static uint64_t start_ms = 0;
    if(start_ms == 0) {
        struct timeval tv_start;
        gettimeofday(&tv_start, NULL);
        start_ms = (tv_start.tv_sec * 1000000 + tv_start.tv_usec) / 1000;
    }
    struct timeval tv_now;
    gettimeofday(&tv_now, NULL);
    uint64_t now_ms = (tv_now.tv_sec * 1000000 + tv_now.tv_usec) / 1000;
    return (uint32_t)(now_ms - start_ms);
}

uint32_t tick_get(void)
{
    static uint32_t start_ms = 0;
    if(start_ms == 0) {
        struct timeval tv_start;
        gettimeofday(&tv_start, NULL);
        start_ms = (tv_start.tv_sec * 1000000 + tv_start.tv_usec) / 1000;
    }
    struct timeval tv_now;
    gettimeofday(&tv_now, NULL);
    uint32_t now_ms = (tv_now.tv_sec * 1000000 + tv_now.tv_usec) / 1000;
    return now_ms - start_ms;
}
