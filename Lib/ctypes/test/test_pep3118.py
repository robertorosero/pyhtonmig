import unittest
from ctypes import *
import struct, sys

if sys.byteorder == "little":
    ENDIAN = "<"
else:
    ENDIAN = ">"

simple_types = [
    ("b", c_byte),
    ("B", c_ubyte),
    ("h", c_short),
    ("H", c_ushort),
# c_int and c_uint may be aliases to c_long
##    ("i", c_int),
##    ("I", c_uint),
    ("l", c_long),
    ("L", c_ulong),
    ("q", c_longlong),
    ("Q", c_ulonglong),
    ("f", c_float),
    ("d", c_double),
# c_longdouble may be an alias to c_double
##    ("g", c_longdouble),
    ("t", c_bool),
# struct doesn't support this (yet)
##    ("O", py_object),
]

class Test(unittest.TestCase):

    def test_simpletypes(self):
        # simple types in native byte order
        for fmt, typ in simple_types:
            v = memoryview(typ())

            # check the PEP3118 format string
            self.failUnlessEqual(v.format, ENDIAN + fmt)

            # shape and strides are None for integral types
            self.failUnlessEqual((v.shape, v.strides),
                                 (None, None))

            # size and itemsize must be what struct.calcsize reports
            struct_size = struct.calcsize(fmt)
            self.failUnlessEqual((v.size, v.itemsize),
                                 (struct_size, struct_size))

if __name__ == "__main__":
    unittest.main()
