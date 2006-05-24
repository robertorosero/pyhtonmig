# Test the hotbuf module.
#
# $Id$
#
#  Copyright (C) 2005   Gregory P. Smith (greg@electricrain.com)
#  Licensed to PSF under a Contributor Agreement.
#

from hotbuf import hotbuf
import unittest
from test import test_support


class HotbufTestCase(unittest.TestCase):

    def test_base( self ):
        CAPACITY = 1024

        # Create a new hotbuf
        self.assertRaises(ValueError, hotbuf, -1)
        self.assertRaises(ValueError, hotbuf, 0)
        b = hotbuf(CAPACITY)
        self.assertEquals(len(b), CAPACITY)
        self.assertEquals(b.capacity(), CAPACITY)
        
        # Play with the position
        assert b.position() == 0
        b.setposition(10)
        self.assertEquals(b.position(), 10)
        self.assertRaises(IndexError, b.setposition, CAPACITY + 1)

        # Play with the limit
        assert b.limit() == CAPACITY
        b.setlimit(CAPACITY - 10)
        self.assertEquals(b.limit(), CAPACITY - 10)
        self.assertRaises(IndexError, b.setlimit, CAPACITY + 1)
        b.setlimit(b.position() - 1)
        self.assertEquals(b.position(), b.limit())

        # Play with reset before the mark has been set.
        self.assertRaises(IndexError, b.setlimit, CAPACITY + 1)
        
        # Play with the mark
        b.setposition(10)
        b.setlimit(100)
        b.setmark()
        b.setposition(15)
        self.assertEquals(b.mark(), 10)

        # Play with clear
        b.clear()
        self.assertEquals((b.position(), b.limit(), b.mark()),
                          (0, CAPACITY, -1))

        # Play with flip.
        b.setposition(42)
        b.setlimit(104)
        b.setmark()
        b.flip()
        self.assertEquals((b.position(), b.limit(), b.mark()),
                          (0, 42, -1))
        
        # Play with rewind.
        b.setposition(42)
        b.setlimit(104)
        b.setmark()
        b.rewind()
        self.assertEquals((b.position(), b.limit(), b.mark()),
                          (0, 104, -1))

        # Play with remaining.
        self.assertEquals(b.remaining(), 104)

        
def test_main():
    test_support.run_unittest(HotbufTestCase)


if __name__ == "__main__":
    test_main()

