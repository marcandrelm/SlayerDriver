#!/usr/bin/python
#------------------------------------------------------------------------------
# ELE784: unit test4
#
# test case : 
#   blocking vs non blocking
#------------------------------------------------------------------------------
from fcntl import ioctl
import array
import os

#IOctl functions
GETNUMDATA   = 0x80042a00
PRINT_STATS  = 0x00002a08 

#------------------------------------------------------------------------------
DEVPATH = "/dev/etsele_cdev"
text = "Alice was beginning to get very tired of sitting by her sister on \
the bank, and of having nothing to do: once or twice she had peeped into the \
book her sister was reading, but it had no pictures or conversations in it, \
'and what is the use of a book,' thought Alice 'without pictures or \
conversations?'\
\
So she was considering in her own mind (as well as she could, for the hot day \
made her feel very sleepy and stupid), whether the pleasure of making a \
daisy-chain would be worth the trouble of getting up and picking the daisies, \
when suddenly a White Rabbit with pink eyes ran close by her. "
text_len = len(text)

# TEST START HERE -------------------------------------------------------------
# Nonblock mode -----------------------
fh = os.open(DEVPATH, os.O_NONBLOCK|os.O_RDWR, 0);
os.write(fh, text)
print "Text is",text_len,"char long"
str = os.read(fh, text_len)
print "The following text should be broken at the end" 
print str
print "Check here --^"
os.close(fh);

# Block mode (RDWR) -------------------
fh = os.open(DEVPATH, os.O_RDWR, 0);
try:
    os.write(fh, text)
except (IOError, OSError),e:
    if  e.errno == 11:
        print "RDWR Pass"
        pass
    else:
        print e
        print e.errno
        raise Exception("unknown bug")        
# Flush -----------
arg = array.array('L', [0])
ioctl(fh, GETNUMDATA, arg)
os.read(fh, arg[0])
os.close(fh);

# Block mode (WR) ---------------------
print " "
print "!!!Next part must be done manualy!!!"
print "Use 'python Read_block.py -n NBCHAR' to unblock write" 
print " "
print "Going to write",text_len,"char"
#fh = open(DEVPATH, 'w', 0);
#ioctl(fh, PRINT_STATS, 0)
#fh.write(text)
#fh.close();

fh = os.open(DEVPATH, os.O_WRONLY);
ioctl(fh, PRINT_STATS, 0)
os.write(fh, text)
os.close(fh);

# EOF -------------------------------------------------------------------------

