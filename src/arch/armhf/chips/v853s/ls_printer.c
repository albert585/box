#include "ls_printer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/select.h>
#include <errno.h>

#define RECV_BUF_SIZE 4096

static int fd_print = -1;
static int fd_lte   = -1;
static pthread_t recv_thread;
static volatile int running = 0;
static ls_recv_cb_t user_cb = NULL;

static char print_path[128];
static char lte_path[128];

static int parse_json_int(const char *json, const char *key, int def)
{
    char search[64];
    int n = snprintf(search, sizeof(search), "\"%s\":", key);
    const char *p = json;
    while ((p = strstr(p, search))) {
        p += n;
        if (*p == '-' || (*p >= '0' && *p <= '9'))
            return atoi(p);
        p++;
    }
    return def;
}

static int json_strip(const char *raw, char *out, size_t osize)
{
    const char *start = raw;
    while (*start && *start != '{') start++;
    const char *end = start;
    int depth = 0;
    while (*end) {
        if (*end == '{') depth++;
        else if (*end == '}') { depth--; if (depth == 0) { end++; break; } }
        end++;
    }
    size_t len = (size_t)(end - start);
    if (len >= osize) len = osize - 1;
    memcpy(out, start, len);
    out[len] = '\0';
    return len > 0 ? 0 : -1;
}

static int try_connect(const char *path)
{
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);

    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return -1;

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(fd);
        return -1;
    }
    return fd;
}

static int send_json(int fd, const char *json)
{
    if (fd < 0) return -1;
    size_t len = strlen(json);
    ssize_t n = send(fd, json, len, 0);
    return (n == (ssize_t)len) ? 0 : -1;
}

static int recv_json(int fd, char *buf, size_t size)
{
    ssize_t n = recv(fd, buf, size - 1, 0);
    if (n <= 0) return -1;
    buf[n] = '\0';
    return (int)n;
}

static void dispatch(const char *raw)
{
    if (!user_cb || !raw) return;

    char json[RECV_BUF_SIZE];
    if (json_strip(raw, json, sizeof(json)) < 0) return;

    int cmd = parse_json_int(json, "cmd", -1);
    int ack = parse_json_int(json, "ack", -1);

    if (cmd >= 0) {
        user_cb(cmd, ack, json);
    }
}

static void *recv_loop(void *arg)
{
    (void)arg;
    char buf[RECV_BUF_SIZE];

    while (running) {
        fd_set rfds;
        FD_ZERO(&rfds);
        int maxfd = -1;
        if (fd_print >= 0) { FD_SET(fd_print, &rfds); if (fd_print > maxfd) maxfd = fd_print; }
        if (fd_lte >= 0)   { FD_SET(fd_lte, &rfds);   if (fd_lte > maxfd)   maxfd = fd_lte; }

        if (maxfd < 0) { usleep(100000); continue; }

        struct timeval tv = {.tv_sec = 1, .tv_usec = 0};
        int ret = select(maxfd + 1, &rfds, NULL, NULL, &tv);
        if (ret < 0 && errno != EINTR) break;
        if (ret <= 0) continue;

        if (fd_print >= 0 && FD_ISSET(fd_print, &rfds)) {
            if (recv_json(fd_print, buf, sizeof(buf)) > 0) {
                printf("[ls] print rx: %s\n", buf);
                dispatch(buf);
            } else {
                close(fd_print); fd_print = -1;
                printf("[ls] print socket disconnected, reconnecting...\n");
            }
        }

        if (fd_lte >= 0 && FD_ISSET(fd_lte, &rfds)) {
            if (recv_json(fd_lte, buf, sizeof(buf)) > 0) {
                printf("[ls] lte rx: %s\n", buf);
                dispatch(buf);
            } else {
                close(fd_lte); fd_lte = -1;
                printf("[ls] lte socket disconnected, reconnecting...\n");
            }
        }

        if (fd_print < 0) {
            fd_print = try_connect(print_path);
            if (fd_print >= 0) printf("[ls] print socket connected\n");
        }
        if (fd_lte < 0) {
            fd_lte = try_connect(lte_path);
            if (fd_lte >= 0) printf("[ls] lte socket connected\n");
        }
    }
    return NULL;
}

int ls_printer_init(const char *print_sock, const char *lte_sock)
{
    snprintf(print_path, sizeof(print_path), "%s",
             print_sock ? print_sock : LS_PRINT_SOCK);
    snprintf(lte_path, sizeof(lte_path), "%s",
             lte_sock ? lte_sock : LS_LTE_SOCK);

    fd_print = try_connect(print_path);
    fd_lte   = try_connect(lte_path);

    printf("[ls] init: print=%s (%s), lte=%s (%s)\n",
           print_path, fd_print >= 0 ? "ok" : "fail",
           lte_path,   fd_lte >= 0   ? "ok" : "fail");

    running = 1;
    pthread_create(&recv_thread, NULL, recv_loop, NULL);
    return 0;
}

void ls_printer_deinit(void)
{
    running = 0;
    if (fd_print >= 0) { close(fd_print); fd_print = -1; }
    if (fd_lte >= 0)   { close(fd_lte);   fd_lte = -1; }
    pthread_join(recv_thread, NULL);
}

void ls_printer_set_callback(ls_recv_cb_t cb)
{
    user_cb = cb;
}

int ls_print_send(int cmd)
{
    char json[64];
    snprintf(json, sizeof(json), "{\"cmd\":%d}", cmd);
    printf("[ls] print tx: %s\n", json);
    return send_json(fd_print, json);
}

int ls_print_send_data(int cmd, int data)
{
    char json[64];
    snprintf(json, sizeof(json), "{\"cmd\":%d,\"data\":%d}", cmd, data);
    printf("[ls] print tx: %s\n", json);
    return send_json(fd_print, json);
}

int ls_print_send_str(int cmd, const char *data)
{
    char json[256];
    snprintf(json, sizeof(json), "{\"cmd\":%d,\"data\":\"%s\"}", cmd, data ? data : "");
    printf("[ls] print tx: %s\n", json);
    return send_json(fd_print, json);
}

int ls_lte_send(int cmd)
{
    char json[64];
    snprintf(json, sizeof(json), "{\"cmd\":%d}", cmd);
    printf("[ls] lte tx: %s\n", json);
    return send_json(fd_lte, json);
}

int ls_lte_send_data(int cmd, int data)
{
    char json[64];
    snprintf(json, sizeof(json), "{\"cmd\":%d,\"data\":%d}", cmd, data);
    printf("[ls] lte tx: %s\n", json);
    return send_json(fd_lte, json);
}
