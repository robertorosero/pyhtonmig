"""This test checks for correct wait3() behavior.
"""

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
