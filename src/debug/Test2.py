#!/usr/bin/python
#------------------------------------------------------------------------------
# ELE784: unit test1
#
# test case : 
#   Write Read
#
#------------------------------------------------------------------------------
from fcntl import ioctl
import array
import os

# TEST START HERE -------------------------------------------------------------
DEVPATH = "/dev/etsele_cdev"
Warning_msg = "This is a TEST, Warning\nThis is a TEST\nCountdown set to 5:00 min"
msg_len = len(Warning_msg)

cdev_fh = os.open(DEVPATH, os.O_RDWR, 0);
os.write(cdev_fh, Warning_msg)
str = os.read(cdev_fh, msg_len) 
print str
if len(str) == msg_len:
    print "PASS1"
else:
    print "FAIL"
os.close(cdev_fh);
print "-----------------------------"
cdev_fh = open(DEVPATH, "w", 0);
cdev_fh.write(Warning_msg)
cdev_fh.close();
cdev_fh = open(DEVPATH, "r", 0);
str = cdev_fh.read(msg_len) 
print str
if len(str) == msg_len:
    print "PASS2"
else:
    print "FAIL"
cdev_fh.close();
# EOF -------------------------------------------------------------------------
