
#include <stdbool.h>

extern void sysSleep(void);
extern void sysWake(void);
extern void sysDeepSleep(void);
extern void switchRobot(void);
extern void switchBackground(void);
extern void switchForeground(void);
extern void lcdClose(void);
extern void lcdOpen(void);
extern void lcdRefresh(void);
extern void touchOpen(void);
extern void touchClose(void);
extern void setDontDeepSleep(bool b);