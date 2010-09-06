/*
  ctrlbox2osc
  Copyright 2009, Jonatan Liljedahl
  
  Small app that listens to messages from custom USB controller
  and converts them to OSC.

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with This program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <stdio.h>  
#include <string.h> 
#include <unistd.h> 
#include <fcntl.h>  
#include <errno.h>  
#include <termios.h>
#include <lo/lo.h>
#include <string.h>

#define BAUDRATE B500000

// #define POLLING

static lo_address osc_addr;
int verbose = 0;
char *arg_port = "57120";
char *arg_dev = "/dev/ttyUSB0";

int open_serial_port(char *dev) {
#ifdef POLLING
    int fd = open(dev, O_RDWR | O_NOCTTY | O_NONBLOCK);
#else
    int fd = open(dev, O_RDWR | O_NOCTTY);
#endif
    if (fd == -1)
        return -1;

    struct termios options;
    if (tcgetattr(fd, &options) == -1) {
        printf("tcgetattr failed: %s - %s(%d).\n", dev, strerror(errno), errno);
        goto error;
    }

    speed_t baud = BAUDRATE;
    cfsetispeed(&options, baud);
    cfsetospeed(&options, baud);

    cfmakeraw(&options);
//    options.c_cflag |= CLOCAL;
    
    if (tcsetattr(fd, TCSANOW, &options) == -1) {
        printf("tcsetattr failed: %s - %s(%d).\n", dev, strerror(errno), errno);
        goto error;
    }
    
//    fcntl(fd,F_SETFL,0);
    return (fd);

error:
    if( fd != -1 )
        close(fd);
    return (-1);
}

static void do_cb(int type, int ch, unsigned int val) {
    char path[128];
    ch += 1;
    if(verbose)
        printf("type: %d ch: %02d val: %04d\n",type,ch,val);
    if(type==0) {
        sprintf(path,"/ctrlbox/analog/%d",ch);
        lo_send(osc_addr,path,"f",(float)val/1023.0f);
    } else {
        sprintf(path,"/ctrlbox/digital/%d",ch);
        lo_send(osc_addr,path,"i",val);
    }        
}

static int get_data(int fd) {
    unsigned char c;
    static int ch = 0, state = 0;
    static unsigned int val = 0;
    
//    if(verbose) printf("waiting for data...\n");
    int x = read(fd,&c,1);
//    printf("got data: %d\n",x);
    if(x<=0) {
#ifdef POLLING
        usleep(10000);
        return 1;
#else
        printf("ctrlbox: device disconnected\n");
        return 0;
#endif
    }
    if(c&128) {
        ch = c & 0x7f;
        state = (ch & 0x40) ? 3:1;
    } else {
        if(state==1) {
            val = c << 7;
            state=2;
        } else if(state==2) {
            val |= c & 0x7f;
            state=0;
            do_cb(0,ch,val);
        } else if(state==3) {
            val = c;
            state=0;
            do_cb(1,ch-0x40,val);
        }
    }
    return 1;
}

int main(int argc, char **argv) {
    if(argc>1 && strcmp(argv[1],"-v")==0) {
        printf("(Verbose mode)\n");
        verbose=1;
        argc--;
        argv++;
    }
    
    if(argc>1) arg_port = argv[1];
    if(argc>2) arg_dev = argv[2];

    int fd = open_serial_port(arg_dev);
    if(fd<0) {
        printf("Could not open serial device %s\n", arg_dev);
        return -1;
    } else {
        printf("Opened serial device %s\n", arg_dev);
    }
    
    printf(
        "Sending on osc.udp://localhost:%s/ctrlbox/analog/N (f) and digital/N (i)\n"
        "Use -v to dump values\n", arg_port
    );
    
    
    osc_addr = lo_address_new("localhost",arg_port);
    
    while(1)
        if(!get_data(fd)) break;

    close(fd);
    return 0;
}

