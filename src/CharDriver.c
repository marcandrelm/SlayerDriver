//-----------------------------------------------------------------------------
//   ____ _                  ____       _
//  / ___| |__   __ _ _ __  |  _ \ _ __(_)_   _____ _ __
// | |   | '_ \ / _` | '__| | | | | '__| \ \ / / _ \ '__|
// | |___| | | | (_| | |_   | |_| | |  | |\ V /  __/ |
//  \____|_| |_|\__,_|_(_)  |____/|_|  |_| \_/ \___|_|
//  _____ _     _____ _____ ___  _  _
// | ____| |   | ____|___  ( _ )| || |
// |  _| | |   |  _|    / // _ \| || |_
// | |___| |___| |___  / /| (_) |__   _|
// |_____|_____|_____|/_/  \___/   |_|
//
// Engineer      : Marc-Andre Lafaille Magnan
//
// Create Date   : 8:58:00 29/09/2014
// Design Name   :
// Module Name   :
// Project Name  : Char driver
// Target Devices: x86/x64 code
// Tool versions : kernel 3.2.34etsele
// Description   : Memory buffer between user space and kernel space.
//
//
// Revision:
// Revision 1.00 - Code is taped out for evaluation
// Additional Comments:
// Revision 0.01 - File Created based on Bruno De Keplar example
// Additional Comments:
//
//-----------------------------------------------------------------------------


// Table of content -----------------------------------------------------------
//  Inclusion           [Incl]
//  Define              [Def]
//  Licence             [Lic]
//  Struct              [Strc]
//  Enumeration         [Enum]
//  Function            [Func]
//  Global Variable     [GVar]
//  Function definition [Fdef]

// Inclusion [Incl] -----------------------------------------------------------
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/kdev_t.h>
#include <linux/types.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/fcntl.h>
#include <linux/wait.h>
#include <linux/spinlock.h>
#include <linux/semaphore.h>

#include <asm/atomic.h>
#include <asm/uaccess.h>

// Define [Def] ---------------------------------------------------------------
#define DEBUG               (0)
#define SUCCESS             (0)
#define DRIVER_TAG          "ETS_Cdev:"
#define DRIVER_NAME         "Cdev"
#define DRIVER_CNAME        "etsele_cdev"
#define READWRITE_BUFSIZE   (16)
#define DEFAULT_BUFSIZE     (256)
#define DEFAULT_MAXOPEN     (4) 

//IO control defines
#define BUFF_IOC_MAGIC   (42)
#define BUFF_IOC_MAX     (8)
#define GETNUMDATA      _IOR (BUFF_IOC_MAGIC, 0, int)
#define GETNUMREADER    _IOR (BUFF_IOC_MAGIC, 1, int)
#define GETBUFSIZE      _IOR (BUFF_IOC_MAGIC, 2, int)
#define SETBUFSIZE      _IOW (BUFF_IOC_MAGIC, 3, int)
#define GETNUMUSERS     _IOR (BUFF_IOC_MAGIC, 4, int)
#define GETMAXOPEN      _IOR (BUFF_IOC_MAGIC, 5, int)
#define SETMAXOPEN      _IOW (BUFF_IOC_MAGIC, 6, int)
#define PRINT_IOCTL     _IO  (BUFF_IOC_MAGIC, 7)
#define PRINT_STATS     _IO  (BUFF_IOC_MAGIC, 8)

// Debug options
#define print_alert(...)  printk(KERN_ALERT DRIVER_TAG __VA_ARGS__); 
#define print_warn(...)   printk(KERN_WARNING DRIVER_TAG __VA_ARGS__);
#if (DEBUG > 0)
    #define print_debug(...)  printk(KERN_DEBUG DRIVER_TAG __VA_ARGS__);
    #define IS_CAPABLE  
#else
    #define print_debug(...)
    //Cannot be Admin in the labs computer 
    #define IS_CAPABLE  if(!capable (CAP_SYS_ADMIN)) { \
                            print_alert("Not Admin") \
                            return -EPERM; } 
#endif

 
// Licence [Lic] --------------------------------------------------------------
MODULE_AUTHOR  ("Charles-Alexandre Gagnon");
MODULE_AUTHOR  ("Marc-Andre Lafaille Magnan");
MODULE_LICENSE ("Dual BSD/GPL");
MODULE_VERSION("1.0");
MODULE_DESCRIPTION("ELE784: laboratory 1\nChar driver");

// Struct [Strc] --------------------------------------------------------------
struct BufStruct {
    unsigned int    InIdx;
    unsigned int    OutIdx;
    unsigned char   BufEmpty;
    unsigned char   BufFull;    
    unsigned int    BufSize;
    unsigned char   *Buffer;  
} Buffer;

struct Buf_Dev {
    struct semaphore    SemBuf;
    unsigned char       *ReadBuf;
    unsigned char       *WriteBuf;    
    unsigned char       numWriter;
    unsigned char       numReader;
    unsigned char       numOpen;  
    unsigned char       maxOpen;  
    dev_t               dev;
    struct cdev         cdev;
} BDev;


// Enumeration [Enum] ---------------------------------------------------------
typedef enum {
    SEQ_VAR0,   SEQ_VAR1,  SEQ_VAR2, SEQ_VAR3,
    SEQ_CHRREG, SEQ_CLASS, SEQ_DEV,  SEQ_CDEV
} Seq_Driv; // Init sequence

// Function [Func] ------------------------------------------------------------
int     buf_init(void);
void    buf_exit(void);
int     buf_open   (struct inode *inode, struct file *filp);
int     buf_release(struct inode *inode, struct file *filp);
ssize_t buf_read   (struct file *filp,       char __user *ubuf, size_t count, 
                    loff_t *f_ops);
ssize_t buf_write  (struct file *filp, const char __user *ubuf, size_t count, 
                    loff_t *f_ops);
long    buf_ioctl  (struct file *filp, unsigned int cmd, unsigned long arg);
int     buf_data(struct BufStruct *Buf);
int     buf_in  (struct BufStruct *Buf, unsigned char *Data);
int     buf_out (struct BufStruct *Buf, unsigned char *Data);

// Global Variable [GVar] -----------------------------------------------------

struct class *      Cdev_class; 
wait_queue_head_t   b_queue;        // Blocking queue
//FIXME look at b_queue 
struct file_operations Buf_fops = {
    .owner          = THIS_MODULE,
	.open           = buf_open,
	.release        = buf_release,
	.read           = buf_read,
	.write          = buf_write,
	.unlocked_ioctl = buf_ioctl,
};

// Function definition [Fdef] -------------------------------------------------

int __init buf_init(void) {
    int Err = 0;
    Seq_Driv Fail = SEQ_VAR0;
     
    print_warn("Start Initialization");
    
    // Sleep initialisation ----------------
    init_waitqueue_head (&b_queue);
        
    // Initialisation de Buffer ------------
    Buffer.InIdx    = 0;
    Buffer.OutIdx   = 0;
    Buffer.BufEmpty = 1;
    Buffer.BufFull  = 0;
    Buffer.BufSize  = DEFAULT_BUFSIZE;
    Buffer.Buffer   = kmalloc(DEFAULT_BUFSIZE*sizeof(unsigned char),GFP_KERNEL);
    if (Buffer.Buffer == NULL) {
        print_alert("Error Allocating Buffer.Buffer"); 
        Err  = -ENOMEM;
        Fail = SEQ_VAR1-1;
        goto CleanDriver;
    }

    // Initialisation de BDev --------------
    BDev.ReadBuf = kmalloc(READWRITE_BUFSIZE*sizeof(unsigned char),GFP_KERNEL); 
    if (BDev.ReadBuf == NULL) {
        print_alert("Error Allocating BDev.ReadBuf");  
        Err  = -ENOMEM;
        Fail = SEQ_VAR2-1;
        goto CleanDriver;
    }
    BDev.WriteBuf = kmalloc(READWRITE_BUFSIZE*sizeof(unsigned char),GFP_KERNEL);
    if (BDev.WriteBuf == NULL) { 
        print_alert("Error Allocating BDev.WriteBuf");
        Err  = -ENOMEM;
        Fail = SEQ_VAR3-1;
        goto CleanDriver;
    }    
    sema_init(&BDev.SemBuf,1);
    BDev.numWriter = 0;
    BDev.numReader = 0;    
    BDev.numOpen   = 0;
    BDev.maxOpen   = DEFAULT_MAXOPEN;
    
    // Driver ID aquisition and Kernel Registry
    Err = alloc_chrdev_region(&BDev.dev, 0, 1, DRIVER_NAME);
    if(Err < 0) { 
        print_alert("Can't get Major/Minor");
        Fail = SEQ_CHRREG-1;
        goto CleanDriver;
    }
    Cdev_class = class_create(THIS_MODULE, DRIVER_NAME);
    device_create(Cdev_class,NULL,BDev.dev,&BDev,DRIVER_CNAME);
    cdev_init(&BDev.cdev, &Buf_fops);
    BDev.cdev.owner = THIS_MODULE;
    BDev.cdev.ops = &Buf_fops;
    Err = cdev_add(&BDev.cdev,BDev.dev,1);
    if(Err < 0) { 
        print_alert("Cdev Add failed");
        Fail = SEQ_CDEV-1;
        goto CleanDriver;
    }
    
    // Debug print for user space application
    print_warn("ioctl functions");
    print_warn("GETNUMDATA  : 0x%08x", GETNUMDATA);
    print_warn("GETNUMREADER: 0x%08x", GETNUMREADER);
    print_warn("GETBUFSIZE  : 0x%08x", GETBUFSIZE);
    print_warn("SETBUFSIZE  : 0x%08x", SETBUFSIZE);
    print_warn("GETNUMUSERS : 0x%08x", GETNUMUSERS);
    print_warn("GETMAXOPEN  : 0x%08x", GETMAXOPEN); 
    print_warn("SETMAXOPEN  : 0x%08x", SETMAXOPEN);
    print_warn("PRINT_IOCTL : 0x%08x", PRINT_IOCTL);
    print_warn("PRINT_STATS : 0x%08x", PRINT_STATS);

    return SUCCESS;
    
    // Error manager
    CleanDriver : switch (Fail) {
    case SEQ_CDEV   : cdev_del(&BDev.cdev);
    case SEQ_DEV    : device_destroy(Cdev_class, BDev.dev);
    case SEQ_CLASS  : class_destroy (Cdev_class);
    case SEQ_CHRREG : unregister_chrdev_region (BDev.dev, 1);
    case SEQ_VAR3   : kfree(&BDev.WriteBuf);
    case SEQ_VAR2   : kfree(&BDev.ReadBuf);
    case SEQ_VAR1   : kfree(&Buffer.Buffer);
    case SEQ_VAR0   : 
        return -Err;
    default: return -ENXIO;
    }
}
//------------------------------------------------------------------------------
void __exit buf_exit(void) {

    // Unregister from kernel
    cdev_del(&BDev.cdev);
    device_destroy(Cdev_class, BDev.dev);
    class_destroy (Cdev_class);
    unregister_chrdev_region (BDev.dev, 1);
    
    // deallocate all
    kfree(Buffer.Buffer);
    kfree(BDev.ReadBuf);
    kfree(BDev.WriteBuf);
    
    // Bye bye
    print_warn("Exiting");
}
//------------------------------------------------------------------------------
int buf_open(struct inode *inode, struct file *filp) {
    int AccMode = filp->f_flags & O_ACCMODE;    
    print_warn("Open");
        
    down(&BDev.SemBuf);    
    if((BDev.numWriter && !(AccMode == O_RDONLY)) ||
        BDev.numOpen == BDev.maxOpen ) {
        up(&BDev.SemBuf);         
        return -ENOTTY;
    }
    if (AccMode == O_RDONLY) { BDev.numReader++; } 
    if (AccMode == O_WRONLY) { BDev.numWriter++; }
    if (AccMode == O_RDWR) { 
        BDev.numReader++;
        BDev.numWriter++; 
    }
    BDev.numOpen++;
    up(&BDev.SemBuf);    
    
	return SUCCESS;	
}
//------------------------------------------------------------------------------
int buf_release(struct inode *inode, struct file *filp) {
    print_warn("Release"); 
    
    down(&BDev.SemBuf);
    if ((filp->f_flags & O_ACCMODE) == O_RDONLY) { BDev.numReader--; } 
    if ((filp->f_flags & O_ACCMODE) == O_WRONLY) { BDev.numWriter--; }
    if ((filp->f_flags & O_ACCMODE) == O_RDWR) { 
        BDev.numReader--;
        BDev.numWriter--; 
    }
    BDev.numOpen--;
    up(&BDev.SemBuf); 
    
	return SUCCESS;
}
//------------------------------------------------------------------------------
ssize_t buf_read(struct file *filp, char __user *ubuf, size_t count, 
                loff_t *f_ops) {
    
    size_t lbuft;       // R/W local buffer tracker 
    size_t buft = 0;    // kernel    buffer tracker
    *f_ops = 0 ;        // not used
    
    print_warn("Read");
    
    down(&BDev.SemBuf);
    print_debug("Start Empty:%d Full:%d\n",Buffer.BufEmpty, Buffer.BufFull );
    
    // Macro transfer
    while(buft < count) {
        size_t local = 0;
                
        // Micro transfer size
        if ((count - buft) >  READWRITE_BUFSIZE) { 
            lbuft = READWRITE_BUFSIZE;
        } else {
            lbuft = count - buft;
        }
        
        // Is buffer Empty?
        while(Buffer.BufEmpty) {
            if ((filp->f_flags & O_NONBLOCK) && !buft) {
                up(&BDev.SemBuf);
                return -EAGAIN;
            } else if (((filp->f_flags & O_ACCMODE) == O_RDWR &&
                    !(filp->f_flags & O_NONBLOCK))) {
                // Cannot block while reading as ReadWrite that will cause a 
                // deadlock as no will be able to write to wake up the read 
                up(&BDev.SemBuf);
                return -EAGAIN;
            }
            up(&BDev.SemBuf);
            if(buft) {
                wake_up(&b_queue);
                return buft;
            }
            print_debug("Read sleep");            
            if(wait_event_killable(b_queue, !Buffer.BufEmpty)) { goto ENDR; }
            print_debug("Read awake");   
            down(&BDev.SemBuf);
        }
        
        // Fill local buffer
        while(!Buffer.BufEmpty && local<lbuft) {
            buf_out(&Buffer, &(BDev.ReadBuf[local]));
            printk(KERN_DEBUG DRIVER_TAG" %c", (BDev.ReadBuf[local]));
            local++;
        }
        
        // Transfer to User
        if(copy_to_user(&ubuf[buft], BDev.ReadBuf, local)) {
    	    up(&BDev.SemBuf);
    	    print_alert("-EFAULT on Read");  
    	    wake_up(&b_queue);
    	    return -EFAULT;
        }
        buft += local;
    }  
    up(&BDev.SemBuf);
    
    ENDR: 
    wake_up(&b_queue);
    print_warn("%zu bytes copied",buft);
    print_warn("Empty:%d Full:%d", Buffer.BufEmpty, Buffer.BufFull); 
    return buft;
}
//------------------------------------------------------------------------------
ssize_t buf_write(struct file *filp, const char __user *ubuf, size_t count, 
                    loff_t *f_ops) {
    /*
    Kernel checks R/W permissions autmatically. Documentation says not to check.
    "You might want to check this field for read/write permission in your 
    open or ioctl function, but you don't need to check permissions for 
    read and write, because the kernel checks before invoking your method. "
    http://www.makelinux.net/ldd3/chp-3-sect-3  
    */  

    size_t lbuft;       // R/W local buffer tracker
    size_t buft = 0;    // kernel    buffer tracker
    *f_ops = 0 ;        // not used
    
    print_warn("Write");  
     
    down(&BDev.SemBuf);
    print_warn("Start Empty:%d Full:%d", Buffer.BufEmpty, Buffer.BufFull);
    // Macro transfer  
    while(buft < count) {
        size_t local = 0;
        // Micro transfer size
        if ((count - buft) > READWRITE_BUFSIZE) {
            lbuft = READWRITE_BUFSIZE;
        }
        else {
            lbuft = count - buft;
        }
        // Is buffer Full?
        while(Buffer.BufFull) {
            if ((filp->f_flags & O_NONBLOCK) && !buft) {
                up(&BDev.SemBuf);
                return -EAGAIN;
            }
            up(&BDev.SemBuf);
            if(buft) {
                wake_up(&b_queue);
                return buft;
            }
            print_debug("Write sleep");
            if(wait_event_killable(b_queue, !Buffer.BufFull)) { goto ENDW; }
            print_debug("Write awake");
            down(&BDev.SemBuf);
        }
        
        // Transfer to Kernel
        if(copy_from_user(BDev.WriteBuf, &ubuf[buft], lbuft)) {
    	    up(&BDev.SemBuf);
    	    print_alert("-EFAULT on Write");
    	    wake_up(&b_queue);
    	    return -EFAULT;
        } 
        // Fill buffer
        while(!Buffer.BufFull && local<lbuft) {
            buf_in(&Buffer, &BDev.WriteBuf[local]);
            print_debug(" %c", BDev.WriteBuf[local]);            
            local++;
        }
        buft += local;
    }  
    up(&BDev.SemBuf);
    
    ENDW: 
    wake_up(&b_queue);
    print_warn("%zu bytes copied",buft);
    print_alert("End Empty:%d Full:%d", Buffer.BufEmpty,Buffer.BufFull);   
    return buft;
}
//------------------------------------------------------------------------------
long buf_ioctl(struct file *filp, unsigned int cmd, unsigned long arg) {
    int retval = 0;
    int tmp    = 0;

    //IOC verification (MAGIC_nb/Valid_cmd/UserPtr)
    if (_IOC_TYPE(cmd)!= BUFF_IOC_MAGIC) return -ENOTTY;
    if (_IOC_NR  (cmd) > BUFF_IOC_MAX)   return -ENOTTY;
    if (_IOC_DIR (cmd) & _IOC_READ)
        retval = !access_ok(VERIFY_WRITE, (void __user *)arg, _IOC_SIZE(cmd));
    else if (_IOC_DIR(cmd) & _IOC_WRITE)
        retval = !access_ok(VERIFY_READ,  (void __user *)arg, _IOC_SIZE(cmd));
    if (retval) return -EFAULT;
    
    //IOC commands
    switch (cmd) {
        case GETNUMDATA:
            down(&BDev.SemBuf);
            retval = buf_data(&Buffer);
            retval = put_user(retval,(int __user*) arg);
            up(&BDev.SemBuf);
            return retval;
            
        case GETNUMREADER: 
            down(&BDev.SemBuf);
            retval = BDev.numReader;
            retval = put_user(retval,(int __user*) arg);
            up(&BDev.SemBuf); 
            return retval;
            
        case GETBUFSIZE:    
            down(&BDev.SemBuf);
            retval = Buffer.BufSize;
            //print_debug("GETBUFSIZE:%d", retval);
            retval = put_user(retval,(int __user*) arg);
            //print_debug("Retval:%d", retval);
            up(&BDev.SemBuf); 
            return retval;
            
        case SETBUFSIZE:
            IS_CAPABLE
            if(!(filp->f_mode & FMODE_WRITE))       { return -ENOTTY; }
            if(get_user(retval,(int __user *) arg)) { return -EFAULT; }
            
            down(&BDev.SemBuf);
            print_alert("SETBUFSIZE");
            tmp = buf_data(&Buffer);
            print_alert("buf_data is %d",tmp);
            print_alert("buf will become %d",retval);          
            if((retval < tmp) || retval == 0) {
                up(&BDev.SemBuf);
                print_alert("Illegal req r:%d u:%d", retval, tmp);
                return -ENOBUFS;
            }
            print_alert("Going to realloc");
            tmp    = retval;
            retval = (int) krealloc(&(Buffer.Buffer), 
                            retval * sizeof(unsigned char), GFP_KERNEL);
            if((int*) retval == NULL) {
                up(&BDev.SemBuf);
                print_alert("Error reallocating Buffer.Buffer");
                return -ENOMEM;
            }
            print_alert("realloc success");
            print_alert("buf_data is %d",tmp);
            Buffer.BufSize = tmp;
            up(&BDev.SemBuf);
            return SUCCESS;
            
        case GETNUMUSERS: 
            down(&BDev.SemBuf);
            retval = BDev.numOpen;
            retval = put_user(retval,(int __user*) arg);
            up(&BDev.SemBuf); 
            return retval; 
            
        case GETMAXOPEN: 
            down(&BDev.SemBuf);
            retval = BDev.maxOpen;
            retval = put_user(retval,(int __user*) arg);
            up(&BDev.SemBuf); 
            return retval;
            
        case SETMAXOPEN:
            IS_CAPABLE
            if(!(filp->f_mode & FMODE_WRITE))       { return -ENOTTY; }
            if(get_user(retval,(int __user *) arg)) { return -EFAULT; }

            down(&BDev.SemBuf); 
            print_alert("SETMAXOPEN");
            if(retval < BDev.numOpen) {
                print_alert("Illegal req r:%d u:%d", retval, BDev.numOpen);
                up(&BDev.SemBuf); 
                return -ENOBUFS; 
            }
            BDev.maxOpen = retval;
            up(&BDev.SemBuf);             
            return SUCCESS;
            
        case PRINT_IOCTL:
            print_warn("IOCTL print");
            return SUCCESS;
 
        case PRINT_STATS:
            print_warn("Driver Status");
            print_warn("Buffer info -------------");
            print_warn("InIdx   :%d",Buffer.InIdx);
            print_warn("OutIdx  :%d",Buffer.OutIdx);
            print_warn("BufEmpty:%d",Buffer.BufEmpty);
            print_warn("BufFull :%d",Buffer.BufFull);
            print_warn("BufSize :%d",Buffer.BufSize);
            print_warn("");
            print_warn("BDev info -------------");
            print_warn("Writer  :%d",BDev.numWriter);
            print_warn("Reader  :%d",BDev.numReader);
            print_warn("Open    :%d",BDev.numOpen);
            print_warn("MaxOpen :%d",BDev.maxOpen);
            print_warn("filp info -------------");
            print_warn("mode    :%d",filp->f_mode);
            print_warn("pos     :%lld",filp->f_pos);
            print_warn("flags   :0x%08x",filp->f_flags);//<fcntl.h>
            // print_warn("BufSize :%0x%08x",filp->f_count);
            // can't find print format for atomic_long_t
            print_warn("version :%llu",filp->f_version);
            print_warn("");            
            return SUCCESS;         
            
        default : return -ENOTTY;  
    }
    return -ENOTTY;
}

//------------------------------------------------------------------------------
int buf_data(struct BufStruct *Buf) {

    print_debug("BufEmpty: %d", Buf->BufEmpty);
    if(Buf->BufEmpty) { return 0; }
    print_debug("BufFull : %d", Buf->BufFull);
    if(Buf->BufFull)  { return Buf->BufSize; } 
    print_debug("InIdx   : %d", Buf->InIdx);
    print_debug("OutIdx  : %d", Buf->OutIdx);     
    if(Buf->OutIdx < Buf->InIdx) {
        return Buf->InIdx - Buf->OutIdx;
    } else {
        return Buf->BufSize - (Buf->OutIdx - Buf->InIdx);
    }
}
//------------------------------------------------------------------------------
int buf_in(struct BufStruct *Buf, unsigned char *Data) {
    if (Buf->BufFull) { return -1; }
    Buf->BufEmpty = 0;
    Buf->Buffer[Buf->InIdx] = *Data;
    Buf->InIdx = (Buf->InIdx + 1) % Buf->BufSize;
    if (Buf->InIdx == Buf->OutIdx) {
        Buf->BufFull = 1;
    }
    return SUCCESS;
}
//------------------------------------------------------------------------------
int buf_out (struct BufStruct *Buf, unsigned char *Data) {
    if (Buf->BufEmpty) { return -1; }
    Buf->BufFull = 0;
    *Data = Buf->Buffer[Buf->OutIdx];
    Buf->OutIdx = (Buf->OutIdx + 1) % Buf->BufSize;
    if (Buf->OutIdx == Buf->InIdx) {
        Buf->BufEmpty = 1;
    }
    return SUCCESS;
}
//------------------------------------------------------------------------------
module_init(buf_init);
module_exit(buf_exit);
// EOF -------------------------------------------------------------------------
