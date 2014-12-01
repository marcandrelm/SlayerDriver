#!/usr/bin/python 
#------------------------------------------------------------------------------
# ELE784: unit test3
#
# test case : 
#   IOCTL
#------------------------------------------------------------------------------
from fcntl import ioctl 
import array
import os
import time

#ioctl functions
IOCTL_GET           = 0x80042d10  
IOCTL_SET           = 0x40042d20    
IOCTL_STREAMON      = 0x40042d30   
IOCTL_STREAMOFF     = 0x40042d40    
IOCTL_GRAB          = 0x80042d50  
IOCTL_PANTILT       = 0x40042d60 
IOCTL_PANTILT_RESET = 0x00002d61

#------------------------------------------------------------------------------
DEVPATH = "/dev/elecam0"

arg = array.array('L', [0])

fh = open(DEVPATH, 'r', 0);


ioctl(fh, IOCTL_PANTILT_RESET, 0)

# clean up --------------------------------------------------------------------
fh.close()
# EOF -------------------------------------------------------------------------
