#!/usr/bin/python 
#------------------------------------------------------------------------------
# ELE784: unit Read
#------------------------------------------------------------------------------
from fcntl import ioctl
import array

#ioctl functions
GETNUMDATA   = 0x80042a00

# START HERE ------------------------------------------------------------------
arg = array.array('L', [0])
fh = open("/dev/etsele_cdev", 'r', 0);
ioctl(fh, GETNUMDATA, arg)
print fh.read(arg[0])
fh.close()
# EOF -------------------------------------------------------------------------
