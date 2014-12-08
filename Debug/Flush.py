#!/usr/bin/python 
#------------------------------------------------------------------------------
# ELE784: unit Flush buffer
#------------------------------------------------------------------------------
from fcntl import ioctl 
import array

#ioctl functions
GETNUMDATA   = 0x80042a00 

# TEST START HERE -------------------------------------------------------------
DEVPATH = "/dev/etsele_cdev"
arg = array.array('L', [0])

cdev_fh = open(DEVPATH, 'r', 0);

ioctl(cdev_fh, GETNUMDATA, arg)
print "Data is",arg[0]

cdev_fh.read(arg[0])
ioctl(cdev_fh, GETNUMDATA, arg)
print "Data is",arg[0]

cdev_fh.close()
# EOF -------------------------------------------------------------------------
