# !/usr/bin/env python

""""
Test harness for the standard library logging module.

"""

import logging
import httplib
import socket
from cStringIO import StringIO

log=logging.getLogger("py.httplib")
stringLog = StringIO()

# define the handler and level
handler = logging.StreamHandler(stringLog)
log.setLevel(logging.INFO)

# Set up module-level logger. Found on Google, don't know if this works for sure yet
console = logging.StreamHandler()
console.setLevel(logging.INFO)
console.setFormatter(logging.Formatter('%(asctime)s : %(message)s',
                                       '%Y-%m-%d %H:%M:%S'))
loggerName = None
logging.getLogger(loggerName).addHandler(console)
del console

# add the handler to the logger
log.addHandler(handler)

# create socket
sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

# httplib.HTTPResponse(sock).log("message 1") # says there is no attribute for "log"
httplib._log.info("message 1")
# httplib.HTTPConnection(sock.connect).log("message 2")

print stringLog.getvalue()  # For testing purposes

if stringLog.getvalue() != "Error:  It worked":
    print "it worked"
else:
    print "it didn't work"