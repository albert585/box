#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "arch.h"
#include "common/battery/battery_manager.h"

void arch_cpu_powersave(void) {}

void arch_cpu_restore(void) {}

void arch_deep_sleep(void)
{
    if(battery_get_status() != BATTERY_DISCHARGING) return;

    deepSleep = true;
    sleepTs   = -1;
    arch_cpu_powersave();

    char buffer[16] = {0};
    while(read(powerd, buffer, 0x10u) > 0);
    while(read(homed, buffer, 0x10u) > 0);

    system("echo \"0\" >/sys/class/rtc/rtc0/wakealarm");
    system("echo \"0\" >/sys/class/rtc/rtc0/wakealarm");
    system("echo \"mem\" > /sys/power/state");

    deepSleep = false;
    sleepTs = -1;
    arch_cpu_restore();
    arch_touch_open();
    arch_lcd_open();
    arch_lcd_set_brightness(25);
    while(read(powerd, buffer, 0x10u) > 0);
}
