#!/usr/bin/python 
#------------------------------------------------------------------------------
# ELE784: unit write
#------------------------------------------------------------------------------
from fcntl import ioctl

# START HERE ------------------------------------------------------------------
DEVPATH = "/dev/etsele_cdev"

msg = "There's only one man who would dare give me the raspberry: \n\
[pulls down helmet as camera zooms in on his face] \n\
Lone Starr!\n"

fh = open(DEVPATH, 'w'); 
fh.write(msg)
fh.close()
# EOF -------------------------------------------------------------------------
