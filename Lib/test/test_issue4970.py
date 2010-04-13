# Code demonstrating 'Unknown signal 32' failure on Gentoo
# x86 buildbot.  See http://bugs.python.org/issue4970.

# Output signals, to find out which one signal 32 is.
import signal
from pprint import pprint
pprint(signal.__dict__)

import os
import time
import _thread

try:
    os.execv('/usr/bin/dorothyq', ['dorothyq'])
except OSError:
    pass

def f():
    time.sleep(1.0)

_thread.start_new(f, ())
