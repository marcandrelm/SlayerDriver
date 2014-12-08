#include <stdio.h>
#include <fcntl.h>
#include <stropts.h>
#include <linux/ioctl.h>

#define BUFF_IOC_MAGIC   (45)
#define BUFF_IOC_MAX     (0x6F)

#define IOCTL_GET               _IOR (BUFF_IOC_MAGIC, 0x10, int)
#define IOCTL_SET               _IOW (BUFF_IOC_MAGIC, 0x20, int)
#define IOCTL_STREAMON          _IOW (BUFF_IOC_MAGIC, 0x30, int)
#define IOCTL_STREAMOFF         _IOW (BUFF_IOC_MAGIC, 0x40, int)
#define IOCTL_GRAB              _IOR (BUFF_IOC_MAGIC, 0x50, int)
#define IOCTL_PANTILT           _IOW (BUFF_IOC_MAGIC, 0x60, int)
#define IOCTL_PANTILT_RESET     _IO  (BUFF_IOC_MAGIC, 0x61)



int main (void){
    int fd = open("/dev/elecam*", O_RDWR);
    if(fd<0){
        perror("open");
        return fd;
    }
    //unsigned char arg[] = {0x00, 0x00, 0x80, 0xFF};
    /*if(ioctl(fd, IOCTL_PANTILT, &arg) <0){
        perror("IOCTL");
        return -1;
    }
    */
    //arg[0] = 0x03; 
    if(ioctl(fd, IOCTL_PANTILT_RESET, 0) <0){
        perror("IOCTL");
        return -1;
    }
    return 0;
}
