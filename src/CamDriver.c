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
	__u8   open_count;
	__u8 * bulk_in_buffer;        // the buffer to receive data
	size_t bulk_in_size;		  // the size of the receive buffer
	__u8   bulk_in_endpointAddr;  // the address of the bulk in endpoint
	__u8   bulk_out_endpointAddr; // the address of the bulk out endpoint
	__u8   ID3_endpointAddr;      // reserved variable (must rename later)
	__u8   ID4_endpointAddr;      // reserved variable (must rename later)
	__u8   ID5_endpointAddr;      // reserved variable (must rename later)
	struct kref  kref; // Shared structure reference counter

	// TBD
	struct mutex     	io_mutex;         	// synchronize I/O with disconnect
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

// Debug
void print_ioctl(void);
void print_usb_intf_info(const struct usb_host_interface *interface);



// Global Variable [GVar] -----------------------------------------------------
struct file_operations cam_fops = {
	.owner   = THIS_MODULE,
	.read    = cam_read,
	.open    = ele784_open,
	.release = cam_release,
	.unlocked_ioctl = cam_ioctl,
};

struct usb_driver cam_driver = {
	.name       = DRIVER_NAME,
	.id_table   = cam_table,
	.probe      = cam_probe,
	.disconnect = cam_disconnect,
	.id_table   = cam_table,
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

// Function definition [Fdef] -------------------------------------------------

module_init (cam_init);
module_exit (cam_exit);

MODULE_DEVICE_TABLE (usb, cam_table); // Hotplug detect

/*-----------------------------------------------------------------------------
   func descp..


   note:
*/
int __init cam_init(void) {
	int result;

	print_debug("%s->Start\n",__FUNCTION__);
    result = usb_register(&cam_driver);
	if (result) {
	    print_alert("failed with error %d\n",result);
	    return result;
	}

	print_ioctl();

	print_debug("%s->Done\n", __FUNCTION__);
	return result;
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
int cam_probe(struct usb_interface *intf,
        const struct usb_device_id *id) {
    struct usb_cam *dev;
    const struct usb_host_interface *interface;
    struct usb_host_endpoint *endpoint;
	int result, buffer_size;

	print_debug("%s->Start\n",__FUNCTION__);

    interface = &intf->altsetting[0]; // Setting 0 should aways exist

    if (interface->desc.bInterfaceClass    == USB_CLASS_VIDEO &&
       	interface->desc.bInterfaceSubClass == SC_VIDEOSTREAMING) {

    	dev = kmalloc(sizeof(struct usb_cam), GFP_KERNEL);
    	if (dev == NULL) {
    		print_alert("Out of memory");
    		return -ENOMEM;
    	}

    	kref_init (&dev->kref);
    	mutex_init(&dev->io_mutex);
    	//init_completion	(&dev->bulk_in_completion);

    	dev->udev = usb_get_dev(interface_to_usbdev(intf));
        usb_set_intfdata(intf, dev->udev);

	    result = usb_register_dev(intf, &cam_class);
	    if (result) {
		    print_alert("Not able to get a minor for this device\n");
		    usb_set_intfdata(intf, NULL);
		    goto ERROR;
	    }

	    usb_set_interface(dev->udev, TARGET_INTERFACE, TARGET_CONFIG);

	    print_usb_intf_info(intf->cur_altsetting);

	    endpoint = &intf->cur_altsetting->endpoint[0];

		buffer_size = usb_endpoint_maxp(&endpoint->desc);
		dev->bulk_in_size = buffer_size;
		dev->bulk_in_endpointAddr = (endpoint->desc.bEndpointAddress);
		dev->bulk_in_buffer = kmalloc(buffer_size, GFP_KERNEL);
		if (dev->bulk_in_buffer == NULL) {
			print_alert("Could not allocate in_buffer\n");
			result = -ENOMEM;
			goto ERROR;
		}

    } else {
        print_alert("can not find proper descriptors\n");
        return -EBADR;
    }

	//print_debug("USB attached to %s-%d\n", DRIVER_NAME, intf->minor);
	return SUCCESS;

    ERROR:
    print_alert("%s->Error!!\n",__FUNCTION__);
	if (dev) {
		kref_put(&dev->kref, cam_delete);
		kfree(dev);
	}
	print_debug("%s->Error\n",__FUNCTION__);
	return result;
}

/*-----------------------------------------------------------------------------
   func descp..

*/
void cam_disconnect(struct usb_interface *interface) {
    struct usb_cam *dev;
	int minor = interface->minor;

    print_debug("%s\n",__FUNCTION__);

	// prevent cam_open() from racing cam_disconnect()
	mutex_lock(&dev->io_mutex);

	dev = usb_get_intfdata(interface);
	usb_set_intfdata(interface, NULL);

	usb_deregister_dev(interface, &cam_class);

	mutex_unlock(&dev->io_mutex); //error here?

	kref_put(&dev->kref, cam_delete); //error here?

	print_debug("%s #%d now disconnected\n", DRIVER_NAME, minor);
}

/*-----------------------------------------------------------------------------
   func descp..

*/
void cam_delete(struct kref *kref) {
	struct usb_cam *dev = to_cam_dev(kref);

	print_debug("%s->Start\n",__FUNCTION__);
	//usb_free_urb(dev->bulk_in_urb);
	usb_put_dev(dev->udev);
	/*
	if (dev->bulk_in_buffer != NULL) {
		kfree (dev->bulk_in_buffer);
	}
	*/
	kfree (dev);

	print_debug("%s->Done\n",__FUNCTION__);
}

/*-----------------------------------------------------------------------------
   func descp..

*/
static int ele784_open(struct inode *inode, struct file *file) {
	struct usb_cam *dev;
    struct usb_interface *interface;
    int minor;

    print_debug("%s->Start\n",__FUNCTION__);

    minor = iminor(inode);

    interface = usb_find_interface(&cam_driver, minor);
    if (!interface) {
    	print_alert("Unable to open device\n");
        return -ENODEV;
    }
    dev = usb_get_intfdata(interface);
	if (!dev) {
		print_alert("Unable to open device\n");
		return -ENODEV;
	}

	mutex_lock(&dev->io_mutex);
	dev->open_count++;
	file->private_data = dev;
	mutex_unlock(&dev->io_mutex);

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
	if (dev == NULL) {
		return -ENODEV;
	}

	mutex_lock(&dev->io_mutex);
	dev->open_count--;
	mutex_unlock(&dev->io_mutex);

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
    
    int result = 0;
    int tmp    = 0;
    char sendval;

    (void) iface_desc;
    (void) tmp;

    //IOC verification (MAGIC_nb/Valid_cmd/UserPtr)
    if (_IOC_TYPE(cmd)!= BUFF_IOC_MAGIC) { return -ENOTTY; }
    if (_IOC_NR  (cmd) > BUFF_IOC_MAX)   { return -ENOTTY; }
    if (_IOC_DIR (cmd) & _IOC_READ) {
        result = !access_ok(VERIFY_WRITE, (void __user *)arg, _IOC_SIZE(cmd));
    }
    else if (_IOC_DIR(cmd) & _IOC_WRITE) {
        result = !access_ok(VERIFY_READ,  (void __user *)arg, _IOC_SIZE(cmd));
    }
    if (result) { return -EFAULT; }
    //IOC commands
    switch (cmd) {
        case IOCTL_GET:
            print_debug("GET\n");
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
            return result;

        case IOCTL_SET:
            print_debug("SET\n");
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
            print_debug("STREAMON\n");
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
            print_debug("STREAMOFF\n");
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
            print_debug("GRAB\n");
            return SUCCESS;
            
        case IOCTL_PANTILT:
            print_debug("PANTILT\n");
            //TODO get arg from user --------------------------------------------------
            result = usb_control_msg(dev, usb_sndctrlpipe(dev,0), 0x01,
                            USB_DIR_OUT | USB_TYPE_CLASS | USB_RECIP_INTERFACE,
                            0x0100, 0x0900, buff, 4*sizeof(char), 0);
            if (result != 4) {
                if (result >= 0) {
                    print_alert("not all bytes transfered\n");
                    return -EIO;
                }
                print_alert("usb_control_msg error\n");
                return result;
            }
            return SUCCESS;

        case  IOCTL_PANTILT_RESET:
            // if 0x61  -> IOCTL_PANTILT_RESET
            sendval = 0x3;
            result = usb_control_msg(dev, usb_sndctrlpipe(dev,0), 0x01,
                            USB_DIR_OUT | USB_TYPE_CLASS | USB_RECIP_INTERFACE,
                            0x0200, 0x0900, &sendval, sizeof(char), 0);
           if(result != 1){
                if(result >= 0){
                    print_alert("not all bytes transfered\n");
                    return -EIO;
                }
                print_alert("usb_control_msg error\n");
                return result;
            }

            return SUCCESS;
            
        default :
        	return -ENOTTY;
    }
    return -ENOTTY;
}

/*-----------------------------------------------------------------------------
   func descp..

*/
ssize_t cam_read(struct file *file, char __user *buffer, size_t count,
                        loff_t *ppos) {
	struct usb_cam *dev;
	int result = 0;

	print_debug("%s \n\r",__FUNCTION__);

	dev = (struct usb_cam *)file->private_data;

	// do a blocking bulk read to get data from the device
	result = usb_bulk_msg(dev->udev,
			      usb_rcvbulkpipe(dev->udev, dev->bulk_in_endpointAddr),
			      dev->bulk_in_buffer,
			      min(dev->bulk_in_size, count),
			      (int *) &count, 10); //TODO  int timeout was equal to HZ*10

	// if the read was successful, copy the data to userspace
	if (!result) {
		if (copy_to_user(buffer, dev->bulk_in_buffer, count))
			result = -EFAULT;
		else
			result = count;
	}

	return result;
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

/*-----------------------------------------------------------------------------
   func descp..

*/
void print_ioctl(void) {
    // Debug print for user space application
    print_warn("ioctl functions\n");
    print_warn("IOCTL_GET           : 0x%08x\n", IOCTL_GET);
    print_warn("IOCTL_SET           : 0x%08x\n", IOCTL_SET);
    print_warn("IOCTL_STREAMON      : 0x%08x\n", IOCTL_STREAMON);
    print_warn("IOCTL_STREAMOFF     : 0x%08x\n", IOCTL_STREAMOFF);
    print_warn("IOCTL_GRAB          : 0x%08x\n", IOCTL_GRAB);
    print_warn("IOCTL_PANTILT       : 0x%08x\n", IOCTL_PANTILT);
    print_warn("IOCTL_PANTILT_RESET : 0x%08x\n", IOCTL_PANTILT_RESET);
}

/*-----------------------------------------------------------------------------
   func descp..

*/
void print_usb_intf_info(const struct usb_host_interface *interface) {
	int i, tmp;
	struct usb_interface_descriptor desc = interface->desc;
	struct usb_host_endpoint *EndPoint;

	print_warn("   -------------");
	print_warn("bDescriptorType   : %u\n", desc.bDescriptorType);
	print_warn("bInterfaceClass   : %u\n", desc.bInterfaceClass);
	print_warn("bInterfaceSubClass: %u\n", desc.bInterfaceSubClass);
	print_warn("bInterfaceProtocol: %u\n", desc.bInterfaceProtocol);
	print_warn("bNumEndpoints     : %u\n", desc.bNumEndpoints);
	print_warn("bLength           : %u\n", desc.bLength);
	for (i=0; i < desc.bNumEndpoints; i++) {
		EndPoint = &interface->endpoint[i];
		print_warn("   -------------");
		print_warn("Endpointer[%d] : %p\n", i, EndPoint);
		print_warn("is enabled?    : %u\n", EndPoint->enabled);

		tmp = usb_endpoint_dir_in(&EndPoint->desc);
		if (tmp) {
			print_warn("direction is IN\n");
		} else {
			print_warn("direction is OUT\n");
		}

		tmp = (EndPoint->desc.bmAttributes & USB_ENDPOINT_XFERTYPE_MASK);
		if (tmp == USB_ENDPOINT_XFER_CONTROL) {
			print_warn("type is Control\n");
		} else if (tmp == USB_ENDPOINT_XFER_ISOC) {
			print_warn("type is Isochronous\n");
		} else if (tmp == USB_ENDPOINT_XFER_BULK) {
			print_warn("type is Bulk\n");
		} else if (tmp == USB_ENDPOINT_XFER_INT) {
			print_warn("type is Interrup\n");
		}

		print_warn("bLength        : %u\n", EndPoint->desc.bLength);
		print_warn("wMaxPacketSize : %u\n", EndPoint->desc.wMaxPacketSize);

	}
	print_warn("   -------------");
}
// EOF ------------------------------------------------------------------------
