# !/usr/bin/env python

""" 
Test harness for the standard library logging module.
This does not work at all (I don't know what I'm doing).
"""

import logging, asyncore

logging.basicConfig()

log = logging.getLogger("py.asyncore")
log.setLevel(logging.DEBUG)     # The level is set to DEBUG so nothing will be ignored

