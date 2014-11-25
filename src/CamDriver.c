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
// #include <linux/config.h> //does not exist
// #include "/usr/src/linux-headers-3.2.34etsele/include/linux/config.h" 
//does not exist

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/kref.h>
#include <linux/spinlock.h>
#include <linux/semaphore.h>
#include <linux/usb.h>
#include <asm/uaccess.h> 
#include <linux/sched/rt.h>
#include <linux/export.h>

// Licence [Lic] --------------------------------------------------------------
MODULE_AUTHOR  ("Marc-Andre Lafaille Magnan");
MODULE_LICENSE ("Dual BSD/GPL");
MODULE_VERSION ("1.0");
MODULE_DESCRIPTION("ELE784: laboratory 2\nCamera USB driver");

// Define [Def] ---------------------------------------------------------------

#define DEBUG               (1)
#define SUCCESS             (0)
#define DRIVER_TAG          "Cam_Udev:"
#define DRIVER_NAME         "UsbCam"
#define DRIVER_CNAME        "etsele_udev"
#define READWRITE_BUFSIZE   (16)
#define DEFAULT_BUFSIZE     (256)
#define DEFAULT_MAXOPEN     (4) 

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
#define IOCTL_PANTILT_RESET    _IOW (BUFF_IOC_MAGIC, 0x61, int)

//what is IOCTL, GRAB FOR ????----------------------------------------

#define to_cam_dev(d) container_of(d, struct usb_cam, kref)

// Debug options
	// Critical print (no sleep)
#define print_alert(...)  printk(KERN_ALERT DRIVER_TAG __VA_ARGS__); 
	// Warning print (may sleep)
#define print_warn(...)   printk(KERN_WARNING DRIVER_TAG __VA_ARGS__);
#if (DEBUG > 0)
	// Debug print (may sleep), are removed on project release
    #define print_debug(...)  printk(KERN_DEBUG DRIVER_TAG __VA_ARGS__);
	// Verify if system Admin
    #define IS_CAPABLE  
#else
    #define print_debug(...)
    //Cannot be Admin in the labs computer 
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
	struct kref  kref; //TODO verify what is kref
};

// Enumeration [Enum] ---------------------------------------------------------

// Function [Func] ------------------------------------------------------------


static int  cam_probe     (struct usb_interface *intf, 
              const struct usb_device_id *id);
void cam_disconnect(struct usb_interface *interface);                     
                     
void cam_delete  (struct kref *kref); //TODO what is this?

static int  ele784_open    (struct inode *inode, struct file *file);
int  cam_release (struct inode *inode, struct file *file);

long  cam_ioctl  (struct file *file, unsigned int cmd, unsigned long arg);

ssize_t cam_read (struct file *file,       char __user *buffer,
				size_t count, loff_t *ppos);
ssize_t cam_write(struct file *file, const char __user *user_buffer, 
                    size_t count, loff_t *ppos); // to be killed
                        
void cam_read_bulk_callback (struct urb *urb, struct pt_regs *regs);

void cam_write_bulk_callback(struct urb *urb, struct pt_regs *regs);
    // to be killed
void cam_grab (void); // Prototype TBD


// Global Variable [GVar] -----------------------------------------------------
struct file_operations cam_fops = {
	.owner =	THIS_MODULE,
	.read =		cam_read,
	.write =	cam_write,
	.open =		ele784_open,
	.release =	cam_release,
	.unlocked_ioctl = cam_ioctl,
};

struct usb_driver cam_driver = {
	//.owner          = THIS_MODULE, //dosen't seem needed? Gives compile error
	.name           = "ELE784-camdriver",
	.id_table       = cam_table,
	.probe          = cam_probe,
	.disconnect     = cam_disconnect,
};

/* 
  usb class driver info in order to get a minor number from the usb core,
  and to have the device registered with devfs and the driver core
*/
struct usb_class_driver cam_class = {
	.name = "elecam%d",
	.fops = &cam_fops,
	.minor_base = USB_CAM_MINOR_BASE,
};

static DEFINE_SPINLOCK(cam_lock);

// Function definition [Fdef] -------------------------------------------------

MODULE_DEVICE_TABLE (usb, cam_table); // Hotplug detect


//-----------------------------------------------------------------------------
static int cam_probe(struct usb_interface *intf, 
               const struct usb_device_id *id) {
	struct usb_cam                  *dev = NULL;
	const struct usb_host_interface *interface;
    int n, altSetNum;
	int retval = -ENOMEM;
	
	print_debug("%s \n",__FUNCTION__);

	// allocate memory for our device state and initialize it
	dev = kmalloc(sizeof(struct usb_cam), GFP_KERNEL);
	if (dev == NULL) {
		print_alert("Out of memory");
		goto error;
	}
	memset(dev, 0x00, sizeof (*dev));
	kref_init(&dev->kref);
	
	dev->udev = usb_get_dev(interface_to_usbdev(intf)); 

    interface = &intf->altsetting[n];
    altSetNum = interface->desc.bAlternateSetting;
    if(interface->desc.bInterfaceClass == USB_CLASS_VIDEO &&
                                    interface->desc.bInterfaceSubClass ==2){
       	// save our data pointer in this interface device   
        usb_set_intfdata(intf, dev->udev);
        
        // we can register the device now, as it is ready   
	    retval = usb_register_dev(intf, &cam_class);
	    if (retval) {
		    // something prevented us from registering this driver   
		    print_alert("Not able to get a minor for this device.");
		    usb_set_intfdata(intf, NULL);
		    goto error;
	    }
	    usb_set_interface (dev->udev, 1,4); //from doc, comes from reverse engineering
    }
    else{
        retval = -1; //TODO find something better
        goto error;
    }
	//TODO init_completion() and completion in callback;
	
	// let the user know what node this device is now attached   
	printk(KERN_WARNING "USB cam device now attached to USBcam-%d", intf->minor);
	return 0;

    error:
	if (dev) { kref_put(&dev->kref, cam_delete); }
	return retval;
}
//-----------------------------------------------------------------------------
void cam_disconnect(struct usb_interface *interface) {
    //TODO call usb_put_dev() somewhere here -------------------------------------------
	struct usb_cam *dev;
	int minor = interface->minor;

    print_debug("%s \n\r",__FUNCTION__);

	// prevent cam_open() from racing cam_disconnect() 
	spin_lock(&cam_lock); 

	dev = usb_get_intfdata(interface);
	usb_set_intfdata(interface, NULL);

	// give back our minor 
	usb_deregister_dev(interface, &cam_class);

	spin_unlock(&cam_lock);
 
	// decrement our usage count   
	kref_put(&dev->kref, cam_delete);

	printk(KERN_WARNING "USB cam #%d now disconnected", minor);
}
//-----------------------------------------------------------------------------
void cam_delete(struct kref *kref) {	
	struct usb_cam *dev = to_cam_dev(kref);
	
	print_debug("%s \n\r",__FUNCTION__);

	usb_put_dev(dev->udev);
	kfree (dev->bulk_in_buffer);
	kfree (dev);
}
//-----------------------------------------------------------------------------
static int ele784_open(struct inode *inode, struct file *file)
{
    struct usb_interface *intf;
    int subminor;
    
    printk(KERN_WARNING "ELE784 -> Open \n\r");
    
    subminor = iminor(inode);
    
    intf = usb_find_interface(&cam_driver, subminor);
    if (!intf) {
        printk(KERN_WARNING "ELE784 -> Open: Ne peux ouvrir le peripherique");
        return -ENODEV;
    }
    
    file->private_data = intf;
    return 0;
}
//-----------------------------------------------------------------------------
int cam_release(struct inode *inode, struct file *file) {
	struct usb_cam *dev;
	
	print_debug("%s \n\r",__FUNCTION__);

	dev = (struct usb_cam *)file->private_data;
	if (dev == NULL) { return -ENODEV; }

	// decrement the count on our device   
	kref_put(&dev->kref, cam_delete);
	return 0;
}
//-----------------------------------------------------------------------------
long  cam_ioctl  (struct file *file, unsigned int cmd, unsigned long arg) {
    
    struct usb_interface *interface = file->private_data;
    struct usb_device *dev = usb_get_intfdata(interface);
    struct usb_host_interface   *iface_desc = interface->cur_altsetting;
    unsigned char buff[] = {0x00, 0x00, 0x80, 0xFF};
    
    int retval = 0;
    int tmp    = 0;    
    
   
    
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
            //TODO get endpoint address------------------------------------------------
            //TODO usb_send_control_pipe
            retval = usb_control_msg(dev, 0, 0x01, 
                            USB_DIR_OUT | USB_TYPE_CLASS | USB_RECIP_INTERFACE,
                            0x0100, 0x0900, buff, 4, 0);
            if(retval != 4){
                if(retval >= 0){
                    print_alert("not all bytes transfered");
                    return -EIO;
                }
                print_alert("usb_control_msg error");
                return retval;
            }
            
            // if 0x60  -> IOCTL_PANTILT
            /*
            usb_device          Votre device USB
            pipe                Endpoint #0 de type SND
            request             0x01
            requestType         USB_DIR_OUT | USB_TYPE_CLASS |
                                USB_RECIP_INTERFACE
            value               0x0100
            index               0x0900
            data                up/down/left/right
            size                4
            timeout             0 
            
            unsigned int up[4]    = { 0x00, 0x00, 0x80, 0xFF };
            unsigned int down[4]  = { 0x00, 0x00, 0x80, 0x00 };
            unsigned int Left[4]  = { 0x80, 0x00, 0x00, 0x00 };
            unsigned int Right[4] = { 0x80, 0xFF, 0x00, 0x00 };
            */
            
            // if 0x61  -> IOCTL_PANTILT_RESET
            /*
             usb_device          Votre device USB
             pipe                Endpoint #0 de type SND
             request             0x01
             requestType         USB_DIR_OUT | USB_TYPE_CLASS |
                                 USB_RECIP_INTERFACE
             value               0x0200
             index               0x0900
             data                0x03
             size                1
             timeout             0
             */
           case  IOCTL_PANTILT_RESET:
           
            return SUCCESS;
            
        default : return -ENOTTY;  
    }
    return -ENOTTY;
}
//-----------------------------------------------------------------------------
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

//-----------------------------------------------------------------------------
ssize_t cam_write(struct file *file, const char __user *user_buffer, size_t count, loff_t *ppos)
{
	struct usb_cam *dev;
	int retval = 0;
	struct urb *urb = NULL;
	char *buf = NULL;
	
	print_debug("%s \n\r",__FUNCTION__);

	dev = (struct usb_cam *)file->private_data;

	// verify that we actually have some data to write   
	if (count == 0)
		goto exit;

	// create a urb, and a buffer for it, and copy the data to the urb   
	urb = usb_alloc_urb(0, GFP_KERNEL);
	if (!urb) {
		retval = -ENOMEM;
		goto error;
	}
    
	//buf = usb_buffer_alloc(dev->udev, count, GFP_KERNEL, &urb->transfer_dma);
	// had to comment all instances of usb_buffer_alloc and usb_buffer_free
	if (!buf) {
		retval = -ENOMEM;
		goto error;
	}
	if (copy_from_user(buf, user_buffer, count)) {
		retval = -EFAULT;
		goto error;
	}

	// initialize the urb properly   
	usb_fill_bulk_urb(urb, dev->udev,
			  usb_sndbulkpipe(dev->udev, dev->bulk_out_endpointAddr),
			  buf, count, cam_write_bulk_callback, dev);
	urb->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;

	// send the data out the bulk port   
	retval = usb_submit_urb(urb, GFP_KERNEL);
	if (retval) {
		print_alert("%s - failed submitting write urb, error %d", __FUNCTION__, retval);
		goto error;
	}

	// release our reference to this urb, the USB core will eventually 
	// free it entirely 
	usb_free_urb(urb);

    exit: return count;

    error:
	//usb_buffer_free(dev->udev, count, buf, urb->transfer_dma);
	// had to comment all instances of usb_buffer_alloc and usb_buffer_free
	usb_free_urb(urb);
	kfree(buf);
	return retval;
}
//-----------------------------------------------------------------------------
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
//-----------------------------------------------------------------------------
void cam_write_bulk_callback(struct urb *urb, struct pt_regs *regs) {
	// sync/async unlink faults aren't errors   
	
	print_debug("%s \n\r",__FUNCTION__);
	print_debug("cam_write_bulk_callback is to be removed\n"); 
	
	/*
	if ( urb->status && 
	   !(urb->status == -ENOENT || 
	     urb->status == -ECONNRESET ||
	     urb->status == -ESHUTDOWN)) {
	    dbg("%s - nonzero write bulk status received: %d",
		    __FUNCTION__, urb->status);
	}

	// free up our allocated buffer   
	//usb_buffer_free(urb->dev, urb->transfer_buffer_length, 
	// had to comment all instances of usb_buffer_alloc and usb_buffer_free
			urb->transfer_buffer, urb->transfer_dma);
	*/
}
//-----------------------------------------------------------------------------
void cam_grab (void){
// utility for IOCTL
// grab 5 urb, isochronous
}
//-----------------------------------------------------------------------------
int __init USB_CAM_init(void) {
	int result;
//	int KBUILD_MODNAME = 1; // Set Manualy MODNAME
    printk(KERN_WARNING "ELE784 -> Init \n\r");
	
	// register this driver with the USB subsystem 
	result = usb_register_driver(&cam_driver,THIS_MODULE, KBUILD_MODNAME);
	if (result) { 
	    printk(KERN_ALERT"%s : failed with error %d\n\r", __FUNCTION__,result);
	}
	
	return result;
}
//-----------------------------------------------------------------------------
void __exit USB_CAM_exit(void) {
	// deregister this driver with the USB subsystem
	printk(KERN_WARNING"ELE784-Cleanup");
	printk(KERN_WARNING"%s \n\r",__FUNCTION__);
	usb_deregister(&cam_driver);
}

//-----------------------------------------------------------------------------
//TODO forgot some include?

module_init (USB_CAM_init);
module_exit (USB_CAM_exit);

// EOF ------------------------------------------------------------------------
