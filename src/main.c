#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pthread.h>
#include <time.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <string.h>
#include "lvgl/lvgl.h"
#include "lvgl/src/drivers/display/fb/lv_linux_fbdev.h"
#include "lib/file_manager.h"
#include "lib/container.h"
#include "lib/button.h"
#include "lib/settings.h"
#include "lib/player.h"
#include "lib/test_button.h"
#include "main.h"

#define PATH_MAX_LENGTH 256
char homepath[PATH_MAX_LENGTH] = {0};

int disphd  = 0;    
int fbd = 0;
int homed  = 0;
int powerd  = 0;

int32_t sleepTs     = -1;
uint32_t homeClickTs = -1;
uint32_t backgroundTs = -1;

extern uint32_t custom_tick_get(void);
extern uint32_t tick_get(void);

bool deepSleep  = false;
bool dontDeepSleep  = false;

// CPU频率控制
static char original_governor[32] = {0};

lv_display_t * disp = NULL;

const char *getenv_default(const char *name, const char *default_val)
{
    const char* value = getenv(name);
    return value ? value : default_val;
}

static void lv_linux_disp_init(void)
{
    const char *device = getenv_default("LV_LINUX_FBDEV_DEVICE", "/dev/fb0");
    disp= lv_linux_fbdev_create();
    lv_linux_fbdev_set_file(disp, device);
    lv_display_set_resolution(disp,640,480);

}



static void lv_linux_touch_init(void)
{
    lv_indev_t *touch =lv_evdev_create(LV_INDEV_TYPE_POINTER, "/dev/input/event6");
    lv_indev_set_display(touch, disp);
    //lv_evdev_set_calibration(touch, 20, 860, 220, -120);
    //lv_evdev_set_swap_axes(touch,false);
}

void readKeyHome(void) {
        char buffer[16] = {0};
        while (read(homed, buffer, 0x10u) > 0) {
                if(buffer[10] != 0x73) return;

                if(buffer[12] == 0x00) {
                        printf("[key]home_up\n");
            uint32_t ts = custom_tick_get();
            if(homeClickTs != -1 && ts - homeClickTs <= 300){
                switchForeground();
                homeClickTs = -1;
            } else {
                homeClickTs = ts;
            }
        } else {
            printf("[key]home_down\n");
        }
    }
}
void readKeyPower(void) {  
    char buffer[16] = {0};
    while (read(powerd, buffer, 0x10u) > 0) {
                if(buffer[10] != 0x74) return;

                if(buffer[12] == 0x00) {
                        //printf("[key]power_up\n");
                        if(sleepTs == -1) {
                                sysSleep();   
                        }
                        else {
                                sysWake();
                        }
                }
                else {
                        //printf("[key]power_down\n");
                }
        }
}
void sysSleep(void){
        deepSleep = false;
        sleepTs = custom_tick_get();
        touchClose();   
        lcdClose();
}
void switchBackground(void){
    if(backgroundTs != -1) return;
    backgroundTs = custom_tick_get();
    sleepTs    = -1;
}
void switchForeground(void)
{
    if(backgroundTs == -1) return;

    chdir(homepath);
    system("chmod 777 switch_foreground");
    system("sh ./switch_foreground &");
    //sleep(114514);
}

void switchRobot(){
    switchBackground();

    chdir(homepath);
    close(disphd);
    close(fbd);
    close(powerd);
    system("switch_robot");
}

void lcdRefresh(void) {
    int buffer[8] = {0};
        ioctl(fbd, 0x4606u, buffer);
}
void lcdOpen(void) {
    int buffer[8] = {0};
    buffer[1] = 1;
    ioctl(disphd, 0xFu, buffer);
    printf("[lcd]opened\n");
}

void lcdClose(void) {
    char buffer[24] = {0};
    ioctl(disphd, 0xFu, buffer);
    printf("[lcd]closed\n");
}
void touchOpen(void) {
    int tpd = open("/proc/sprocomm_tpInfo", 526338);
    write(tpd, "1", 1u);
    close(tpd);
    printf("[tp]opened\n");
}

void touchClose(void) {
    int tpd = open("/proc/sprocomm_tpInfo", 526338);
    write(tpd, "0", 1u);
    close(tpd);
    printf("[tp]closed\n");
}

// 保存当前CPU governor
static void saveCpuFreq(void) {
    FILE *fp = fopen("/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor", "r");
    if (fp) {
        if (fgets(original_governor, sizeof(original_governor), fp)) {
            // 移除换行符
            original_governor[strcspn(original_governor, "\n")] = 0;
            printf("[cpu] Saved original governor: %s\n", original_governor);
        }
        fclose(fp);
    } else {
        printf("[cpu] Failed to read scaling_governor\n");
    }
}

// 设置为最低频率（切换到 powersave governor）
static void setCpuMinFreq(void) {
    saveCpuFreq();
    FILE *fp = fopen("/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor", "w");
    if (fp) {
        fprintf(fp, "powersave");
        fclose(fp);
        printf("[cpu] Set to minimum frequency (powersave mode)\n");
    } else {
        printf("[cpu] Failed to set powersave governor\n");
    }
}

// 恢复原始CPU governor
static void restoreCpuFreq(void) {
    if (original_governor[0] != 0) {
        FILE *fp = fopen("/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor", "w");
        if (fp) {
            fprintf(fp, "%s", original_governor);
            fclose(fp);
            printf("[cpu] Restored governor: %s\n", original_governor);
        } else {
            printf("[cpu] Failed to restore governor\n");
        }
    } else {
        printf("[cpu] No original governor to restore\n");
    }
}
void sysDeepSleep(void){
	deepSleep = true;
    sleepTs   = -1;
    // 降低CPU频率到最低，保持浅睡眠逻辑（LCD和触摸屏已关闭）
    setCpuMinFreq();

    // 按电源键会醒过来，继续执行下面的代码
}

void sysWake(void){
        deepSleep = false;
        sleepTs = -1;
        // 恢复CPU频率
        restoreCpuFreq();
        // 打开触摸屏和LCD
        touchOpen();
        lcdOpen();
}
void setDontDeepSleep(bool b){
    dontDeepSleep = b;
}

int main(int argc, char *argv[])
{
  bool isDaemonMode = false;
  system("killall  robotd");
  system("killall -SIGSTOP robot_run_1");
    for (uint32_t i = 0; i < argc; i++)
    {
        char * arg = argv[i];
        printf("argv[%d] = %s\n", i, arg);
        if(strcmp(arg, "-d") == 0) {
            isDaemonMode = false;
        }

        if(strcmp(arg, "-w") == 0) {
            daemon(1, 0);
            switchBackground();
            while(1) {
                usleep(25000);
                readKeyHome();
            }
        }
    }

  powerd = open("/dev/input/event0", O_RDWR);
  fcntl(powerd, 4,2048);
  homed = open("/dev/input/event2", O_RDWR);
  fcntl(homed, 4,2048);
  disphd = open("/dev/disp", O_RDWR);
  fbd = open("/dev/fb0" , O_RDWR);
  getcwd(homepath, PATH_MAX_LENGTH);
  setenv("TZ", "CST-8", 1);
  tzset();

  if(isDaemonMode) daemon(1,0);

  lcdRefresh();
  lv_init();
  lv_linux_disp_init();
  printf("display OK!\n");
  lv_linux_touch_init();
  printf("init OK\n");
  /*Initialized LVGL*/
  


  lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(0xFFFFFF), 0);

  create_container();
  button();
  create_test_button();
  //lv_demo_widgets();

  while(1) {
        readKeyHome();
        if(backgroundTs == -1){
            readKeyPower();
         	if(sleepTs == -1) {
            	lv_timer_handler();
	            usleep(5000);
            }
            else {
                if(dontDeepSleep) 
                    sleepTs = tick_get();

                else if(!deepSleep && tick_get() - sleepTs >= 60000) 
                    sysDeepSleep();
                
                usleep(25000);
            }
        }
        else {
            usleep(25000);
        }
    }
  close(disphd);
  close(powerd);
  close(homed);
  close(fbd);
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
    uint64_t now_ms;
    now_ms = (tv_now.tv_sec * 1000000 + tv_now.tv_usec) / 1000;

    uint32_t time_ms = now_ms - start_ms;
    return time_ms;
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
    uint32_t now_ms;
    now_ms = (tv_now.tv_sec * 1000000 + tv_now.tv_usec) / 1000;

    uint32_t time_ms = now_ms - start_ms;
    return time_ms;
}
