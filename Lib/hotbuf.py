#!/usr/bin/env python

"""
A buffer class for fast I/O.
"""

from _hotbuf import _hotbuf
from struct import Struct

_long = Struct('l')

class hotbuf(_hotbuf):

    def getlong( self ):
        r = _long.unpack_from(self, 0)
        self.setposition(self.position + _long.size)
        return r

##     def putlong( self ):
##         s = _long.pack(0)
##         self.setposition(self.position + _long.size)
##         return 


