import robotparser
import logging
from cStringIO import StringIO

# ... run the tests ...
log=logging.getLogger("py.robotparser")
stringLog = StringIO()

# define the handler and level
handler = logging.StreamHandler(stringLog)
log.setLevel(logging.INFO)

# add the handler to the logger
log.addHandler(handler)

robotparser._log.info("message 1") # 1st test

print stringLog.getvalue()  # For testing purposes

if stringLog.getvalue() != "Error:  It worked":
    print "it worked"
else:
    print "it didn't work"