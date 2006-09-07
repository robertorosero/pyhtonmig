import shlex
import logging
from cStringIO import StringIO

# ... run the tests ...
log=logging.getLogger("py.shlex")
stringLog = StringIO()

# define the handler and level
handler = logging.StreamHandler(stringLog)
log.setLevel(logging.INFO)

# add the handler to the logger
log.addHandler(handler)

shlex._log.info("message 1")

print stringLog.getvalue()  # For testing purposes

if stringLog.getvalue()  == "message 1" + "\n":
    print "it worked"
else:
    print "it didn't work"