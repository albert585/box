/*
 * fake_dlam_printer.c — Minimal dlamPrinter replacement
 *
 * Protocol: Unix socket JSON in → UART binary out to MCU → response logged + forwarded
 * UART frame: [A5][type][cmd][ack][len_hi][len_lo][data...][CRC32][5A]
 *
 * Build: arm-unknown-linux-musleabihf-gcc -static -s -o fake_dlam_printer fake_dlam_printer.c
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <termios.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <poll.h>
#include <signal.h>
#include <stdint.h>

#define SOCK_PATH  "/data/print.server"
#define UART_DEV   "/dev/ttyS3"
#define BAUD_RATE  B115200
#define BUF_SIZE   8192

static int uart_fd   = -1;
static int client_fd = -1;
static int verbose   = 1;

static void hex_dump(const uint8_t *data, int len)
{
    printf("[%d bytes]\n", len);
    for (int i = 0; i < len; i++) {
        printf("%02x ", data[i]);
        if ((i + 1) % 16 == 0) printf("\n");
    }
    if (len % 16) printf("\n");
    /* try ascii */
    printf("  ascii: ");
    for (int i = 0; i < len && i < 128; i++) {
        char c = data[i];
        putchar((c >= 32 && c < 127) ? c : '.');
    }
    printf("\n");
}

#define CRC_SEED 0x35769521

static uint32_t crc_table[256];
static int crc_ready = 0;

static void crc_init(void)
{
    if (crc_ready) return;
    for (int i = 0; i < 256; i++) {
        uint32_t c = i;
        for (int j = 0; j < 8; j++)
            c = (c >> 1) ^ ((c & 1) ? 0xEDB88320UL : 0);
        crc_table[i] = c;
    }
    crc_ready = 1;
}

static uint32_t crc_calc(uint32_t seed, const uint8_t *buf, int len)
{
    crc_init();
    uint32_t c = ~seed;
    for (int i = 0; i < len; i++)
        c = crc_table[(c ^ buf[i]) & 0xFF] ^ (c >> 8);
    return ~c;
}

static int uart_init(const char *dev, speed_t speed)
{
    int fd = open(dev, O_RDWR | O_NOCTTY);
    if (fd < 0) { perror("open uart"); return -1; }
    struct termios tty;
    tcgetattr(fd, &tty);
    cfsetispeed(&tty, speed);
    cfsetospeed(&tty, speed);
    tty.c_cflag = CS8 | CLOCAL | CREAD;
    tty.c_iflag = IGNPAR;
    tty.c_oflag = 0;
    tty.c_lflag = 0;
    tty.c_cc[VMIN]  = 1;
    tty.c_cc[VTIME] = 10;
    tcsetattr(fd, TCSANOW, &tty);
    return fd;
}

static int build_packet(uint8_t type, uint8_t cmd, uint8_t ack,
                         const uint8_t *payload, uint16_t payload_len, uint8_t *out)
{
    int idx = 0;
    out[idx++] = 0xA5;              /* marker */
    out[idx++] = 0x01;              /* version */
    uint16_t total = payload_len + 5;
    out[idx++] = total & 0xFF;      /* total_len_lo */
    out[idx++] = (total >> 8) & 0xFF; /* total_len_hi */
    out[idx++] = type;
    out[idx++] = cmd;
    out[idx++] = ack;
    out[idx++] = payload_len & 0xFF;  /* data_len_lo */
    out[idx++] = (payload_len >> 8) & 0xFF; /* data_len_hi */
    if (payload && payload_len) {
        memcpy(out + idx, payload, payload_len);
        idx += payload_len;
    }
    uint32_t c = crc_calc(CRC_SEED, out + 4, payload_len + 5);
    out[idx++] = c & 0xFF;
    out[idx++] = (c >> 8) & 0xFF;
    out[idx++] = (c >> 16) & 0xFF;
    out[idx++] = (c >> 24) & 0xFF;
    out[idx++] = 0x5A;
    return idx;
}

static int parse_packet(const uint8_t *buf, int len, uint8_t *out_header, uint8_t *out_data, int out_max)
{
    if (len < 14) return -1; /* min: 9 header + 0 payload + 4 CRC + 1 end */
    int start = 0;
    while (start < len && buf[start] != 0xA5) start++;
    if (start + 14 > len) return -1;

    uint16_t total = (buf[start + 2] << 8) | buf[start + 3];
    int total_frame = total + 9; /* 9 header + total(data+cmd header) + 4 CRC + 1 end */
    if (start + total_frame > len) return -1;
    if (buf[start + total_frame - 1] != 0x5A) return -1;

    uint16_t data_len = (buf[start + 7] << 8) | buf[start + 8];

    /* CRC over [type][cmd][ack][data_len_lo][data_len_hi][data...] */
    uint32_t expected = crc_calc(CRC_SEED, buf + start + 4, data_len + 5);
    uint32_t actual = (buf[start + 9 + data_len] << 24) |
                      (buf[start + 10 + data_len] << 16) |
                      (buf[start + 11 + data_len] << 8)  |
                       buf[start + 12 + data_len];
    if (expected != actual) {
        fprintf(stderr, "CRC err: got %08x exp %08x\n", actual, expected);
        return -1;
    }

    if (out_header) {
        out_header[0] = buf[start + 4]; /* type */
        out_header[1] = buf[start + 5]; /* cmd */
        out_header[2] = buf[start + 6]; /* ack */
        out_header[3] = data_len & 0xFF;
        out_header[4] = (data_len >> 8) & 0xFF;
    }
    int copy = data_len < out_max ? data_len : out_max;
    if (copy > 0 && out_data) memcpy(out_data, buf + start + 9, copy);
    return copy;
}

static int json_get_int(const char *json, const char *key, int def)
{
    char k[64];
    snprintf(k, sizeof(k), "\"%s\"", key);
    const char *p = strstr(json, k);
    if (!p) return def;
    p += strlen(k);
    while (*p == ' ' || *p == ':' || *p == '\t') p++;
    return atoi(p);
}

static void json_get_str(const char *json, const char *key, char *out, int max)
{
    char k[64];
    snprintf(k, sizeof(k), "\"%s\"", key);
    const char *p = strstr(json, k);
    if (!p) { out[0] = 0; return; }
    p += strlen(k);
    while (*p == ' ' || *p == ':' || *p == '\t') p++;
    if (*p != '"') { out[0] = 0; return; }
    p++;
    int i = 0;
    while (*p && *p != '"' && i < max - 1) out[i++] = *p++;
    out[i] = 0;
}

static void handle_cmd(const char *json)
{
    int cmd = json_get_int(json, "cmd", -1);
    if (cmd < 0) return;
    if (verbose) printf("[JSON -> MCU] cmd=%d\n", cmd);

    uint8_t pkt[2048];
    int pkt_len = 0;
    uint8_t payload[512];
    char str[256];

    switch (cmd) {
    case 0: {
        char path[256] = {0};
        int fmt = json_get_int(json, "fmt", 0);
        int type = json_get_int(json, "type", 1);
        json_get_str(json, "data", path, sizeof(path));
        printf("[cmd] print fmt=%d type=%d file=%s\n", fmt, type, path);
        /* original: reads BMP → MMJ_PrinterImgBin → sends strips as cmd 0x1b
           requires libMb.so. For now: skip and just acknowledge. */
        snprintf(str, sizeof(str), "{\"cmd\":0,\"status\":0}");
        if (client_fd > 0) write(client_fd, str, strlen(str));
        return;
    }
    case 1: case 2: case 3: case 4: case 5: case 6: case 7:
    case 10: case 11:
        pkt_len = build_packet(5, cmd, 1, NULL, 0, pkt);
        break;
    case 8:
        printf("[cmd] OTA mode triggered\n");
        return;
    case 9:
        printf("[cmd] Bluetooth+GATT reset\n");
        return;
    case 12:
        printf("[cmd] status check (BT/GATT)\n");
        snprintf(str, sizeof(str), "{\"cmd\":12,\"status\":1}");
        if (client_fd > 0) write(client_fd, str, strlen(str));
        return;
    case 60: json_get_str(json, "value", str, sizeof(str));
        pkt_len = build_packet(1, 3, 1, (uint8_t *)str, strlen(str), pkt); break;
    case 61: json_get_str(json, "value", str, sizeof(str));
        sscanf(str, "%x:%x:%x:%x:%x:%x",
               (int*)&payload[0],(int*)&payload[1],(int*)&payload[2],
               (int*)&payload[3],(int*)&payload[4],(int*)&payload[5]);
        pkt_len = build_packet(3, 7, 1, payload, 6, pkt); break;
    case 62: json_get_str(json, "value", str, sizeof(str));
        pkt_len = build_packet(4, 3, 1, (uint8_t *)str, strlen(str), pkt); break;
    case 63: { int v = json_get_int(json, "value", 75);
        pkt_len = build_packet(5, 0x11, 1, (uint8_t *)&v, 1, pkt); break; }
    case 64: case 65: json_get_str(json, "value", str, sizeof(str));
        pkt_len = build_packet(1, (cmd == 64) ? 5 : 0x18, 1, (uint8_t *)str, strlen(str), pkt); break;
    case 280 ... 290: {
        static const int acts[] = {10,10,11,16,8,10,31,10,12,13,10};
        int idx = cmd - 280;
        if (idx < (int)(sizeof(acts)/sizeof(acts[0])))
            pkt_len = build_packet(5, acts[idx], 1, NULL, 0, pkt);
        break;
    }
    default:
        printf("[cmd] unknown cmd=%d, forwarding raw\n", cmd);
        { uint8_t t = json_get_int(json, "type", 5);
          pkt_len = build_packet(t, cmd, 1, NULL, 0, pkt); }
        break;
    }

    if (pkt_len > 0) {
        if (verbose) hex_dump(pkt, pkt_len);
        write(uart_fd, pkt, pkt_len);
    }
}

static void handle_mcu_data(void)
{
    uint8_t rbuf[4096], header[5], data[2048];
    int n = read(uart_fd, rbuf, sizeof(rbuf));
    if (n <= 0) return;

    printf("\n[MCU ->] raw %d bytes:\n", n);
    hex_dump(rbuf, n);

    /* single-byte MCU status (not framed) */
    if (n == 1) {
        const char *status = "?";
        switch (rbuf[0]) {
        case 0xFC: status = "lid_open"; break;
        case 0xFE: status = "cover_open"; break;
        default:   status = "unknown_status";
        }
        printf("[MCU ->] status: %s\n", status);
        return;
    }

    int data_len = parse_packet(rbuf, n, header, data, sizeof(data));
    if (data_len >= 0) {
        uint8_t type = header[0], cmd = header[1], ack = header[2];
        printf("[MCU ->] type=%d cmd=%d ack=%d data_len=%d\n", type, cmd, ack, data_len);
        hex_dump(data, data_len);

        /* convert to JSON like original dlamPrinter */
        char resp[2048];
        int n = 0;
        if (data_len > 0 && data[0] < 128) {
            /* text/string data */
            data[data_len] = 0;
            n = snprintf(resp, sizeof(resp),
                "{\"cmd\":100,\"ack\":%d,\"type\":%d,\"data\":\"%s\"}",
                ack, type, data);
        } else {
            /* numeric/binary data */
            int val = 0;
            for (int i = 0; i < data_len && i < 4; i++)
                val = (val << 8) | data[i];
            n = snprintf(resp, sizeof(resp),
                "{\"cmd\":100,\"ack\":%d,\"type\":%d,\"data\":%d}",
                ack, type, val);
        }
        if (n > 0) {
            printf("[MCU ->] json: %s\n", resp);
            if (client_fd > 0)
                write(client_fd, resp, n);
        }
    } else {
        printf("[MCU ->] parse failed (CRC err or incomplete)\n");
    }
}

static void handle_client_data(void)
{
    char buf[BUF_SIZE];
    int n = recv(client_fd, buf, sizeof(buf) - 1, 0);
    if (n <= 0) {
        printf("client disconnected\n");
        close(client_fd);
        client_fd = -1;
        return;
    }
    buf[n] = 0;
    printf("[socket ->] %d bytes\n", n);

    for (int i = 0; i < n; i++) {
        if (buf[i] == '{') {
            int depth = 1, j = i + 1;
            while (j < n && depth > 0) {
                if (buf[j] == '{') depth++;
                if (buf[j] == '}') depth--;
                j++;
            }
            if (depth == 0) {
                buf[j] = 0;
                printf("[JSON] %s\n", buf + i);
                handle_cmd(buf + i);
                i = j;
            }
        }
    }
}

static int unix_listen(const char *path)
{
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) { perror("socket"); return -1; }
    unlink(path);
    struct sockaddr_un addr = {.sun_family = AF_UNIX};
    strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);
    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind"); close(fd); return -1;
    }
    if (listen(fd, 1) < 0) {
        perror("listen"); close(fd); return -1;
    }
    chmod(path, 0666);
    return fd;
}

int main(int argc, char *argv[])
{
    const char *sock = SOCK_PATH, *uart = UART_DEV;
    if (argc > 1) {
        const char *a1 = argv[1];
        if (argc > 2) {
            /* 3+ arg modes: "cmd_mode <string>", "print_file <path>", "baud <path>" */
            if (strcmp(a1, "cmd_mode") == 0) {
                printf("cmd_mode: %s\n", argv[2]);
                return 0;
            }
            if (strcmp(a1, "print_file") == 0) {
                printf("print_file: %s\n", argv[2]);
                return 0;
            }
            if (argc > 3) uart = argv[3];
            sock = argv[1];
        } else {
            sock = argv[1];
        }
    }

    signal(SIGPIPE, SIG_IGN);

    uart_fd = uart_init(uart, BAUD_RATE);
    if (uart_fd < 0) return 1;

    int srv = unix_listen(sock);
    if (srv < 0) return 1;

    printf("fake_dlam_printer: %s <-> %s\n", sock, uart);

    while (1) {
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(srv, &rfds);
        FD_SET(uart_fd, &rfds);
        if (client_fd > 0) FD_SET(client_fd, &rfds);

        int max = srv > uart_fd ? srv : uart_fd;
        if (client_fd > max) max = client_fd;

        if (select(max + 1, &rfds, NULL, NULL, NULL) < 0) {
            if (errno == EINTR) continue;
            break;
        }

        if (FD_ISSET(srv, &rfds)) {
            if (client_fd > 0) { close(client_fd); client_fd = -1; }
            client_fd = accept(srv, NULL, NULL);
            printf(client_fd > 0 ? "client connected\n" : "accept failed\n");
        }

        if (client_fd > 0 && FD_ISSET(client_fd, &rfds))
            handle_client_data();

        if (FD_ISSET(uart_fd, &rfds))
            handle_mcu_data();
    }
    return 0;
}
