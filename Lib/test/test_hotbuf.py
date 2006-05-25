# Test the hotbuf module.
#
# $Id$
#
#  Copyright (C) 2006   Martin Blais <blais@furius.ca>
#  Licensed to PSF under a Contributor Agreement.
#

from hotbuf import hotbuf
import unittest
from test import test_support


CAPACITY = 1024
MSG = 'Martin Blais was here scribble scribble.'


class HotbufTestCase(unittest.TestCase):

    def test_base( self ):
        # Create a new hotbuf
        self.assertRaises(ValueError, hotbuf, -1)
        self.assertRaises(ValueError, hotbuf, 0)
        b = hotbuf(CAPACITY)
        self.assertEquals(len(b), CAPACITY)
        self.assertEquals(b.capacity, CAPACITY)

        # Play with the position
        assert b.position == 0
        b.setposition(10)
        self.assertEquals(b.position, 10)
        self.assertRaises(IndexError, b.setposition, CAPACITY + 1)

        # Play with the limit
        assert b.limit == CAPACITY
        b.setlimit(CAPACITY - 10)
        self.assertEquals(b.limit, CAPACITY - 10)
        self.assertRaises(IndexError, b.setlimit, CAPACITY + 1)
        b.setlimit(b.position - 1)
        self.assertEquals(b.position, b.limit)

        # Play with reset before the mark has been set.
        self.assertRaises(IndexError, b.setlimit, CAPACITY + 1)

        # Play with the mark
        b.setposition(10)
        b.setlimit(100)
        b.setmark()
        b.setposition(15)
        self.assertEquals(b.mark, 10)

        # Play with clear
        b.clear()
        self.assertEquals((b.position, b.limit, b.mark),
                          (0, CAPACITY, -1))

        # Play with flip.
        b.setposition(42)
        b.setlimit(104)
        b.setmark()
        b.flip()
        self.assertEquals((b.position, b.limit, b.mark),
                          (0, 42, -1))

        # Play with rewind.
        b.setposition(42)
        b.setlimit(104)
        b.setmark()
        b.rewind()
        self.assertEquals((b.position, b.limit, b.mark),
                          (0, 104, -1))

        # Play with remaining.
        self.assertEquals(b.remaining(), 104)
        b.setposition(10)
        self.assertEquals(b.remaining(), 94)

    def test_compact( self ):
        b = hotbuf(CAPACITY)

        b.setposition(100)
        b.setlimit(200)
        m = b.mark
        b.compact()
        self.assertEquals((b.position, b.limit, b.mark),
                          (100, CAPACITY, -1))

    def test_byte( self ):
        b = hotbuf(256)

        # Fill up the buffer with bytes.
        for x in xrange(256):
            b.putbyte(x)

        # Test overflow.
        self.assertRaises(IndexError, b.putbyte, 42)

        # Read all data from the buffer.
        b.flip()
        for x in xrange(256):
            nx = b.getbyte()
            assert nx == x

        # Test underflow.
        self.assertRaises(IndexError, b.putbyte, 42)

    def test_str( self ):
        b = hotbuf(256)

        # Write into the buffer
        b.putstr(MSG)
        b.flip()

        # Read back and assert message
        self.assertEquals(b.getstr(len(MSG)), MSG)
        
        # Test overflow.
        b.flip()
        self.assertRaises(IndexError, b.putstr, ' ' * 1000)
        
        # Test underflow.
        self.assertRaises(IndexError, b.getstr, 1000)

    def test_conversion( self ):
        b = hotbuf(CAPACITY)

        b.setposition(100)
        b.setlimit(132)

        self.assertEquals(len(b), 32)
        s = str(b)
        self.assertEquals(len(s), 32)

        r = repr(b)
        self.assert_(r.startswith('<hotbuf '))
        
    def test_compare( self ):
        b = hotbuf(CAPACITY)

## FIXME we need a few methods to be able to write strings into and out of it




def test_main():
    test_support.run_unittest(HotbufTestCase)

if __name__ == "__main__":
    test_main()

