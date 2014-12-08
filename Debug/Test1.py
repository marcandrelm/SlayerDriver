#!/usr/bin/python
#------------------------------------------------------------------------------
# ELE784: unit test1
#   open and close
#------------------------------------------------------------------------------
from fcntl import ioctl
import os

# TEST START HERE -------------------------------------------------------------
DEVPATH = "/home/ens/AJ74860/Bureau/watch"
DEVPATH = "/dev/etsele_cdev"
msg = "Poppadom?\nGelato!\nPapoy?"
msg_len = len(msg)

fh1 = os.open(DEVPATH, os.O_RDONLY)
fh2 = os.open(DEVPATH, os.O_RDWR)
fh3 = os.open(DEVPATH, os.O_RDONLY)
try:
    fh4 = os.open(DEVPATH, os.O_WRONLY)
except (IOError, OSError):
    #print e
    #print "Last print should be ENOTTY"
    pass   
os.write(fh2, msg)
str = os.read(fh2, msg_len) 
print str

os.close(fh2)
fh5 = os.open(DEVPATH, os.O_WRONLY)
os.close(fh1)
os.close(fh3)
os.close(fh5)
print "Pass"
# EOF -------------------------------------------------------------------------
