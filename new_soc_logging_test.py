import httplib
reload(httplib)
import fakesocket
import logging
from cStringIO import StringIO

origsocket=httplib.socket
# monkeypatch -- replace httplib.socket with our own fake module
httplib.socket=fakesocket

# ... run the tests ...
log=logging.getLogger("py.httplib")
stringLog = StringIO()

# define the handler and level
handler = logging.StreamHandler(stringLog)
log.setLevel(logging.INFO)

# add the handler to the logger
log.addHandler(handler)

httplib._log.info("message 1") # 1st test

myconn = httplib.HTTPConnection('www.google.com')
myconn.set_debuglevel(43)
if myconn.debuglevel > 0:
	print "Debug level is > 0"

#myconn.connect()
httplib.HTTPConnection('MOCK')
myconn.putrequest("GET", "/search?q=python")
#myconn.getresponse()

print stringLog.getvalue()  # For testing purposes

if stringLog.getvalue() != "Error:  It worked":
    print "it worked"
else:
    print "it didn't work"

# restore it to working order, so other tests won't fail
httplib.socket=origsocket