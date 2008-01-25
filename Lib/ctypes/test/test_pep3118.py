import unittest
from ctypes import *
import re, struct, sys

if sys.byteorder == "little":
    ENDIAN = "<"
else:
    ENDIAN = ">"

def normalize(format):
    # Remove current endian specifier and white space from a format
    # string
    format = format.replace(ENDIAN, "")
    format = format.replace("=", "")
    return re.sub(r"\s", "", format)

class Test(unittest.TestCase):

    def test_types(self):
        for tp, fmt, shape, itemtp in types:
            ob = tp()
            v = memoryview(ob)
            try:
                self.failUnlessEqual(normalize(v.format), fmt)
                self.failUnlessEqual(v.size, sizeof(ob))
                self.failUnlessEqual(v.itemsize, sizeof(itemtp))
                self.failUnlessEqual(v.shape, shape)
                # ctypes object always have a non-strided memory block
                self.failUnlessEqual(v.strides, None)
                # they are always read/write
                self.failIf(v.readonly)

                if v.shape:
                    n = 1
                    for dim in v.shape:
                        n = n * dim
                    self.failUnlessEqual(v.itemsize * n, v.size)
            except:
                # so that we can see the failing type
                print(tp)
                raise

# define some structure classes

class Point(Structure):
    _fields_ = [("x", c_long), ("y", c_long)]

class PackedPoint(Structure):
    _pack_ = 2
    _fields_ = [("x", c_long), ("y", c_long)]

class Point2(Structure):
    pass
Point2._fields_ = [("x", c_long), ("y", c_long)]

class EmptyStruct(Structure):
    _fields_ = []

class aUnion(Union):
    _fields_ = [("a", c_int)]

types = [
    # type                      format          shape           calc itemsize

    ## simple types

    (c_char,                    "c",            None,           c_char),
    (c_byte,                    "b",            None,           c_byte),
    (c_ubyte,                   "B",            None,           c_ubyte),
    (c_short,                   "h",            None,           c_short),
    (c_ushort,                  "H",            None,           c_ushort),

    # c_int and c_uint may be aliases to c_long
    #(c_int,                     "i",            None,           c_int),
    #(c_uint,                    "I",            None,           c_uint),

    (c_long,                    "l",            None,           c_long),
    (c_ulong,                   "L",            None,           c_ulong),

    # c_longlong and c_ulonglong are aliases on 64-bit platforms
    #(c_longlong,                "q",            None,           c_longlong),
    #(c_ulonglong,               "Q",            None,           c_ulonglong),

    (c_float,                   "f",            None,           c_float),
    (c_double,                  "d",            None,           c_double),
    # c_longdouble may be an alias to c_double

    (c_bool,                    "t",            None,           c_bool),
    (py_object,                 "O",            None,           py_object),

    ## pointers

    (POINTER(c_byte),           "&b",           None,           POINTER(c_byte)),
    (POINTER(POINTER(c_long)),  "&&l",          None,           POINTER(POINTER(c_long))),

    ## arrays and pointers

    (c_double * 4,              "(4)d",         (4,),           c_double),
    (c_float * 4 * 3 * 2,       "(2,3,4)f",     (2,3,4),        c_float),
    (POINTER(c_short) * 2,      "(2)&h",        (2,),           POINTER(c_short)),
    (POINTER(c_short) * 2 * 3,  "(3,2)&h",      (3,2,),         POINTER(c_short)),
    (POINTER(c_short * 2),      "&(2)h",        None,           POINTER(c_short)),

    ## structures and unions

    (Point,                     "T{l:x:l:y:}",  None,           Point),
    # packed structures do not implement the pep
    (PackedPoint,               "B",            None,           PackedPoint),
    (Point2,                    "T{l:x:l:y:}",  None,           Point2),
    (EmptyStruct,               "T{}",          None,           EmptyStruct),
    # the pep does't support unions
    (aUnion,                    "B",            None,           aUnion),

    ## other

    # function signatures are not implemented
    (CFUNCTYPE(None),           "X{}",          None,           CFUNCTYPE(None)),

    ]

if __name__ == "__main__":
    unittest.main()
