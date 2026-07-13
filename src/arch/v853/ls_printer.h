#ifndef LS_PRINTER_H
#define LS_PRINTER_H

#include <stdbool.h>
#include <stddef.h>

#define LS_PRINT_SOCK  "/tmp/printer_print.sock"
#define LS_LTE_SOCK    "/tmp/printer_lte.sock"

enum {
    LS_CMD_MCU_VER        = 3,
    LS_CMD_PAPER_TYPE     = 4,
    LS_CMD_PRINT_STATUS   = 5,
    LS_CMD_BT_SCAN        = 9,
    LS_CMD_QUERY_STATUS   = 10,
    LS_CMD_BT_STATUS      = 12,
    LS_CMD_SET_PAGE_WIDTH = 66,

    LS_CMD_LTE_STATUS     = 280,
    LS_CMD_LTE_POWER_OFF  = 281,
    LS_CMD_LTE_STATE      = 282,
};

enum {
    LS_ACK_STARTUP     = 3,
    LS_ACK_MCU_VERSION = 3,
    LS_ACK_PAPER_OPEN  = 101,
    LS_ACK_PAPER_CLOSE = 102,
    LS_ACK_PAPER_STATE = 106,
    LS_ACK_BATTERY     = 109,
    LS_ACK_LTE_READY   = 111,
    LS_ACK_SIM_STATE   = 224,
    LS_ACK_SPP_STATUS  = 220,
    LS_ACK_LTE_STATE   = 222,
};

typedef void (*ls_recv_cb_t)(int cmd, int ack, const char *json);
int ls_printer_init(const char *print_sock, const char *lte_sock);
void ls_printer_deinit(void);
void ls_printer_set_callback(ls_recv_cb_t cb);

int ls_print_send(int cmd);
int ls_print_send_data(int cmd, int data);
int ls_print_send_str(int cmd, const char *data);

int ls_lte_send(int cmd);
int ls_lte_send_data(int cmd, int data);

#endif
