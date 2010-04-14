# Code demonstrating 'Unknown signal 32' failure on Gentoo
# x86 buildbot.  See http://bugs.python.org/issue4970.

import os
import pprint
import time
import _thread

pprint.pprint(os.confstr_names)
print(os.confstr('CS_GNU_LIBC_VERSION'))
print(os.confstr('CS_GNU_LIBPTHREAD_VERSION'))


try:
    os.execv('/usr/bin/dorothyq', ['dorothyq'])
except OSError:
    pass

def f():
    time.sleep(1.0)

_thread.start_new(f, ())
