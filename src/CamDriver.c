/*-----------------------------------------------------------------------------
  _   _ ____  ____    ____       _
 | | | / ___|| __ )  |  _ \ _ __(_)_   _____ _ __
 | | | \___ \|  _ \  | | | | '__| \ \ / / _ \ '__|
 | |_| |___) | |_) | | |_| | |  | |\ V /  __/ |
  \___/|____/|____/ _|____/|_| _|_| \_/ \___|_|
 | ____| |   | ____|___  ( _ )| || |
 |  _| | |   |  _|    / // _ \| || |_
 | |___| |___| |___  / /| (_) |__   _|
 |_____|_____|_____|/_/  \___/   |_|


 Engineer      : Marc-Andre Lafaille Magnan

 Create Date   : 11:45:00 10/11/2014
 Project Name  : CamDriver
 Target Devices: Orbitaf AF
 Tool versions : kernel 3.2.34etsele
 Description   : camera usb driver

 Revision:
 Revision 0.01 - Constructing
 Additional Comments:

-----------------------------------------------------------------------------*/

/*--- Table of content --------------------------------------------------------
    Inclusion           [Incl]
    Define              [Def]
    Licence             [Lic]
    Struct              [Strc]
    Enumeration         [Enum]
    Function            [Func]
    Global Variable     [GVar]
    Function definition [Fdef]
*/

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

// Licence [Lic] --------------------------------------------------------------
MODULE_AUTHOR  ("M-A. Lf. M., C.-A. G.");
MODULE_LICENSE ("Dual BSD/GPL");
MODULE_VERSION ("0.5");
MODULE_DESCRIPTION("Home made Logitec camera \"QuickCam® Orbit AF\" driver ");

// Define [Def] ---------------------------------------------------------------

#define DEBUG               (1)
#define SUCCESS             (0)
#define DRIVER_TAG          "Slayer: "//"eleCam:"
#define DRIVER_NAME         "Slayer"  //"eleCam"

#define TARGET_INTERFACE    (1)  // Specified by lab document
#define TARGET_CONFIG 		(4)  // Specified by lab document

// Vendor ID
#define USB_CAM_VENDOR_ID0  (0x046D)
#define USB_CAM_PRODUCT_ID0	(0x08CC)
#define USB_CAM_VENDOR_ID1  (0x046D)
#define USB_CAM_PRODUCT_ID1	(0x0994)

#define USB_CAM_MINOR_BASE  (192)

//IOCTL
#define BUFF_IOC_MAGIC   (45)
#define BUFF_IOC_MAX     (0x6F)

#define IOCTL_GET               _IOR (BUFF_IOC_MAGIC, 0x10, int)
#define IOCTL_SET               _IOW (BUFF_IOC_MAGIC, 0x20, int)
#define IOCTL_STREAMON          _IOW (BUFF_IOC_MAGIC, 0x30, int)
#define IOCTL_STREAMOFF         _IOW (BUFF_IOC_MAGIC, 0x40, int)
#define IOCTL_GRAB              _IOR (BUFF_IOC_MAGIC, 0x50, int)
#define IOCTL_PANTILT           _IOW (BUFF_IOC_MAGIC, 0x60, int)
#define IOCTL_PANTILT_RESET     _IO  (BUFF_IOC_MAGIC, 0x61)


#define to_cam_dev(d) container_of(d, struct usb_cam, kref)

/* Debug options ------------
 * print_alert() : Critical print ( no sleep)
 * print_warn()  : Warning  print (may sleep)
 * print_debug() : Debug    print (may sleep), are removed on Release
 * IS_CAPABLE()  : Verify if system Admin
*/
#define print_alert(...)  printk(KERN_ALERT   DRIVER_TAG __VA_ARGS__);
#define print_warn(...)   printk(KERN_WARNING DRIVER_TAG __VA_ARGS__);

#if (DEBUG > 0)
    #define print_debug(...)  printk(KERN_DEBUG DRIVER_TAG __VA_ARGS__);
    #define IS_CAPABLE  
#else
    #define print_debug(...)
    #define IS_CAPABLE  if(!capable (CAP_SYS_ADMIN)) { \
                            print_alert("Not Admin") \
                            return -EPERM; } 
 #endif

// Struct [Strc] --------------------------------------------------------------
static struct usb_device_id cam_table [] = {
	{ USB_DEVICE(USB_CAM_VENDOR_ID0, USB_CAM_PRODUCT_ID0) },
	{ USB_DEVICE(USB_CAM_VENDOR_ID1, USB_CAM_PRODUCT_ID1) },
	{ } // Terminating entry
};

struct usb_cam {
	struct usb_device *	    udev;	    // the usb device for this device
	struct usb_interface *	interface;  // the interface for this device
	unsigned char *	  bulk_in_buffer;   // the buffer to receive data
	size_t		 bulk_in_size;		    // the size of the receive buffer
	__u8		 bulk_in_endpointAddr;  // the address of the bulk in endpoint
	__u8		 bulk_out_endpointAddr; // the address of the bulk out endpoint
	__u8		 ID3_endpointAddr;      // reserved variable (must rename later)
	__u8		 ID4_endpointAddr;      // reserved variable (must rename later)
	__u8		 ID5_endpointAddr;      // reserved variable (must rename later)
	struct kref  kref; // Shared structure reference counter

	// TBD
	//struct mutex     	io_mutex;         	// synchronize I/O with disconnect
	//struct completion	bulk_in_completion;	// to wait for an ongoing read
};

// Enumeration [Enum] ---------------------------------------------------------

// Function [Func] ------------------------------------------------------------
int  __init cam_init(void);
void __exit cam_exit(void);

static int ele784_open (struct inode *inode, struct file *file);
int        cam_release (struct inode *inode, struct file *file);

static int  cam_probe (struct usb_interface *intf,
                 const struct usb_device_id *id);
void cam_disconnect   (struct usb_interface *interface);

void cam_delete       (struct kref *kref);

long    cam_ioctl (struct file *file, unsigned int cmd, unsigned long arg);

ssize_t cam_read  (struct file *file, char __user *buffer,
				size_t count, loff_t *ppos);

void cam_read_bulk_callback (struct urb *urb, struct pt_regs *regs);

void cam_grab (void); // Prototype TBD


// Global Variable [GVar] -----------------------------------------------------
struct file_operations cam_fops = {
	.owner =	THIS_MODULE,
	.read =		cam_read,
	.open =		ele784_open,
	.release =	cam_release,
	.unlocked_ioctl = cam_ioctl,
};

struct usb_driver cam_driver = {
	.name           = DRIVER_NAME,
	.id_table       = cam_table,
	.probe          = cam_probe,
	.disconnect     = cam_disconnect,
};

/*
  usb class driver info in order to get a minor number from the usb core,
  and to have the device registered with devfs and the driver core
*/
struct usb_class_driver cam_class = {
	.name = (DRIVER_NAME "%d"),
	.fops = &cam_fops,
	.minor_base = USB_CAM_MINOR_BASE,
};

static DEFINE_SPINLOCK(cam_lock);

// Function definition [Fdef] -------------------------------------------------

module_init (cam_init);
module_exit (cam_exit);

MODULE_DEVICE_TABLE (usb, cam_table); // Hotplug detect

/*-----------------------------------------------------------------------------
   func descp..




   note: usb_register is not used as KBUILD_MODNAME
   		 is not handled properly with Eclipse
*/
int __init cam_init(void) {
	int retval;

	print_debug("%s->Start\n",__FUNCTION__);
    retval = usb_register_driver(&cam_driver,THIS_MODULE, DRIVER_NAME);
	if (retval) {
	    print_alert("failed with error %d\n",retval);
	    return retval;
	}

    // Debug print for user space application
    print_warn("ioctl functions");
    print_warn("IOCTL_GET           : 0x%08x", IOCTL_GET);
    print_warn("IOCTL_SET           : 0x%08x", IOCTL_SET);
    print_warn("IOCTL_STREAMON      : 0x%08x", IOCTL_STREAMON);
    print_warn("IOCTL_STREAMOFF     : 0x%08x", IOCTL_STREAMOFF);
    print_warn("IOCTL_GRAB          : 0x%08x", IOCTL_GRAB);
    print_warn("IOCTL_PANTILT       : 0x%08x", IOCTL_PANTILT);
    print_warn("IOCTL_PANTILT_RESET : 0x%08x", IOCTL_PANTILT_RESET);

	print_debug("%s->Done\n", __FUNCTION__);
	return retval;
}

/*-----------------------------------------------------------------------------
   func descp..

*/
void __exit cam_exit(void) {
	print_debug("%s->Start\n",__FUNCTION__);
	usb_deregister(&cam_driver);
	print_debug("%s->Done\n",__FUNCTION__);
}

/*-----------------------------------------------------------------------------
   func descp..

*/
static int cam_probe(struct usb_interface *intf,
               const struct usb_device_id *id) {
	struct usb_cam *dev;
	const struct usb_host_interface *interface;
	int retval;

	print_debug("%s->Start\n",__FUNCTION__);

	dev = kmalloc(sizeof(struct usb_cam), GFP_KERNEL);
	if (dev == NULL) {
		print_alert("Out of memory");
		return -ENOMEM;
	}
	memset(dev, 0x00, sizeof (*dev));
	kref_init(&dev->kref);
	//mutex_init		(&dev->io_mutex);
	//init_completion	(&dev->bulk_in_completion);

	dev->udev = usb_get_dev(interface_to_usbdev(intf));

    interface = &intf->altsetting[0];
    if (interface->desc.bInterfaceClass    == USB_CLASS_VIDEO &&
       	interface->desc.bInterfaceSubClass == SC_VIDEOSTREAMING) {

        usb_set_intfdata(intf, dev->udev);

	    retval = usb_register_dev(intf, &cam_class);
	    if (retval) {
		    print_alert("Not able to get a minor for this device.");
		    usb_set_intfdata(intf, NULL);
		    goto ERROR;
	    }

	    usb_set_interface (dev->udev, TARGET_INTERFACE, TARGET_CONFIG);

    } else {
        print_alert("can not find proper descriptors");
        retval = -EBADR;
        goto ERROR;
    }

	print_debug("USB attached to %s-%d", DRIVER_NAME, intf->minor);
	return SUCCESS;

    ERROR:
	if (dev) { kref_put(&dev->kref, cam_delete); }
	kfree(dev);
	print_debug("%s->Error\n",__FUNCTION__);
	return retval;
}

/*-----------------------------------------------------------------------------
   func descp..

*/
void cam_disconnect(struct usb_interface *interface) {
    struct usb_cam *dev;
	int minor = interface->minor;

    print_debug("%s \n\r",__FUNCTION__);

	// prevent cam_open() from racing cam_disconnect()
	spin_lock(&cam_lock);

	dev = usb_get_intfdata(interface);
	usb_set_intfdata(interface, NULL);

	usb_deregister_dev(interface, &cam_class);

	spin_unlock(&cam_lock);

	kref_put(&dev->kref, cam_delete);

	print_debug("%s #%d now disconnected", DRIVER_NAME, minor);
}

/*-----------------------------------------------------------------------------
   func descp..

*/
void cam_delete(struct kref *kref) {
	struct usb_cam *dev = to_cam_dev(kref);

	print_debug("%s->Start\n",__FUNCTION__);

	usb_put_dev(dev->udev);
	kfree (dev->bulk_in_buffer);
	kfree (dev);

	print_debug("%s->Done\n",__FUNCTION__);
}

/*-----------------------------------------------------------------------------
   func descp..

*/
static int ele784_open(struct inode *inode, struct file *file)
{
    struct usb_interface *intf;
    int subminor;

    print_debug("%s->Start\n",__FUNCTION__);

    subminor = iminor(inode);

    intf = usb_find_interface(&cam_driver, subminor);
    if (!intf) {
    	print_alert("Unable to open device");
        return -ENODEV;
    }

    file->private_data = intf;
    print_debug("%s->Done\n",__FUNCTION__);

    return SUCCESS;
}

/*-----------------------------------------------------------------------------
   func descp..

*/
int cam_release(struct inode *inode, struct file *file) {
	struct usb_cam *dev;

	print_debug("%s->Start\n",__FUNCTION__);

	dev = (struct usb_cam *)file->private_data;
	if (dev == NULL) { return -ENODEV; }

	kref_put(&dev->kref, cam_delete);

	print_debug("%s->Done\n",__FUNCTION__);

	return SUCCESS;
}

/*-----------------------------------------------------------------------------
   func descp..

*/
long  cam_ioctl  (struct file *file, unsigned int cmd, unsigned long arg) {
    
    struct usb_interface *interface = file->private_data;
    struct usb_device *dev = usb_get_dev(interface_to_usbdev(interface));
    struct usb_host_interface   *iface_desc = interface->cur_altsetting;
    unsigned char buff[] = {0x00, 0x00, 0x80, 0xFF};
    
    int retval = 0;
    int tmp    = 0;
    char sendval;


    //IOC verification (MAGIC_nb/Valid_cmd/UserPtr)
    if (_IOC_TYPE(cmd)!= BUFF_IOC_MAGIC) { return -ENOTTY; }
    if (_IOC_NR  (cmd) > BUFF_IOC_MAX)   { return -ENOTTY; }
    if (_IOC_DIR (cmd) & _IOC_READ) {
        retval = !access_ok(VERIFY_WRITE, (void __user *)arg, _IOC_SIZE(cmd));
    }
    else if (_IOC_DIR(cmd) & _IOC_WRITE) {
        retval = !access_ok(VERIFY_READ,  (void __user *)arg, _IOC_SIZE(cmd));
    }
    if (retval) { return -EFAULT; }
    //IOC commands
    switch (cmd) {
        case IOCTL_GET:
            print_debug("GET");
            /*
            usb_device          Votre device USB
            pipe                Endpoint #0 de type rcv
            request             SET_CUR(0x81)/SET_MIN(0x82)/SET_MAX(0x83)/
                                SET_RES(0x84)
            requestType         USB_DIR_OUT | USB_TYPE_CLASS |
                                USB_RECIP_INTERFACE
            value               Processing Unit Control Selectors << 8
            index               0x0200
            data                2 bytes
            size                2
            timeout             0
            */

            /*
            down(&BDev.SemBuf);
            retval = buf_data(&Buffer);
            retval = put_user(retval,(int __user*) arg);
            */
            return retval;

        case IOCTL_SET:
            print_debug("SET");
            /*
            usb_device          Votre device USB
            pipe                Endpoint #0 de type SND
            request             GET_CUR(0x81)/GET_MIN(0x82)/GET_MAX(0x83)/
                                GET_RES(0x84)
            requestType         USB_DIR_IN | USB_TYPE_CLASS |
                                USB_RECIP_INTERFACE
            value               Processing Unit Control Selectors << 8
            index               0x0200
            data                NULL
            size                2 (receive)
            timeout             0
            */
            
            return SUCCESS;
            
        case IOCTL_STREAMON:
            print_debug("STREAMON");
            /*
            usb_device          Votre device USB
            pipe                Endpoint #0 de type SND
            request             0x0B
            requestType         USB_DIR_OUT | USB_TYPE_STANDARD |
                                USB_RECIP_INTERFACE
            value               0x0004
            index               0x0001
            data                Null
            size                0
            timeout             0
            */
            return SUCCESS;
            
        case IOCTL_STREAMOFF:
            print_debug("STREAMOFF");
            /*
            usb_device          Votre device USB
            pipe                Endpoint #0 de type SND
            request             0x0B
            requestType         USB_DIR_OUT | USB_TYPE_STANDARD |
                                USB_RECIP_INTERFACE
            value               0x0000
            index               0x0001
            data                Null
            size                0
            timeout             0
            */
            return SUCCESS;
            
        case IOCTL_GRAB:
            print_debug("GRAB");
            return SUCCESS;
            
        case IOCTL_PANTILT:
            print_debug("PANTILT");
            //TODO get arg from user --------------------------------------------------
            retval = usb_control_msg(dev, usb_sndctrlpipe(dev,0), 0x01,
                            USB_DIR_OUT | USB_TYPE_CLASS | USB_RECIP_INTERFACE,
                            0x0100, 0x0900, buff, 4*sizeof(char), 0);
            if (retval != 4) {
                if (retval >= 0) {
                    print_alert("not all bytes transfered");
                    return -EIO;
                }
                print_alert("usb_control_msg error");
                return retval;
            }
            return SUCCESS;

        case  IOCTL_PANTILT_RESET:
            // if 0x61  -> IOCTL_PANTILT_RESET
            sendval = 0x3;
            retval = usb_control_msg(dev, usb_sndctrlpipe(dev,0), 0x01,
                            USB_DIR_OUT | USB_TYPE_CLASS | USB_RECIP_INTERFACE,
                            0x0200, 0x0900, &sendval, sizeof(char), 0);
           if(retval != 1){
                if(retval >= 0){
                    print_alert("not all bytes transfered");
                    return -EIO;
                }
                print_alert("usb_control_msg error");
                return retval;
            }

            return SUCCESS;
            
        default : return -ENOTTY;  
    }
    return -ENOTTY;
}

/*-----------------------------------------------------------------------------
   func descp..

*/
ssize_t cam_read(struct file *file, char __user *buffer, size_t count,
                        loff_t *ppos) {
	struct usb_cam *dev;
	int retval = 0;

	print_debug("%s \n\r",__FUNCTION__);

	dev = (struct usb_cam *)file->private_data;

	// do a blocking bulk read to get data from the device
	retval = usb_bulk_msg(dev->udev,
			      usb_rcvbulkpipe(dev->udev, dev->bulk_in_endpointAddr),
			      dev->bulk_in_buffer,
			      min(dev->bulk_in_size, count),
			      (int *) &count, 10); //TODO  int timeout was equal to HZ*10

	// if the read was successful, copy the data to userspace
	if (!retval) {
		if (copy_to_user(buffer, dev->bulk_in_buffer, count))
			retval = -EFAULT;
		else
			retval = count;
	}

	return retval;
}


/*-----------------------------------------------------------------------------
   func descp..

*/
void cam_read_bulk_callback(struct urb *urb, struct pt_regs *regs) {
    print_debug("%s \n\r",__FUNCTION__);
    print_debug("cam_read_bulk_callback is not implemented yet\n");
	// sync/async unlink faults aren't errors

	//if ( urb->status &&
	//   !(urb->status == -ENOENT ||
	//     urb->status == -ECONNRESET ||
	//     urb->status == -ESHUTDOWN)) {
	//    dbg("%s - nonzero write bulk status received: %d",
	//	    __FUNCTION__, urb->status);
	//}

	//// free up our allocated buffer
	//usb_buffer_free(urb->dev, urb->transfer_buffer_length,
	// had to comment all instances of usb_buffer_alloc and usb_buffer_free
	//		urb->transfer_buffer, urb->transfer_dma);

}


/*-----------------------------------------------------------------------------
   func descp..

*/
void cam_grab (void){
// utility for IOCTL
// grab 5 urb, isochronous
}

// EOF ------------------------------------------------------------------------

