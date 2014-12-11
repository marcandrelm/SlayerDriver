/* CamDriver.h

 Author        : Marc-Andre Lafaille Magnan

 Create Date   : 12:00:00 11/12/2014
 Project Name  : CamDriver
 Target Devices: Orbitaf AF
 Tool versions : kernel 3.13.0-40-generic, Eclipse Luna (4.4.1)
 Description   : camera usb driver

 Revision:
 Revision 0.01 - Constructing
 Additional Comments:

*/

#ifndef CAMDRIVER_H_
#define CAMDRIVER_H_

// Inclusion [Incl] -----------------------------------------------------------
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/kref.h>
#include <linux/spinlock.h>
#include <linux/semaphore.h>
#include <linux/usb.h>
#include <linux/uaccess.h>
//#ifdef ()
//#include <linux/sched/rt.h>
//#include <linux/export.h>
//#endif

#include "usbvideo.h"


// Define [Def] ---------------------------------------------------------------

#define DRIVER_TAG              "Slayer: "//"eleCam:"
#define DRIVER_NAME             "Slayer"  //"eleCam"

//IOCTL
#define BUFF_IOC_MAGIC   (45)
#define BUFF_IOC_MAX     (0x6F)

#define IOCTL_GET               _IOR (BUFF_IOC_MAGIC, 0x10, int) // Not Implemented in this version
#define IOCTL_SET               _IOW (BUFF_IOC_MAGIC, 0x20, int) // Not Implemented in this version
#define IOCTL_STREAMON          _IOW (BUFF_IOC_MAGIC, 0x30, int)
#define IOCTL_STREAMOFF         _IOW (BUFF_IOC_MAGIC, 0x40, int)
#define IOCTL_GRAB              _IOR (BUFF_IOC_MAGIC, 0x50, int)
#define IOCTL_PANTILT           _IOW (BUFF_IOC_MAGIC, 0x60, int)
#define IOCTL_PANTILT_RESET     _IO  (BUFF_IOC_MAGIC, 0x61)


#endif // CAMDRIVER_H_
