"""Validate the kernel TX input boundary without transmitting anything (device)."""
import errno
import fcntl
import os
import struct

fd=os.open('/dev/rfid_mfc_tx',os.O_RDWR)
cases=[(0,bytes(16),errno.ENOTTY),
       (0x40104e42,struct.pack('IIQ',0,0,0),errno.EINVAL),
       (0x40104e42,struct.pack('IIQ',513,0,0),errno.EINVAL),
       (0x40104e42,struct.pack('IIQ',4,1,0),errno.EINVAL),
       (0x40104e42,struct.pack('IIQ',4,0,0),errno.EFAULT)]
try:
    for command,data,wanted in cases:
        try: fcntl.ioctl(fd,command,data)
        except OSError as exc: assert exc.errno==wanted,(exc.errno,wanted)
        else: raise AssertionError('invalid request accepted')
finally: os.close(fd)
print('Five invalid ioctl boundary checks passed; no transmission requested')
