"""When called as a script, print a comma-separated list of the open
file descriptors on stdout."""

import errno
import os
import fcntl
import subprocess
import sys

try:
    _MAXFD = os.sysconf("SC_OPEN_MAX")
except:
    _MAXFD = 256

def isopen(fd):
    """Return True if the fd is open, and False otherwise"""
    try:
        fcntl.fcntl(fd, fcntl.F_GETFD, 0)
    except IOError as e:
        if e.errno == errno.EBADF:
            return False
        raise
    return True

if __name__ == "__main__":
    fds = [fd for fd in range(0, _MAXFD) if isopen(fd)]
    print(','.join(map(str, fds)))
    if '--debug' in sys.argv[1:]:
        for fd in fds:
            print(fd, fcntl.fcntl(fd, fcntl.F_GETFL), os.fstat(fd))
        sys.stdout.flush()
        subprocess.call("ls -la /proc/{}/fd/".format(os.getpid()), shell=True)
        if 5 in fds:
            subprocess.call("stat /proc/{}/fd/{}".format(os.getpid(), 5), shell=True)
