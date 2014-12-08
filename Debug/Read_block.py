#!/usr/bin/python 
#------------------------------------------------------------------------------
# ELE784: unit Read
#------------------------------------------------------------------------------
from fcntl import ioctl
import sys, getopt 


# START HERE ------------------------------------------------------------------
print 'Number of arguments:', len(sys.argv), 'arguments.'
print 'Argument List:', str(sys.argv)




def main(argv):
    n = ''
    try:
        opts, args = getopt.getopt(argv,"hn:")
    except getopt.GetoptError:
        print 'test.py -n <uint_ToRead>'
        sys.exit(2)
    for opt, arg in opts:
        if opt == '-h':
            print 'test.py -n <uint_ToRead>'
            sys.exit()
        elif opt in ("-n"):
            if str.isdigit(arg):
                n = int(arg)
            else:
                print "-n did not receive a number"
                sys.exit()
         

    print 'Char to read is', n 
    print ''
    fh = open("/dev/etsele_cdev", 'r', 0);
    print fh.read(n)
    fh.close()
    
if __name__ == "__main__":
   main(sys.argv[1:])
# EOF -------------------------------------------------------------------------
