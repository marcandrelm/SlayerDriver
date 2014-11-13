Driver has beed tested an compiled using LINUX 3.2.34etsele

--- INSTALL -------------------------------------------------------------------
To install:
        go to src
        use the functions “Make” and the “Make load”
    or
        go to /bin/
        sudo insmod CharDriver.ko.debug

--- Function ------------------------------------------------------------------

OPEN ------------------------------------------------------
    Open an instance of the driver
open -mode -blockig
    mode:
    -r ; for read,
    -w ; for write or 
    -rw; for read/write
    blocking:
    -b ; will open it in blocking mode
    -nb; will not block 

ex: open -r -nb

CLOSE -----------------------------------------------------
    Close current file
    No arguments to be specified
ex: close
READ ------------------------------------------------------
    Read from the driver
read -bytes
    -bytes is to be the numeric value for the amount of characters to read

ex: read 10

WRITE -----------------------------------------------------
    Write to the driver
write “string”
    string is a alphanumeric string of characters to write to the buffer.

ex: write "Calvin is not a vengful reviewer..."

IOCTL -----------------------------------------------------
    Divers functions to access lower level data
ioctl -function -paramater
    function: 
    GETNUMDATA	; Returns the quantity of data in the buffer
    GETNUMREADER; Returns the number of current readers of the file
    GETBUFSIZE	; Returns the current size of the circular buffer
    SETBUFSIZE	; Sets the size of the circular buffer.  
                  paramater must be a numeric value representing the number 
                  of bytes to set to the buffer
    GETNUMUSERS	; Returns the number of current users who have opened the driver
    GETMAXOPEN	; Returns the maximum number of users who have opened the driver
    SETMAXOPEN	; Sets the maximum number of users who can open the driver
                  simultaneously.
                  paramater must be a numeric value
    paramater:
        useful for SET__ functions.
        
