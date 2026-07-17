#include <unistd.h>
#include <fcntl.h>

int disphd = 0;
int fbd    = 0;
int homed  = 0;
int powerd = 0;

void arch_device_init(void)
{
    powerd = open("/dev/input/event0", O_RDWR);
    fcntl(powerd, 4, 2048);
    homed = open("/dev/input/event2", O_RDWR);
    fcntl(homed, 4, 2048);
    disphd = open("/dev/disp", O_RDWR);
    fbd    = open("/dev/fb0", O_RDWR);
}

void arch_deinit(void)
{
    if(disphd) close(disphd);
    if(powerd) close(powerd);
    if(homed)  close(homed);
    if(fbd)    close(fbd);
}
