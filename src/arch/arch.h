#ifndef ARCH_H
#define ARCH_H

#include <stdbool.h>
#include <stdint.h>

typedef struct _lv_display_t lv_display_t;

#define PATH_MAX_LENGTH 256

extern char homepath[PATH_MAX_LENGTH];
extern int32_t  sleepTs;
extern uint32_t homeClickTs;
extern uint32_t backgroundTs;
extern bool     deepSleep;
extern uint8_t  dontDeepSleepEnabled;
extern uint8_t  dontTimeoutEnabled;
extern lv_display_t *disp;

extern int disphd;
extern int fbd;
extern int homed;
extern int powerd;

void arch_device_init(void);
void arch_deinit(void);

void arch_display_init(void);
void arch_touch_init(void);

void arch_lcd_open(void);
void arch_lcd_close(void);
void arch_lcd_refresh(void);
void arch_lcd_set_brightness(int brightness);
uint32_t arch_lcd_get_brightness(void);

void arch_touch_open(void);
void arch_touch_close(void);

void arch_read_key_home(void);
void arch_read_key_power(void);

void arch_deep_sleep(void);
void arch_cpu_powersave(void);
void arch_cpu_restore(void);

void arch_lcd_detect_timeout(void);

uint32_t arch_timer_handler(void);

#endif
