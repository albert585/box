#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <time.h>
#include <sys/select.h>

#define FRAME_SYNC 0xA5
#define FRAME_END  0x5A
#define FRAME_HDR  9

static const uint32_t crcKey = 0x35769521;
static uint32_t crc32_table[256], crc_table_ready;

static void crc32_init(void) {
    for (int i = 0; i < 256; i++) {
        uint32_t c = i;
        for (int j = 0; j < 8; j++)
            c = (c & 1) ? (c >> 1) ^ 0xEDB88320 : (c >> 1);
        crc32_table[i] = c;
    }
    crc_table_ready = 1;
}

static uint32_t crc32_calc(const uint8_t *data, int len) {
    if (!crc_table_ready) crc32_init();
    uint32_t c = ~crcKey;
    while (len--) c = crc32_table[(c ^ *data++) & 0xFF] ^ (c >> 8);
    return ~c;
}

static int send_frame(int fd, uint8_t ver, uint8_t main, uint8_t sub,
                       uint8_t ack, const uint8_t *data, uint16_t datalen) {
    uint16_t total_len = 5 + datalen;
    uint8_t buf[512];
    uint16_t off = 0;
    buf[off++] = FRAME_SYNC;
    buf[off++] = ver;
    buf[off++] = total_len & 0xFF;
    buf[off++] = (total_len >> 8) & 0xFF;
    buf[off++] = main;
    buf[off++] = sub;
    buf[off++] = ack;
    buf[off++] = datalen & 0xFF;
    buf[off++] = (datalen >> 8) & 0xFF;
    if (datalen && data) { memcpy(buf + off, data, datalen); off += datalen; }
    uint32_t crc = crc32_calc(buf + 4, total_len);
    buf[off++] = crc & 0xFF;
    buf[off++] = (crc >> 8) & 0xFF;
    buf[off++] = (crc >> 16) & 0xFF;
    buf[off++] = (crc >> 24) & 0xFF;
    buf[off++] = FRAME_END;
    return write(fd, buf, off) == off ? 0 : -1;
}

static int read_voltage(void) {
    FILE *f = fopen("/sys/class/power_supply/battery/voltage_now", "r");
    if (!f) return 0;
    int v; fscanf(f, "%d", &v); fclose(f);
    return v / 1000;  // uV → mV
}

int main(void) {
    int fd = open("/dev/ttyS3", O_RDWR | O_NOCTTY | O_LARGEFILE);
    if (fd < 0) { perror("open"); return 1; }

    struct termios tty;
    memset(&tty, 0, sizeof(tty));
    cfmakeraw(&tty);
    cfsetspeed(&tty, 921600);
    tty.c_cflag |= CLOCAL | CREAD;
    tty.c_cc[VMIN] = 0;
    tty.c_cc[VTIME] = 5;
    tcsetattr(fd, TCSANOW, &tty);
    tcflush(fd, TCIOFLUSH);
    fprintf(stderr, "Listening on /dev/ttyS3 @ 921600\n");

    uint8_t rbuf[4096];
    int total = 0;
    time_t last_active = time(NULL);

    while (1) {
        fd_set fds;
        struct timeval tv = {1, 0};
        FD_ZERO(&fds); FD_SET(fd, &fds);
        select(fd + 1, &fds, NULL, NULL, &tv);

        int n = read(fd, rbuf + total, sizeof(rbuf) - total - 1);
        if (n <= 0) continue;
        total += n;
        last_active = time(NULL);

        // Parse frames
        int off = 0;
        while (off + FRAME_HDR + 5 <= total) {
            if (rbuf[off] != FRAME_SYNC) { off++; continue; }
            int rem = total - off;
            uint16_t tl = rbuf[off+2] | ((uint16_t)rbuf[off+3] << 8);
            if (tl > 0x500) { off++; continue; }
            int fl = FRAME_HDR + (tl - 5) + 5;
            if (fl > rem) break;

            uint8_t *frm = rbuf + off;
            if (frm[fl-1] != FRAME_END) { off++; continue; }

            uint16_t dlen = frm[7] | ((uint16_t)frm[8] << 8);
            if (tl != 5 + dlen) { off++; continue; }

            uint32_t calc = crc32_calc(frm + 4, tl);
            uint32_t recv = frm[9+dlen] | ((uint32_t)frm[9+dlen+1]<<8)
                          | ((uint32_t)frm[9+dlen+2]<<16) | ((uint32_t)frm[9+dlen+3]<<24);

            time_t now = time(NULL);
            struct tm *tm = localtime(&now);
            char ts[16];
            strftime(ts, sizeof(ts), "%H:%M:%S", tm);

            if (calc != recv) {
                fprintf(stderr, "%s CRC ERR (%02x,%02x)\n", ts, frm[4], frm[5]);
                off++; continue;
            }

            fprintf(stderr, "%s (%02x,%02x) ver=%02x ack=%d dlen=%d",
                    ts, frm[4], frm[5], frm[1], frm[6], dlen);
            if (dlen) { fprintf(stderr, " ["); for (int i=0;i<dlen;i++) fprintf(stderr,"%02x ",frm[9+i]); fprintf(stderr,"]"); }
            fprintf(stderr, "\n");

            // Handle MCU queries
            if (frm[6] == 1) {  // ack=1: MCU query
                if (frm[4] == 1 && frm[5] == 0x0c && dlen == 0) {
                    // Battery voltage query
                    int mv = read_voltage();
                    uint8_t vdata[2] = { mv & 0xFF, (mv >> 8) & 0xFF };
                    send_frame(fd, 0x01, 1, 0x0c, 2, vdata, 2);
                    fprintf(stderr, "%s RPLY (1,0c) voltage=%dmV\n", ts, mv);
                } else if (frm[4] == 1 && frm[5] == 0x0b && dlen == 0) {
                    // Battery capacity query
                    FILE *f = fopen("/sys/class/power_supply/battery/capacity", "r");
                    int cap = 0; if (f) { fscanf(f, "%d", &cap); fclose(f); }
                    uint8_t cdata[4] = { 0x01, cap & 0xFF, (cap >> 8) & 0xFF, 0x01 };
                    send_frame(fd, 0x01, 1, 0x0b, 2, cdata, 4);
                    fprintf(stderr, "%s RPLY (1,0b) capacity=%d%%\n", ts, cap);
                }
            }

            off += fl;
        }
        // Compact buffer
        if (off > 0) { memmove(rbuf, rbuf + off, total - off); total -= off; }
    }
    close(fd);
    return 0;
}
