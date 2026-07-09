#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <signal.h>
#include <pthread.h>
#include <sys/select.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/un.h>

#define SYNC 0xA5
#define END  0x5A
#define SOCK "/tmp/dlam_bridge.sock"

static const uint32_t crcKey=0x35769521;
static uint32_t T[256],ok;
static int uart_fd=-1,cli_fd=-1,run=1;
static pthread_mutex_t mu=PTHREAD_MUTEX_INITIALIZER;

static void mit(void){for(int i=0;i<256;i++){uint32_t c=i;for(int j=0;j<8;j++)c=(c&1)?(c>>1)^0xEDB88320:(c>>1);T[i]=c;}ok=1;}
static uint32_t mcrc(const uint8_t*d,int n){if(!ok)mit();uint32_t c=~crcKey;while(n--)c=T[(c^*d++)&0xFF]^(c>>8);return ~c;}

static void send_frame(uint8_t m,uint8_t s,uint8_t a,
                       const uint8_t*d,uint16_t dl){
    uint8_t b[512];int o=0;
    uint16_t tl=5+dl;
    b[o++]=SYNC;b[o++]=0x01;
    b[o++]=tl&0xFF;b[o++]=(tl>>8)&0xFF;
    b[o++]=m;b[o++]=s;b[o++]=a;
    b[o++]=dl&0xFF;b[o++]=(dl>>8)&0xFF;
    if(dl&&d){memcpy(b+o,d,dl);o+=dl;}
    uint32_t crc=mcrc(b+4,tl);
    b[o++]=crc;b[o++]=(crc>>8);b[o++]=(crc>>16);b[o++]=(crc>>24);
    b[o++]=END;
    write(uart_fd,b,o);
}

static int rvol(int*mv,int*cap){
    FILE*f=fopen("/sys/class/power_supply/battery/voltage_now","r");
    if(f){fscanf(f,"%d",mv);fclose(f);*mv/=1000;}
    f=fopen("/sys/class/power_supply/battery/capacity","r");
    if(f){fscanf(f,"%d",cap);fclose(f);}
    return(*mv>0);
}

static void *uth(void*a){
    uint8_t r[4096];int p=0;
    while(run){
        fd_set f;FD_ZERO(&f);FD_SET(uart_fd,&f);
        struct timeval tv={0,200000};
        if(select(uart_fd+1,&f,0,0,&tv)<=0)continue;
        int n=read(uart_fd,r+p,sizeof(r)-p-1);
        if(n<=0)continue;p+=n;

        int o=0;
        while(o+14<=p){
            if(r[o]!=SYNC){o++;continue;}
            int rm=p-o;
            uint16_t tl=r[o+2]|((uint16_t)r[o+3]<<8);
            if(tl>0x500){o++;continue;}
            int fl=14+(tl-5);
            if(fl>rm)break;
            uint8_t*f=r+o;
            if(f[fl-1]!=END){o++;continue;}
            uint16_t dl=f[7]|((uint16_t)f[8]<<8);
            if(tl!=5+dl){o++;continue;}
            uint32_t cr=f[9+dl]|((uint32_t)f[9+dl+1]<<8)
                     |((uint32_t)f[9+dl+2]<<16)|((uint32_t)f[9+dl+3]<<24);
            if(mcrc(f+4,tl)!=cr){o++;continue;}

            fprintf(stderr,"MCU (%02x,%02x) ack=%d dlen=%d [",f[4],f[5],f[6],dl);
            for(int i=0;i<dl;i++)fprintf(stderr,"%02x ",f[9+i]);
            fprintf(stderr,"]\n");

            pthread_mutex_lock(&mu);
            if(cli_fd>=0){
                char j[2048];int jl;
                jl=sprintf(j,"{\"m\":%d,\"s\":%d,\"ack\":%d,\"dlen\":%d,\"data\":\"",
                    f[4],f[5],f[6],dl);
                for(int i=0;i<dl&&jl<sizeof(j)-4;i++)jl+=sprintf(j+jl,"%02x",f[9+i]);
                jl+=sprintf(j+jl,"\"");

                // Decode events for (5,0x27) and (5,0x0F)
                if(((f[4]==5&&f[5]==0x27)||(f[4]==5&&f[5]==0x0f))&&dl>2){
                    int start=3;  // (5,0x27) has 3-byte header
                    if(f[4]==5&&f[5]==0x0f) start=0;  // (5,0x0F) no header
                    if(dl-start>=4){
                        jl+=sprintf(j+jl,",\"events\":[");
                        int ev=start,first=1;
                        while(ev+3<dl){
                            int tag=f[9+ev],val=f[9+ev+1]|(f[9+ev+2]<<8),flg=f[9+ev+3];
                        const char*n="?",*st="";
                        switch(tag){
                            case 1:n="temp";st=flg?"过热":"正常";break;
                            case 2:n="platen";st=flg?"关盖":"开盖";break;
                            case 3:n="paper";st=flg?"有纸":"缺纸";break;
                        }
                        jl+=sprintf(j+jl,"%s{\"tag\":%d,\"name\":\"%s\",\"val\":%d,\"flag\":%d,\"state\":\"%s\"}",
                            first?"":",",tag,n,val,flg,st);
                            first=0;ev+=4;
                        }
                        jl+=sprintf(j+jl,"]");
                    }
                }
                jl+=sprintf(j+jl,"}\n");
                int w=write(cli_fd,j,jl);
                if(w<=0){close(cli_fd);cli_fd=-1;fprintf(stderr,"cli gone\n");}
            }
            pthread_mutex_unlock(&mu);

            if(f[6]==1&&dl==0){
                if(f[4]==1&&f[5]==0x0c){int mv=0,cap=0;rvol(&mv,&cap);
                    uint8_t vd[2]={mv&0xFF,(mv>>8)&0xFF};send_frame(1,0x0c,2,vd,2);
                    fprintf(stderr,"  RPLY %dmV\n",mv);}
                else if(f[4]==1&&f[5]==0x0b){int mv=0,cap=0;rvol(&mv,&cap);
                    uint8_t cd[4]={0x01,cap,0,0x01};send_frame(1,0x0b,2,cd,4);
                    fprintf(stderr,"  RPLY %d%%\n",cap);}
            }
            o+=fl;
        }
        if(o>0){memmove(r,r+o,p-o);p-=o;}
    }
    return 0;
}

static void sigh(int s){run=0;}

int main(int argc,char**argv){
    signal(SIGINT,sigh);signal(SIGTERM,sigh);

    const char*dev="/dev/ttyS3";int baud=B921600;
    if(argc>1)dev=argv[1];
    if(argc>2){int b=atoi(argv[2]);if(b==9600)baud=B9600;else if(b==115200)baud=B115200;}

    uart_fd=open(dev,O_RDWR|O_NOCTTY|O_LARGEFILE);
    if(uart_fd<0){perror("open");return 1;}
    struct termios t;memset(&t,0,sizeof(t));cfmakeraw(&t);
    t.c_cflag|=baud|CLOCAL|CREAD|CRTSCTS;
    t.c_cc[VMIN]=0;t.c_cc[VTIME]=5;
    tcsetattr(uart_fd,TCSANOW,&t);tcflush(uart_fd,TCIOFLUSH);
    // Second tcsetattr matching dlamPrinter init
    ioctl(uart_fd,TCGETS,&t);tcflush(uart_fd,TCIFLUSH);
    tcsetattr(uart_fd,TCSANOW,&t);

    unlink(SOCK);
    int srv=socket(AF_UNIX,SOCK_STREAM,0);
    struct sockaddr_un a={.sun_family=AF_UNIX};
    strcpy(a.sun_path,SOCK);
    bind(srv,(struct sockaddr*)&a,sizeof(a));
    listen(srv,1);

    // Full init like dlamPrinter
    send_frame(1,0x07,1,0,0);usleep(200000);
    send_frame(5,0x0c,1,0,0);usleep(200000);
    send_frame(5,0x0d,1,0,0);usleep(200000);

    pthread_t th;pthread_create(&th,0,uth,0);

    fprintf(stderr,"Bridge: %s @ %d -> %s\n",dev,argc>2?atoi(argv[2]):921600,SOCK);

    while(run){
        fd_set f;FD_ZERO(&f);FD_SET(srv,&f);
        if(cli_fd>=0)FD_SET(cli_fd,&f);
        int mx=srv;if(cli_fd>mx)mx=cli_fd;
        struct timeval tv={0,200000};
        if(select(mx+1,&f,0,0,&tv)<=0)continue;

        if(FD_ISSET(srv,&f)){
            pthread_mutex_lock(&mu);
            if(cli_fd<0){cli_fd=accept(srv,0,0);fprintf(stderr,"cli connected\n");}
            pthread_mutex_unlock(&mu);
        }
        if(cli_fd>=0&&FD_ISSET(cli_fd,&f)){
            char b[256];int n=read(cli_fd,b,sizeof(b)-1);
            if(n<=0){pthread_mutex_lock(&mu);close(cli_fd);cli_fd=-1;
                pthread_mutex_unlock(&mu);fprintf(stderr,"cli gone\n");}
            else{
                b[n]=0;unsigned m=1,s=0x07,a=1;char*r;
                m=strtol(b,&r,16);if(r)s=strtol(r,&r,16);
                if(r){a=strtol(r,&r,16);if(a>2)a=1;}
                fprintf(stderr,"TX (%02x,%02x) ack=%d\n",m,s,a);
                send_frame(m,s,a,0,0);usleep(200000);send_frame(m,s,a,0,0);
            }
        }
    }
    unlink(SOCK);close(srv);close(uart_fd);return 0;
}
