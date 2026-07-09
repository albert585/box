#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/select.h>

#define SOCK_PATH "/tmp/dlam_bridge.sock"

int main(int argc, char **argv) {
    int sock = socket(AF_UNIX, SOCK_STREAM, 0);
    struct sockaddr_un addr; memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX; strcpy(addr.sun_path, SOCK_PATH);
    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("connect"); return 1;
    }

    char buf[256]; int n = 0;
    if (argc > 1) n += sprintf(buf + n, "%s", argv[1]);
    if (argc > 2) n += sprintf(buf + n, " %s", argv[2]);
    if (argc > 3) n += sprintf(buf + n, " %s", argv[3]);
    write(sock, buf, n);

    char rbuf[4096]; int total = 0;
    struct timeval tv; fd_set fds;
    while (1) {
        FD_ZERO(&fds); FD_SET(sock, &fds);
        tv.tv_sec = 2; tv.tv_usec = 0;
        if (select(sock + 1, &fds, NULL, NULL, &tv) <= 0) break;
        int r = read(sock, rbuf + total, sizeof(rbuf) - total - 1);
        if (r <= 0) break;
        total += r;
    }
    if (total > 0) { rbuf[total] = 0; printf("%s", rbuf); }
    else printf("{\"error\":\"timeout\"}\n");

    close(sock);
    return 0;
}
