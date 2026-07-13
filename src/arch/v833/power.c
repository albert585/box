#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "arch.h"

void arch_cpu_powersave(void) {}

void arch_cpu_restore(void) {}

void arch_deep_sleep(void)
{
    deepSleep = true;
    sleepTs   = -1;
    arch_cpu_powersave();
}
