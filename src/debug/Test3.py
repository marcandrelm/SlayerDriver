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
GETNUMDATA   = 0x80042a00  
GETNUMREADER = 0x80042a01    
GETBUFSIZE   = 0x80042a02   
SETBUFSIZE   = 0x40042a03    
GETNUMUSERS  = 0x80042a04  
GETMAXOPEN   = 0x80042a05 
SETMAXOPEN   = 0x40042a06  
PRINT_IOCTL  = 0x00002a07 
PRINT_STATS  = 0x00002a08  


#------------------------------------------------------------------------------
DEVPATH = "/dev/elecam0"

arg = array.array('L', [0])

fh = open(DEVPATH, 'r', 0);

# Save setting
ioctl(fh, GETBUFSIZE, arg)
ORGBUFSIZE = arg[0]
ioctl(fh, GETMAXOPEN, arg)
ORGMAXOPEN = arg[0]

# TEST START HERE -------------------------------------------------------------
print "Basic Ctl ---------"
ioctl(fh, PRINT_IOCTL, 0)
ioctl(fh, PRINT_STATS, 0)

ioctl(fh, GETNUMDATA, arg)
print "  Buf as",arg[0],"data."

ioctl(fh, GETNUMREADER, arg)

print "  There is",arg[0],"reader."

ioctl(fh, GETNUMUSERS, arg)
print "  There is",arg[0],"open."

# Bufsize ---------
print "Bufsize ---------"
ioctl(fh, GETBUFSIZE, arg)
print "  Buf is",arg[0]
arg[0] = 512
ioctl(fh, SETBUFSIZE, arg)
print "  Changing to",arg[0] 
ioctl(fh, GETBUFSIZE, arg)
print "  Buf is",arg[0] 
ioctl(fh, GETNUMDATA, arg)
print "  data is",arg[0] 
try:
    print "  Testing illegal change"
    arg[0] = msg_len-10
    ioctl(fh, SETBUFSIZE, arg)
except IOError,e:
    if  e.errno == 105:
        pass
    else:
        raise Exception(e)
       
ioctl(fh, GETBUFSIZE, arg)
print "  Buf is",arg[0]

# MaxOpen ---------
print "MaxOpen ---------"
ioctl(fh, GETMAXOPEN , arg)
print "  Max open is ",arg[0]
arg[0] = 15  
ioctl(fh, SETMAXOPEN, arg)
print "  Changing to ",arg[0]
ioctl(fh, GETMAXOPEN, arg)
print "  Max open is ",arg[0]    
try:
    print "  Test illegal change" 
    arg[0] = 1
    ioctl(fh, SETMAXOPEN, arg)
except IOError,e:
    if  e.errno == 105:
        pass
    else:
        raise Exception(e)
ioctl(fh, GETMAXOPEN, arg)
print "  Max open is ",arg[0]

print "----------------"
print "Pass"
# clean up --------------------------------------------------------------------
arg[0] = ORGBUFSIZE
ioctl(fh, SETBUFSIZE, arg)
arg[0] = ORGMAXOPEN
ioctl(fh, SETMAXOPEN, arg)
fh.close()
fh1.close()
# EOF -------------------------------------------------------------------------
