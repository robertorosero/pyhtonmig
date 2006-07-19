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
class FakeSocket:
    def __init__(self, text, fileclass=StringIO.StringIO):
        self.text = text
        self.fileclass = fileclass

    def makefile(self, mode, bufsize=None):
        if mode != 'r' and mode != 'rb':
            raise httplib.UnimplementedFileMode()
        return self.fileclass(self.text)

sock = FakeSocket("socket")

httplib._log.info("message 1") # first stage of testing

r = httplib.HTTPResponse(sock) # second stage of testing
r.begin() # call the begin method

# class test:
#	def someTest:
#		self.msg == None
#		self._read_status == "message 1" == CONTINUE
#		skip != True
#		self.debuglevel > 0


print stringLog.getvalue()  # For testing purposes

if stringLog.getvalue() != "Error:  It worked":
    print "it worked"
else:
    print "it didn't work"