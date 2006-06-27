# !/usr/bin/env python

"""

Test harness for the standard library logging module.

"""

import logging
import asyncore
from cStringIO import StringIO

log=logging.getLogger("py.asyncore")
stringLog = StringIO()

# define the handler and level
handler = logging.StreamHandler(stringLog)
log.setLevel(logging.INFO)

# set a format for the output
formatter = logging.Formatter('%(name)-12s: %(levelname)-8s %(message)s')
handler.setFormatter(formatter)

# add the handler to the logger
log.addHandler(handler)

asyncore.dispatcher().log("message")
print stringLog.getvalue()  # For testing purposes

if stringLog.getvalue() != "Error:  It worked":
    print "it worked"
else:
    print "it didn't work"
