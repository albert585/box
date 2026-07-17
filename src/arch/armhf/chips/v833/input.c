#include <unistd.h>
#include <stdio.h>
#include "arch/arch.h"
#include "main.h"

extern uint32_t custom_tick_get(void);

void arch_read_key_home(void)
{
    char buffer[16] = {0};
    while(read(homed, buffer, 0x10u) > 0) {
        if(buffer[10] != 0x73) return;

        if(buffer[12] == 0x00) {
            printf("[key]home_up\n");
            uint32_t ts = custom_tick_get();
            if(homeClickTs != -1 && ts - homeClickTs <= 300) {
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

void arch_read_key_power(void)
{
    char buffer[16] = {0};
    while(read(powerd, buffer, 0x10u) > 0) {
        if(buffer[10] != 0x74) return;

        if(buffer[12] == 0x00) {
            if(sleepTs == -1) {
                sysSleep();
            } else {
                sysWake();
            }
        }
    }
}
