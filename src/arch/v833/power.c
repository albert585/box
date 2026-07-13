#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "arch.h"

static char original_governor[32] = {0};

static void saveCpuFreq(void)
{
    FILE *fp = fopen("/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor", "r");
    if(fp) {
        if(fgets(original_governor, sizeof(original_governor), fp)) {
            original_governor[strcspn(original_governor, "\n")] = 0;
            printf("[cpu] Saved original governor: %s\n", original_governor);
        }
        fclose(fp);
    } else {
        printf("[cpu] Failed to read scaling_governor\n");
    }
}

void arch_cpu_powersave(void)
{
    saveCpuFreq();
    FILE *fp = fopen("/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor", "w");
    if(fp) {
        fprintf(fp, "powersave");
        fclose(fp);
        printf("[cpu] Set to minimum frequency (powersave mode)\n");
    } else {
        printf("[cpu] Failed to set powersave governor\n");
    }
}

void arch_cpu_restore(void)
{
    if(original_governor[0] != 0) {
        FILE *fp = fopen("/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor", "w");
        if(fp) {
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

void arch_deep_sleep(void)
{
    deepSleep = true;
    sleepTs   = -1;
    arch_cpu_powersave();
}
