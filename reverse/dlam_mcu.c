#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/select.h>

#define SYNC 0xA5
#define END  0x5A

static const uint32_t crcKey = 0x35769521;
static uint32_t T[256], ok;

static void minit(void){for(int i=0;i<256;i++){uint32_t c=i;for(int j=0;j<8;j++)c=(c&1)?(c>>1)^0xEDB88320:(c>>1);T[i]=c;}ok=1;}
static uint32_t mcrc(const uint8_t *d,int n){if(!ok)minit();uint32_t c=~crcKey;while(n--)c=T[(c^*d++)&0xFF]^(c>>8);return ~c;}

static void send_frame(int fd,uint8_t m,uint8_t s,uint8_t a){
    uint8_t b[15];
    b[0]=SYNC;b[1]=0x01;b[2]=5;b[3]=0;
    b[4]=m;b[5]=s;b[6]=a;b[7]=0;b[8]=0;
    uint32_t crc=mcrc(b+4,5);
    b[9]=crc;b[10]=crc>>8;b[11]=crc>>16;b[12]=crc>>24;
    b[13]=END;
    write(fd,b,14);
    fprintf(stderr,"TX ");
    for(int i=0;i<14;i++)fprintf(stderr,"%02x ",b[i]);
    fprintf(stderr,"\n");
}

static void hex(const uint8_t *d,int n){for(int i=0;i<n;i++)fprintf(stderr,"%02x ",d[i]);}

int main(int argc,char **argv){
    unsigned m=1,s=0x07,a=1;
    if(argc>1)m=strtol(argv[1],NULL,16)&0xFF;
    if(argc>2)s=strtol(argv[2],NULL,16)&0xFF;
    if(argc>3)a=atoi(argv[3])&0xFF;

    int fd=open("/dev/ttyS3",O_RDWR|O_NOCTTY|O_LARGEFILE);
    if(fd<0){perror("open");return 1;}
    struct termios t;memset(&t,0,sizeof(t));    cfmakeraw(&t);
    t.c_cflag|=B921600|CLOCAL|CREAD|CRTSCTS;
    t.c_cc[VMIN]=0;t.c_cc[VTIME]=5;
    tcsetattr(fd,TCSANOW,&t);tcflush(fd,TCIOFLUSH);

    // Round 1
    send_frame(fd,m,s,a);
    usleep(200000);

    uint8_t r[4096];int tot=0;
    struct timeval tv={0,300000};fd_set fds;
    FD_ZERO(&fds);FD_SET(fd,&fds);
    if(select(fd+1,&fds,NULL,NULL,&tv)>0)tot=read(fd,r,sizeof(r));
    if(tot)for(int o=0;o+14<=tot;){
        if(r[o]!=SYNC){o++;continue;}
        int tl=r[o+2]|((uint16_t)r[o+3]<<8),dl=r[o+7]|((uint16_t)r[o+8]<<8);
        int fl=14+dl;
        if(fl>tot-o||r[o+fl-1]!=END){o++;continue;}
        fprintf(stderr,"B  (%02x,%02x) ack=%d dlen=%d [",r[o+4],r[o+5],r[o+6],dl);
        hex(r+o+9,dl);fprintf(stderr,"]\n");
        o+=fl;
    }
    tcflush(fd,TCIOFLUSH);

    // Round 2
    send_frame(fd,m,s,a);
    usleep(200000);

    tot=0;
    tv.tv_sec=1;tv.tv_usec=0;
    FD_ZERO(&fds);FD_SET(fd,&fds);
    if(select(fd+1,&fds,NULL,NULL,&tv)>0)tot=read(fd,r,sizeof(r));
    if(tot)for(int o=0;o+14<=tot;){
        if(r[o]!=SYNC){o++;continue;}
        int tl=r[o+2]|((uint16_t)r[o+3]<<8),dl=r[o+7]|((uint16_t)r[o+8]<<8);
        int fl=14+dl;
        if(fl>tot-o||r[o+fl-1]!=END){o++;continue;}
        fprintf(stderr,"R  (%02x,%02x) ack=%d dlen=%d [",r[o+4],r[o+5],r[o+6],dl);
        hex(r+o+9,dl);fprintf(stderr,"]\n");
        o+=fl;
    }
    close(fd);
    return 0;
}
