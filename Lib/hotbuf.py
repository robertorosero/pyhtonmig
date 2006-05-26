#!/usr/bin/env python

"""
A buffer class for fast I/O.
"""

from _hotbuf import _hotbuf
from struct import Struct

_long = Struct('l')

class hotbuf(_hotbuf):

    def pack( self, structobj, *values ):
        """
        Pack using the given Struct object 'structobj', the remaining arguments.
        """
        structobj.pack_to(self, 0, *values)
        self.advance(structobj.size)

    def unpack( self, structobj ):
        """
        Pack using the given Struct object 'structobj', the remaining arguments.
        """
        values = structobj.unpack_from(self, 0)
        self.advance(structobj.size)
        return values


