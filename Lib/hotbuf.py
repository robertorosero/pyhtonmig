#!/usr/bin/env python

"""
A buffer class for fast I/O.
"""

from _hotbuf import _hotbuf
from struct import Struct

_long = Struct('l')

class hotbuf(_hotbuf):
    pass

