 1 /*
  2  * USB Skeleton driver - 2.2
  3  *
  4  * Copyright (C) 2001-2004 Greg Kroah-Hartman (greg@kroah.com)
  5  *
  6  *      This program is free software; you can redistribute it and/or
  7  *      modify it under the terms of the GNU General Public License as
  8  *      published by the Free Software Foundation, version 2.
  9  *
 10  * This driver is based on the 2.6.3 version of drivers/usb/usb-skeleton.c
 11  * but has been rewritten to be easier to read and use.
 12  *
 13  */
 14 
 15 #include <linux/kernel.h>
 16 #include <linux/errno.h>
 17 #include <linux/slab.h>
 18 #include <linux/module.h>
 19 #include <linux/kref.h>
 20 #include <linux/uaccess.h>
 21 #include <linux/usb.h>
 22 #include <linux/mutex.h>
 23 
 24 
 25 /* Define these values to match your devices */
 26 #define USB_SKEL_VENDOR_ID      0xfff0
 27 #define USB_SKEL_PRODUCT_ID     0xfff0
 28 
 29 /* table of devices that work with this driver */
 30 static const struct usb_device_id skel_table[] = {
 31         { USB_DEVICE(USB_SKEL_VENDOR_ID, USB_SKEL_PRODUCT_ID) },
 32         { }                                     /* Terminating entry */
 33 };
 34 MODULE_DEVICE_TABLE(usb, skel_table);
 35 
 36 
 37 /* Get a minor range for your devices from the usb maintainer */
 38 #define USB_SKEL_MINOR_BASE     192
 39 
 40 /* our private defines. if this grows any larger, use your own .h file */
 41 #define MAX_TRANSFER            (PAGE_SIZE - 512)
 42 /* MAX_TRANSFER is chosen so that the VM is not stressed by
 43    allocations > PAGE_SIZE and the number of packets in a page
 44    is an integer 512 is the largest possible packet on EHCI */
 45 #define WRITES_IN_FLIGHT        8
 46 /* arbitrarily chosen */
 47 
 48 /* Structure to hold all of our device specific stuff */
 49 struct usb_skel {
 50         struct usb_device       *udev;                  /* the usb device for this device */
 51         struct usb_interface    *interface;             /* the interface for this device */
 52         struct semaphore        limit_sem;              /* limiting the number of writes in progress */
 53         struct usb_anchor       submitted;              /* in case we need to retract our submissions */
 54         struct urb              *bulk_in_urb;           /* the urb to read data with */
 55         unsigned char           *bulk_in_buffer;        /* the buffer to receive data */
 56         size_t                  bulk_in_size;           /* the size of the receive buffer */
 57         size_t                  bulk_in_filled;         /* number of bytes in the buffer */
 58         size_t                  bulk_in_copied;         /* already copied to user space */
 59         __u8                    bulk_in_endpointAddr;   /* the address of the bulk in endpoint */
 60         __u8                    bulk_out_endpointAddr;  /* the address of the bulk out endpoint */
 61         int                     errors;                 /* the last request tanked */
 62         bool                    ongoing_read;           /* a read is going on */
 63         spinlock_t              err_lock;               /* lock for errors */
 64         struct kref             kref;
 65         struct mutex            io_mutex;               /* synchronize I/O with disconnect */
 66         wait_queue_head_t       bulk_in_wait;           /* to wait for an ongoing read */
 67 };
 68 #define to_skel_dev(d) container_of(d, struct usb_skel, kref)
 69 
 70 static struct usb_driver skel_driver;
 71 static void skel_draw_down(struct usb_skel *dev);
 72 
 73 static void skel_delete(struct kref *kref)
 74 {
 75         struct usb_skel *dev = to_skel_dev(kref);
 76 
 77         usb_free_urb(dev->bulk_in_urb);
 78         usb_put_dev(dev->udev);
 79         kfree(dev->bulk_in_buffer);
 80         kfree(dev);
 81 }
 82 
 83 static int skel_open(struct inode *inode, struct file *file)
 84 {
 85         struct usb_skel *dev;
 86         struct usb_interface *interface;
 87         int subminor;
 88         int retval = 0;
 89 
 90         subminor = iminor(inode);
 91 
 92         interface = usb_find_interface(&skel_driver, subminor);
 93         if (!interface) {
 94                 pr_err("%s - error, can't find device for minor %d\n",
 95                         __func__, subminor);
 96                 retval = -ENODEV;
 97                 goto exit;
 98         }
 99 
100         dev = usb_get_intfdata(interface);
101         if (!dev) {
102                 retval = -ENODEV;
103                 goto exit;
104         }
105 
106         retval = usb_autopm_get_interface(interface);
107         if (retval)
108                 goto exit;
109 
110         /* increment our usage count for the device */
111         kref_get(&dev->kref);
112 
113         /* save our object in the file's private structure */
114         file->private_data = dev;
115 
116 exit:
117         return retval;
118 }
119 
120 static int skel_release(struct inode *inode, struct file *file)
121 {
122         struct usb_skel *dev;
123 
124         dev = file->private_data;
125         if (dev == NULL)
126                 return -ENODEV;
127 
128         /* allow the device to be autosuspended */
129         mutex_lock(&dev->io_mutex);
130         if (dev->interface)
131                 usb_autopm_put_interface(dev->interface);
132         mutex_unlock(&dev->io_mutex);
133 
134         /* decrement the count on our device */
135         kref_put(&dev->kref, skel_delete);
136         return 0;
137 }
138 
139 static int skel_flush(struct file *file, fl_owner_t id)
140 {
141         struct usb_skel *dev;
142         int res;
143 
144         dev = file->private_data;
145         if (dev == NULL)
146                 return -ENODEV;
147 
148         /* wait for io to stop */
149         mutex_lock(&dev->io_mutex);
150         skel_draw_down(dev);
151 
152         /* read out errors, leave subsequent opens a clean slate */
153         spin_lock_irq(&dev->err_lock);
154         res = dev->errors ? (dev->errors == -EPIPE ? -EPIPE : -EIO) : 0;
155         dev->errors = 0;
156         spin_unlock_irq(&dev->err_lock);
157 
158         mutex_unlock(&dev->io_mutex);
159 
160         return res;
161 }
162 
163 static void skel_read_bulk_callback(struct urb *urb)
164 {
165         struct usb_skel *dev;
166 
167         dev = urb->context;
168 
169         spin_lock(&dev->err_lock);
170         /* sync/async unlink faults aren't errors */
171         if (urb->status) {
172                 if (!(urb->status == -ENOENT ||
173                     urb->status == -ECONNRESET ||
174                     urb->status == -ESHUTDOWN))
175                         dev_err(&dev->interface->dev,
176                                 "%s - nonzero write bulk status received: %d\n",
177                                 __func__, urb->status);
178 
179                 dev->errors = urb->status;
180         } else {
181                 dev->bulk_in_filled = urb->actual_length;
182         }
183         dev->ongoing_read = 0;
184         spin_unlock(&dev->err_lock);
185 
186         wake_up_interruptible(&dev->bulk_in_wait);
187 }
188 
189 static int skel_do_read_io(struct usb_skel *dev, size_t count)
190 {
191         int rv;
192 
193         /* prepare a read */
194         usb_fill_bulk_urb(dev->bulk_in_urb,
195                         dev->udev,
196                         usb_rcvbulkpipe(dev->udev,
197                                 dev->bulk_in_endpointAddr),
198                         dev->bulk_in_buffer,
199                         min(dev->bulk_in_size, count),
200                         skel_read_bulk_callback,
201                         dev);
202         /* tell everybody to leave the URB alone */
203         spin_lock_irq(&dev->err_lock);
204         dev->ongoing_read = 1;
205         spin_unlock_irq(&dev->err_lock);
206 
207         /* submit bulk in urb, which means no data to deliver */
208         dev->bulk_in_filled = 0;
209         dev->bulk_in_copied = 0;
210 
211         /* do it */
212         rv = usb_submit_urb(dev->bulk_in_urb, GFP_KERNEL);
213         if (rv < 0) {
214                 dev_err(&dev->interface->dev,
215                         "%s - failed submitting read urb, error %d\n",
216                         __func__, rv);
217                 rv = (rv == -ENOMEM) ? rv : -EIO;
218                 spin_lock_irq(&dev->err_lock);
219                 dev->ongoing_read = 0;
220                 spin_unlock_irq(&dev->err_lock);
221         }
222 
223         return rv;
224 }
225 
226 static ssize_t skel_read(struct file *file, char *buffer, size_t count,
227                          loff_t *ppos)
228 {
229         struct usb_skel *dev;
230         int rv;
231         bool ongoing_io;
232 
233         dev = file->private_data;
234 
235         /* if we cannot read at all, return EOF */
236         if (!dev->bulk_in_urb || !count)
237                 return 0;
238 
239         /* no concurrent readers */
240         rv = mutex_lock_interruptible(&dev->io_mutex);
241         if (rv < 0)
242                 return rv;
243 
244         if (!dev->interface) {          /* disconnect() was called */
245                 rv = -ENODEV;
246                 goto exit;
247         }
248 
249         /* if IO is under way, we must not touch things */
250 retry:
251         spin_lock_irq(&dev->err_lock);
252         ongoing_io = dev->ongoing_read;
253         spin_unlock_irq(&dev->err_lock);
254 
255         if (ongoing_io) {
256                 /* nonblocking IO shall not wait */
257                 if (file->f_flags & O_NONBLOCK) {
258                         rv = -EAGAIN;
259                         goto exit;
260                 }
261                 /*
262                  * IO may take forever
263                  * hence wait in an interruptible state
264                  */
265                 rv = wait_event_interruptible(dev->bulk_in_wait, (!dev->ongoing_read));
266                 if (rv < 0)
267                         goto exit;
268         }
269 
270         /* errors must be reported */
271         rv = dev->errors;
272         if (rv < 0) {
273                 /* any error is reported once */
274                 dev->errors = 0;
275                 /* to preserve notifications about reset */
276                 rv = (rv == -EPIPE) ? rv : -EIO;
277                 /* report it */
278                 goto exit;
279         }
280 
281         /*
282          * if the buffer is filled we may satisfy the read
283          * else we need to start IO
284          */
285 
286         if (dev->bulk_in_filled) {
287                 /* we had read data */
288                 size_t available = dev->bulk_in_filled - dev->bulk_in_copied;
289                 size_t chunk = min(available, count);
290 
291                 if (!available) {
292                         /*
293                          * all data has been used
294                          * actual IO needs to be done
295                          */
296                         rv = skel_do_read_io(dev, count);
297                         if (rv < 0)
298                                 goto exit;
299                         else
300                                 goto retry;
301                 }
302                 /*
303                  * data is available
304                  * chunk tells us how much shall be copied
305                  */
306 
307                 if (copy_to_user(buffer,
308                                  dev->bulk_in_buffer + dev->bulk_in_copied,
309                                  chunk))
310                         rv = -EFAULT;
311                 else
312                         rv = chunk;
313 
314                 dev->bulk_in_copied += chunk;
315 
316                 /*
317                  * if we are asked for more than we have,
318                  * we start IO but don't wait
319                  */
320                 if (available < count)
321                         skel_do_read_io(dev, count - chunk);
322         } else {
323                 /* no data in the buffer */
324                 rv = skel_do_read_io(dev, count);
325                 if (rv < 0)
326                         goto exit;
327                 else
328                         goto retry;
329         }
330 exit:
331         mutex_unlock(&dev->io_mutex);
332         return rv;
333 }
334 
335 static void skel_write_bulk_callback(struct urb *urb)
336 {
337         struct usb_skel *dev;
338 
339         dev = urb->context;
340 
341         /* sync/async unlink faults aren't errors */
342         if (urb->status) {
343                 if (!(urb->status == -ENOENT ||
344                     urb->status == -ECONNRESET ||
345                     urb->status == -ESHUTDOWN))
346                         dev_err(&dev->interface->dev,
347                                 "%s - nonzero write bulk status received: %d\n",
348                                 __func__, urb->status);
349 
350                 spin_lock(&dev->err_lock);
351                 dev->errors = urb->status;
352                 spin_unlock(&dev->err_lock);
353         }
354 
355         /* free up our allocated buffer */
356         usb_free_coherent(urb->dev, urb->transfer_buffer_length,
357                           urb->transfer_buffer, urb->transfer_dma);
358         up(&dev->limit_sem);
359 }
360 
361 static ssize_t skel_write(struct file *file, const char *user_buffer,
362                           size_t count, loff_t *ppos)
363 {
364         struct usb_skel *dev;
365         int retval = 0;
366         struct urb *urb = NULL;
367         char *buf = NULL;
368         size_t writesize = min(count, (size_t)MAX_TRANSFER);
369 
370         dev = file->private_data;
371 
372         /* verify that we actually have some data to write */
373         if (count == 0)
374                 goto exit;
375 
376         /*
377          * limit the number of URBs in flight to stop a user from using up all
378          * RAM
379          */
380         if (!(file->f_flags & O_NONBLOCK)) {
381                 if (down_interruptible(&dev->limit_sem)) {
382                         retval = -ERESTARTSYS;
383                         goto exit;
384                 }
385         } else {
386                 if (down_trylock(&dev->limit_sem)) {
387                         retval = -EAGAIN;
388                         goto exit;
389                 }
390         }
391 
392         spin_lock_irq(&dev->err_lock);
393         retval = dev->errors;
394         if (retval < 0) {
395                 /* any error is reported once */
396                 dev->errors = 0;
397                 /* to preserve notifications about reset */
398                 retval = (retval == -EPIPE) ? retval : -EIO;
399         }
400         spin_unlock_irq(&dev->err_lock);
401         if (retval < 0)
402                 goto error;
403 
404         /* create a urb, and a buffer for it, and copy the data to the urb */
405         urb = usb_alloc_urb(0, GFP_KERNEL);
406         if (!urb) {
407                 retval = -ENOMEM;
408                 goto error;
409         }
410 
411         buf = usb_alloc_coherent(dev->udev, writesize, GFP_KERNEL,
412                                  &urb->transfer_dma);
413         if (!buf) {
414                 retval = -ENOMEM;
415                 goto error;
416         }
417 
418         if (copy_from_user(buf, user_buffer, writesize)) {
419                 retval = -EFAULT;
420                 goto error;
421         }
422 
423         /* this lock makes sure we don't submit URBs to gone devices */
424         mutex_lock(&dev->io_mutex);
425         if (!dev->interface) {          /* disconnect() was called */
426                 mutex_unlock(&dev->io_mutex);
427                 retval = -ENODEV;
428                 goto error;
429         }
430 
431         /* initialize the urb properly */
432         usb_fill_bulk_urb(urb, dev->udev,
433                           usb_sndbulkpipe(dev->udev, dev->bulk_out_endpointAddr),
434                           buf, writesize, skel_write_bulk_callback, dev);
435         urb->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;
436         usb_anchor_urb(urb, &dev->submitted);
437 
438         /* send the data out the bulk port */
439         retval = usb_submit_urb(urb, GFP_KERNEL);
440         mutex_unlock(&dev->io_mutex);
441         if (retval) {
442                 dev_err(&dev->interface->dev,
443                         "%s - failed submitting write urb, error %d\n",
444                         __func__, retval);
445                 goto error_unanchor;
446         }
447 
448         /*
449          * release our reference to this urb, the USB core will eventually free
450          * it entirely
451          */
452         usb_free_urb(urb);
453 
454 
455         return writesize;
456 
457 error_unanchor:
458         usb_unanchor_urb(urb);
459 error:
460         if (urb) {
461                 usb_free_coherent(dev->udev, writesize, buf, urb->transfer_dma);
462                 usb_free_urb(urb);
463         }
464         up(&dev->limit_sem);
465 
466 exit:
467         return retval;
468 }
469 
470 static const struct file_operations skel_fops = {
471         .owner =        THIS_MODULE,
472         .read =         skel_read,
473         .write =        skel_write,
474         .open =         skel_open,
475         .release =      skel_release,
476         .flush =        skel_flush,
477         .llseek =       noop_llseek,
478 };
479 
480 /*
481  * usb class driver info in order to get a minor number from the usb core,
482  * and to have the device registered with the driver core
483  */
484 static struct usb_class_driver skel_class = {
485         .name =         "skel%d",
486         .fops =         &skel_fops,
487         .minor_base =   USB_SKEL_MINOR_BASE,
488 };
489 
490 static int skel_probe(struct usb_interface *interface,
491                       const struct usb_device_id *id)
492 {
493         struct usb_skel *dev;
494         struct usb_host_interface *iface_desc;
495         struct usb_endpoint_descriptor *endpoint;
496         size_t buffer_size;
497         int i;
498         int retval = -ENOMEM;
499 
500         /* allocate memory for our device state and initialize it */
501         dev = kzalloc(sizeof(*dev), GFP_KERNEL);
502         if (!dev) {
503                 dev_err(&interface->dev, "Out of memory\n");
504                 goto error;
505         }
506         kref_init(&dev->kref);
507         sema_init(&dev->limit_sem, WRITES_IN_FLIGHT);
508         mutex_init(&dev->io_mutex);
509         spin_lock_init(&dev->err_lock);
510         init_usb_anchor(&dev->submitted);
511         init_waitqueue_head(&dev->bulk_in_wait);
512 
513         dev->udev = usb_get_dev(interface_to_usbdev(interface));
514         dev->interface = interface;
515 
516         /* set up the endpoint information */
517         /* use only the first bulk-in and bulk-out endpoints */
518         iface_desc = interface->cur_altsetting;
519         for (i = 0; i < iface_desc->desc.bNumEndpoints; ++i) {
520                 endpoint = &iface_desc->endpoint[i].desc;
521 
522                 if (!dev->bulk_in_endpointAddr &&
523                     usb_endpoint_is_bulk_in(endpoint)) {
524                         /* we found a bulk in endpoint */
525                         buffer_size = usb_endpoint_maxp(endpoint);
526                         dev->bulk_in_size = buffer_size;
527                         dev->bulk_in_endpointAddr = endpoint->bEndpointAddress;
528                         dev->bulk_in_buffer = kmalloc(buffer_size, GFP_KERNEL);
529                         if (!dev->bulk_in_buffer) {
530                                 dev_err(&interface->dev,
531                                         "Could not allocate bulk_in_buffer\n");
532                                 goto error;
533                         }
534                         dev->bulk_in_urb = usb_alloc_urb(0, GFP_KERNEL);
535                         if (!dev->bulk_in_urb) {
536                                 dev_err(&interface->dev,
537                                         "Could not allocate bulk_in_urb\n");
538                                 goto error;
539                         }
540                 }
541 
542                 if (!dev->bulk_out_endpointAddr &&
543                     usb_endpoint_is_bulk_out(endpoint)) {
544                         /* we found a bulk out endpoint */
545                         dev->bulk_out_endpointAddr = endpoint->bEndpointAddress;
546                 }
547         }
548         if (!(dev->bulk_in_endpointAddr && dev->bulk_out_endpointAddr)) {
549                 dev_err(&interface->dev,
550                         "Could not find both bulk-in and bulk-out endpoints\n");
551                 goto error;
552         }
553 
554         /* save our data pointer in this interface device */
555         usb_set_intfdata(interface, dev);
556 
557         /* we can register the device now, as it is ready */
558         retval = usb_register_dev(interface, &skel_class);
559         if (retval) {
560                 /* something prevented us from registering this driver */
561                 dev_err(&interface->dev,
562                         "Not able to get a minor for this device.\n");
563                 usb_set_intfdata(interface, NULL);
564                 goto error;
565         }
566 
567         /* let the user know what node this device is now attached to */
568         dev_info(&interface->dev,
569                  "USB Skeleton device now attached to USBSkel-%d",
570                  interface->minor);
571         return 0;
572 
573 error:
574         if (dev)
575                 /* this frees allocated memory */
576                 kref_put(&dev->kref, skel_delete);
577         return retval;
578 }
579 
580 static void skel_disconnect(struct usb_interface *interface)
581 {
582         struct usb_skel *dev;
583         int minor = interface->minor;
584 
585         dev = usb_get_intfdata(interface);
586         usb_set_intfdata(interface, NULL);
587 
588         /* give back our minor */
589         usb_deregister_dev(interface, &skel_class);
590 
591         /* prevent more I/O from starting */
592         mutex_lock(&dev->io_mutex);
593         dev->interface = NULL;
594         mutex_unlock(&dev->io_mutex);
595 
596         usb_kill_anchored_urbs(&dev->submitted);
597 
598         /* decrement our usage count */
599         kref_put(&dev->kref, skel_delete);
600 
601         dev_info(&interface->dev, "USB Skeleton #%d now disconnected", minor);
602 }
603 
604 static void skel_draw_down(struct usb_skel *dev)
605 {
606         int time;
607 
608         time = usb_wait_anchor_empty_timeout(&dev->submitted, 1000);
609         if (!time)
610                 usb_kill_anchored_urbs(&dev->submitted);
611         usb_kill_urb(dev->bulk_in_urb);
612 }
613 
614 static int skel_suspend(struct usb_interface *intf, pm_message_t message)
615 {
616         struct usb_skel *dev = usb_get_intfdata(intf);
617 
618         if (!dev)
619                 return 0;
620         skel_draw_down(dev);
621         return 0;
622 }
623 
624 static int skel_resume(struct usb_interface *intf)
625 {
626         return 0;
627 }
628 
629 static int skel_pre_reset(struct usb_interface *intf)
630 {
631         struct usb_skel *dev = usb_get_intfdata(intf);
632 
633         mutex_lock(&dev->io_mutex);
634         skel_draw_down(dev);
635 
636         return 0;
637 }
638 
639 static int skel_post_reset(struct usb_interface *intf)
640 {
641         struct usb_skel *dev = usb_get_intfdata(intf);
642 
643         /* we are sure no URBs are active - no locking needed */
644         dev->errors = -EPIPE;
645         mutex_unlock(&dev->io_mutex);
646 
647         return 0;
648 }
649 
650 static struct usb_driver skel_driver = {
651         .name =         "skeleton",
652         .probe =        skel_probe,
653         .disconnect =   skel_disconnect,
654         .suspend =      skel_suspend,
655         .resume =       skel_resume,
656         .pre_reset =    skel_pre_reset,
657         .post_reset =   skel_post_reset,
658         .id_table =     skel_table,
659         .supports_autosuspend = 1,
660 };
661 
662 module_usb_driver(skel_driver);
663 
664 MODULE_LICENSE("GPL");
665 
