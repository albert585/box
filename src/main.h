#ifndef MAIN_H
#define MAIN_H

#include <stdbool.h>

extern void sysSleep(void);
extern void sysWake(void);
extern void setDontDeepSleep(bool b);
extern void setDontTimeout(bool b);
extern void switchRobot(void);
extern void switchBackground(void);
extern void switchForeground(void);

#endif
