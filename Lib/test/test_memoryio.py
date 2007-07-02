"""Unit tests for memory-based file-like objects.
StringIO -- for unicode strings
BytesIO -- for bytes
"""

import unittest
from test import test_support

import io

try:
    import _string_io, _bytes_io
    has_c_implementation = True
except ImportError:
    has_c_implementation = False


class MemoryTestMixin:

    def write_ops(self, f):
        t = self.buftype
        self.assertEqual(f.write(t("blah.")), 5)
        self.assertEqual(f.seek(0), 0)
        self.assertEqual(f.write(t("Hello.")), 6)
        self.assertEqual(f.tell(), 6)
        self.assertEqual(f.seek(-1, 1), 5)
        self.assertEqual(f.tell(), 5)
        self.assertEqual(f.write(t(" world\n\n\n")), 9)
        self.assertEqual(f.seek(0), 0)
        self.assertEqual(f.write(t("h")), 1)
        self.assertEqual(f.seek(-1, 2), 13)
        self.assertEqual(f.tell(), 13)
        self.assertEqual(f.truncate(12), 12)
        self.assertEqual(f.tell(), 12)

    def test_write(self):
        buf = self.buftype("hello world\n")
        memio = self.ioclass(buf)

        self.write_ops(memio)

    def test_write_noinit(self):
        memio = self.ioclass()

        self.write_ops(memio)

    def test_writelines(self):
        buf = self.buftype("1234567890")
        memio = self.ioclass()

        memio.writelines([buf] * 100)
        self.assertEqual(memio.getvalue(), buf * 100)

    def test_writelines_error(self):
        memio = self.ioclass()
        def error_gen():
            yield self.buftype('spam')
            raise KeyboardInterrupt

        self.assertRaises(KeyboardInterrupt, memio.writelines, error_gen())

    def test_truncate(self):
        buf = self.buftype("1234567890")
        memio = self.ioclass(buf)

        memio.seek(6)
        self.assertEqual(memio.truncate(), 6)
        self.assertEqual(memio.getvalue(), buf[:6])
        self.assertEqual(memio.truncate(4), 4)
        self.assertEqual(memio.getvalue(), buf[:4])
        self.assertEqual(memio.tell(), 4)
        memio.write(buf)
        self.assertEqual(memio.getvalue(), buf[:4] + buf)
        self.assertRaises(IOError, memio.truncate, -1)

    def test_close(self):
        buf = self.buftype("1234567890")
        memio = self.ioclass(buf)

        self.assertEqual(memio.closed, False)
        memio.close()
        self.assertRaises(ValueError, memio.read)
        self.assertRaises(ValueError, memio.write, buf)
        self.assertRaises(ValueError, memio.writelines, buf)
        self.assertRaises(ValueError, memio.seek, 3)
        self.assertRaises(ValueError, memio.truncate)
        self.assertRaises(ValueError, memio.getvalue)
        self.assertRaises(ValueError, memio.tell)
        self.assertEqual(memio.closed, True)

    def test_init(self):
        buf = self.buftype("1234567890")
        memio = self.ioclass(buf)

    def test_read(self):
        buf = self.buftype("1234567890")
        memio = self.ioclass(buf)

        self.assertEqual(memio.read(1), buf[:1])
        self.assertEqual(memio.read(4), buf[1:5])
        self.assertEqual(memio.read(900), buf[5:])
        self.assertEqual(memio.read(), self.EOF)
        self.assertEqual(memio.seek(0), 0)
        self.assertEqual(memio.read(), buf)
        self.assertEqual(memio.tell(), 10)

    def test_read_noargs(self):
        buf = self.buftype("1234567890")
        memio = self.ioclass(buf)

        self.assertEqual(buf, memio.read())
        self.assertEqual(self.EOF, memio.read())

    def test_readline(self):
        pass

    def test_readlines(self):
        pass

    def test_iterator(self):
        buf = self.buftype("1234567890\n")
        memio = self.ioclass(buf * 10)

        self.assertEqual(iter(memio), memio)
        self.failUnless(hasattr(memio, '__iter__'))
        self.failUnless(hasattr(memio, '__next__'))
        i = 0
        for line in memio:
            self.assertEqual(line, buf)
            i += 1
        self.assertEqual(i, 10)

        memio.seek(0)
        i = 0
        for line in memio:
            self.assertEqual(line, buf)
            i += 1
        self.assertEqual(i, 10)

    def test_getvalue(self):
        buf = self.buftype("1234567890")
        memio = self.ioclass(buf)

        self.assertEqual(memio.getvalue(), buf)
        memio = self.ioclass(buf * 1000)
        self.assertEqual(memio.getvalue()[-3:], "890")

    def test_seek(self):
        buf = self.buftype("1234567890")
        memio = self.ioclass(buf)

        memio.read(5)
        memio.seek(0)
        self.assertEqual(buf, memio.read())

        memio.seek(3)
        self.assertEqual(buf[3:], memio.read())

    def test_tell(self):
        buf = self.buftype("1234567890")
        memio = self.ioclass(buf)

        self.assertEqual(0, memio.tell())
        memio.seek(5)
        self.assertEqual(5, memio.tell())
        memio.seek(10000)
        self.assertEqual(10000, memio.tell())

    def test_flags(self):
        memio = self.ioclass()

        self.assertEqual(memio.writable(), True)
        self.assertEqual(memio.readable(), True)
        self.assertEqual(memio.seekable(), True)
        self.assertEqual(memio.isatty(), False)
        memio.close()
        self.assertEqual(memio.writable(), True)
        self.assertEqual(memio.readable(), True)
        self.assertEqual(memio.seekable(), True)


class PyBytesIOTest(MemoryTestMixin, unittest.TestCase):
    """
    Test the Python implementation of BytesIO.
    """
    buftype = bytes
    ioclass = io._BytesIO
    EOF = b""

    def test_readinto(self):
        buf = self.buftype("1234567890")
        memio = self.ioclass(buf)

        b = bytes("hello")
        self.assertEqual(memio.readinto(b), 5)
        self.assertEqual(b, b"12345")
        self.assertEqual(memio.readinto(b), 5)
        self.assertEqual(b, b"67890")
        self.assertEqual(memio.readinto(b), 0)
        self.assertEqual(b, b"67890")

        b = bytes("hello world")
        self.assertEqual(memio.seek(0), 0)
        self.assertEqual(memio.readinto(b), 10)
        self.assertEqual(b, "1234567890d")


class PyStringIOTest(MemoryTestMixin, unittest.TestCase):
    """
    Test the Python implementation of StringIO.
    """
    buftype = unicode
    ioclass = io._StringIO
    EOF = ""

if has_c_implementation:
    class CBytesIOTest(PyBytesIOTest):
        """
        Test the C implementation of BytesIO if available.
        """
        ioclass = _bytes_io.BytesIO


    class CStringIOTest(PyStringIOTest):
        """
        Test the C implementation of StringIO if available.
        """
        ioclass = _string_io.StringIO


def test_main():
    if has_c_implementation:
        test_support.run_unittest(PyBytesIOTest, PyStringIOTest,
                                  CBytesIOTest, CStringIOTest)
    else:
        test_support.run_unittest(PythonBytesIOTest, PythonStringIOTest)

if __name__ == '__main__':
    test_main()
