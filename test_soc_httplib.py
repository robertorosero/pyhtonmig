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

# set a format for the output
formatter = logging.Formatter('%(name)s:  %(levelname)s %(message)s')
handler.setFormatter(formatter)

# add the handler to the logger
log.addHandler(handler)

# create socket
sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

httplib.HTTPResponse(sock).log("message 1")
httplib.HTTPConnection().log("message 2")
print stringLog.getvalue()  # For testing purposes

if stringLog.getvalue() != "Error:  It worked":
    print "it worked"
else:
    print "it didn't work"
