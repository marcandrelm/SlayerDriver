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
 Project Name  : Char driver
 Target Devices: Orbitaf AF
 Tool versions : kernel 3.2.34etsele
 Description   : camera usb driver
 
 Revision:
 Revision 1.00 - Code is taped out for evaluation
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
//#include <linux/smp_lock.h> //does not exist //replace with semaphore
#include <linux/spinlock.h>
#include <linux/semaphore.h>
#include <linux/usb.h>
#include <asm/uaccess.h>

// Licence [Lic] --------------------------------------------------------------
MODULE_AUTHOR  ("Marc-Andre Lafaille Magnan");
MODULE_LICENSE ("Dual BSD/GPL");
MODULE_VERSION("1.0");
MODULE_DESCRIPTION("ELE784: laboratory 2\nCamera USB driver");

// Define [Def] ---------------------------------------------------------------

#define USB_CAM_VENDOR_ID0  (0x046D)
#define USB_CAM_PRODUCT_ID0	(0x08CC)
#define USB_CAM_VENDOR_ID1  (0x046D)
#define USB_CAM_PRODUCT_ID1	(0x0994)

#define USB_CAM_MINOR_BASE  (192)

#define to_cam_dev(d) container_of(d, struct usb_cam, kref)

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
	struct kref  kref; //TODO verify what is kref
};


// Enumeration [Enum] ---------------------------------------------------------

// Function [Func] ------------------------------------------------------------
int  cam_open    (struct inode *inode, struct file *file);
int  cam_release (struct inode *inode, struct file *file);

int  cam_probe     (struct usb_interface *interface, 
              const struct usb_device_id *id);
void cam_disconnect(struct usb_interface *interface)                     
                     
void cam_delete  (struct kref *kref); //TODO what is this?

ssize_t cam_read (struct file *file,       char __user *buffer, 
                    size_t count, loff_t *ppos);
ssize_t cam_write(struct file *file, const char __user *user_buffer, 
                    size_t count, loff_t *ppos);
                        
void cam_read_bulk_callback (struct urb *urb, struct pt_regs *regs);
void cam_write_bulk_callback(struct urb *urb, struct pt_regs *regs);

// Global Variable [GVar] -----------------------------------------------------
struct file_operations cam_fops = {
	.owner =	THIS_MODULE,
	.read =		cam_read,
	.write =	cam_write,
	.open =		cam_open,
	.release =	cam_release,
};

struct usb_driver cam_driver = {
	.owner      = THIS_MODULE,
	.name       = "camera",
	.id_table   = cam_table,
	.probe      = cam_probe,
	.disconnect = cam_disconnect,
};

/* 
  usb class driver info in order to get a minor number from the usb core,
  and to have the device registered with devfs and the driver core
*/
struct usb_class_driver cam_class = {
	.name = "usb/cam%d",
	.fops = &cam_fops,
	.mode = S_IFCHR | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH,
	.minor_base = USB_CAM_MINOR_BASE,
};


// Function definition [Fdef] -------------------------------------------------

MODULE_DEVICE_TABLE (usb, cam_table); // Hotplug detect

//-----------------------------------------------------------------------------
static int cam_open(struct inode *inode, struct file *file) {
	struct usb_cam *dev;
	struct usb_interface *interface;
	int subminor;
	int retval = 0;

	subminor = iminor(inode);

	interface = usb_find_interface(&cam_driver, subminor);
	if (!interface) {
		err ("%s - error, can't find device for minor %d",
		     __FUNCTION__, subminor);
		retval = -ENODEV;
		goto exit;
	}

	dev = usb_get_intfdata(interface);
	if (!dev) {
		retval = -ENODEV;
		goto exit;
	}
	
	// increment our usage count for the device   
	kref_get(&dev->kref);
 
	file->private_data = dev;

    exit: return retval;
}
//-----------------------------------------------------------------------------
static int cam_release(struct inode *inode, struct file *file) {
	struct usb_cam *dev;

	dev = (struct usb_cam *)file->private_data;
	if (dev == NULL)
		return -ENODEV;

	// decrement the count on our device   
	kref_put(&dev->kref, cam_delete);
	return 0;
}
//-----------------------------------------------------------------------------
static int cam_probe(struct usb_interface *interface, 
               const struct usb_device_id *id) {
	struct usb_cam                  *dev = NULL;
	struct usb_host_interface       *iface_desc;
	struct usb_endpoint_descriptor  *endpoint;
	size_t buffer_size;
	int i;
	int retval = -ENOMEM;

	// allocate memory for our device state and initialize it
	dev = kmalloc(sizeof(struct usb_cam), GFP_KERNEL);
	if (dev == NULL) {
		err("Out of memory");
		goto error;
	}
	memset(dev, 0x00, sizeof (*dev));
	kref_init(&dev->kref);

	dev->udev = usb_get_dev(interface_to_usbdev(interface));
	dev->interface = interface;

	// set up the endpoint information   
	// use only the first bulk-in and bulk-out endpoints   
	iface_desc = interface->cur_altsetting;
	for (i = 0; i < iface_desc->desc.bNumEndpoints; ++i) {
		endpoint = &iface_desc->endpoint[i].desc;

		if (!dev->bulk_in_endpointAddr &&
		    (endpoint->bEndpointAddress & USB_DIR_IN) &&
		    ((endpoint->bmAttributes & USB_ENDPOINT_XFERTYPE_MASK)
					== USB_ENDPOINT_XFER_BULK)) {
			// we found a bulk in endpoint   
			buffer_size = endpoint->wMaxPacketSize;
			dev->bulk_in_size = buffer_size;
			dev->bulk_in_endpointAddr = endpoint->bEndpointAddress;
			dev->bulk_in_buffer = kmalloc(buffer_size, GFP_KERNEL);
			if (!dev->bulk_in_buffer) {
				err("Could not allocate bulk_in_buffer");
				goto error;
			}
		}

		if (!dev->bulk_out_endpointAddr &&
		    !(endpoint->bEndpointAddress & USB_DIR_IN) &&
		    ((endpoint->bmAttributes & USB_ENDPOINT_XFERTYPE_MASK)
					== USB_ENDPOINT_XFER_BULK)) {
			// we found a bulk out endpoint   
			dev->bulk_out_endpointAddr = endpoint->bEndpointAddress;
		}
	}
	if (!(dev->bulk_in_endpointAddr && dev->bulk_out_endpointAddr)) {
		err("Could not find both bulk-in and bulk-out endpoints");
		goto error;
	}

	// save our data pointer in this interface device   
	usb_set_intfdata(interface, dev);

	// we can register the device now, as it is ready   
	retval = usb_register_dev(interface, &cam_class);
	if (retval) {
		// something prevented us from registering this driver   
		err("Not able to get a minor for this device.");
		usb_set_intfdata(interface, NULL);
		goto error;
	}

	// let the user know what node this device is now attached   
	info("USB cameton device now attached to USBcam-%d", interface->minor);
	return 0;

    error:
	if (dev)
		kref_put(&dev->kref, cam_delete);
	return retval;
}
//-----------------------------------------------------------------------------
static void cam_disconnect(struct usb_interface *interface) {
	struct usb_cam *dev;
	int minor = interface->minor;

	// prevent cam_open() from racing cam_disconnect() 
	lock_kernel();

	dev = usb_get_intfdata(interface);
	usb_set_intfdata(interface, NULL);

	// give back our minor 
	usb_deregister_dev(interface, &cam_class);

	unlock_kernel();
 
	// decrement our usage count   
	kref_put(&dev->kref, cam_delete);

	info("USB cameton #%d now disconnected", minor);
}
//-----------------------------------------------------------------------------
static void cam_delete(struct kref *kref) {	
	struct usb_cam *dev = to_cam_dev(kref);

	usb_put_dev(dev->udev);
	kfree (dev->bulk_in_buffer);
	kfree (dev);
}

//-----------------------------------------------------------------------------
static ssize_t cam_read(struct file *file, char __user *buffer, size_t count, 
                        loff_t *ppos) {
	struct usb_cam *dev;
	int retval = 0;

	dev = (struct usb_cam *)file->private_data;
	
	// do a blocking bulk read to get data from the device   
	retval = usb_bulk_msg(dev->udev,
			      usb_rcvbulkpipe(dev->udev, dev->bulk_in_endpointAddr),
			      dev->bulk_in_buffer,
			      min(dev->bulk_in_size, count),
			      &count, HZ*10);

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
static ssize_t cam_write(struct file *file, const char __user *user_buffer, size_t count, loff_t *ppos)
{
	struct usb_cam *dev;
	int retval = 0;
	struct urb *urb = NULL;
	char *buf = NULL;

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

	buf = usb_buffer_alloc(dev->udev, count, GFP_KERNEL, &urb->transfer_dma);
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
		err("%s - failed submitting write urb, error %d", __FUNCTION__, retval);
		goto error;
	}

	// release our reference to this urb, the USB core will eventually 
	// free it entirely 
	usb_free_urb(urb);

    exit: return count;

    error:
	usb_buffer_free(dev->udev, count, buf, urb->transfer_dma);
	usb_free_urb(urb);
	kfree(buf);
	return retval;
}
//-----------------------------------------------------------------------------
static void cam_read_bulk_callback(struct urb *urb, struct pt_regs *regs) {
	dbg("cam_read_bulk_callback is not implemented"
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
	//		urb->transfer_buffer, urb->transfer_dma);
	
}
//-----------------------------------------------------------------------------
static void cam_write_bulk_callback(struct urb *urb, struct pt_regs *regs) {
	// sync/async unlink faults aren't errors   
	if ( urb->status && 
	   !(urb->status == -ENOENT || 
	     urb->status == -ECONNRESET ||
	     urb->status == -ESHUTDOWN)) {
	    dbg("%s - nonzero write bulk status received: %d",
		    __FUNCTION__, urb->status);
	}

	// free up our allocated buffer   
	usb_buffer_free(urb->dev, urb->transfer_buffer_length, 
			urb->transfer_buffer, urb->transfer_dma);
}



//-----------------------------------------------------------------------------
static int __init USB_CAM_init(void) {
	int result;

	// register this driver with the USB subsystem 
	result = usb_register(&cam_driver);
	if (result)
		err("usb_register failed. Error number %d", result);

	return result;
}
//-----------------------------------------------------------------------------
static void __exit USB_CAM_exit(void) {
	// deregister this driver with the USB subsystem
	usb_deregister(&cam_driver);
}

//-----------------------------------------------------------------------------
module_init (USB_CAM_init);
module_exit (USB_CAM_exit);

// EOF ------------------------------------------------------------------------
