"""Unit tests for memory-based file-like objects.
StringIO -- for unicode strings
BytesIO -- for bytes
"""

import unittest
from test import test_support

import io

try:
    import _stringio, _bytesio
    has_c_implementation = True
except ImportError:
    has_c_implementation = False


class MemoryTestMixin:

    def write_ops(self, f, t):
        self.assertEqual(f.write(t("blah.")), 5)
        self.assertEqual(f.seek(0), 0)
        self.assertEqual(f.write(t("Hello.")), 6)
        self.assertEqual(f.tell(), 6)
        self.assertEqual(f.seek(5), 5)
        self.assertEqual(f.tell(), 5)
        self.assertEqual(f.write(t(" world\n\n\n")), 9)
        self.assertEqual(f.seek(0), 0)
        self.assertEqual(f.write(t("h")), 1)
        self.assertEqual(f.truncate(12), 12)
        self.assertEqual(f.tell(), 12)

    def test_write(self):
        buf = self.buftype("hello world\n")
        memio = self.ioclass(buf)

        self.write_ops(memio, self.buftype)
        self.assertEqual(memio.getvalue(), buf)
        memio = self.ioclass()
        self.write_ops(memio, self.buftype)
        self.assertEqual(memio.getvalue(), buf)
        memio.close()
        memio.write(buf)

    def test_writelines(self):
        buf = self.buftype("1234567890")
        memio = self.ioclass()

        memio.writelines([buf] * 100)
        self.assertEqual(memio.getvalue(), buf * 100)
        memio.close()
        memio.writelines([buf])

    def test_writelines_error(self):
        memio = self.ioclass()
        def error_gen():
            yield self.buftype('spam')
            raise KeyboardInterrupt

        self.assertRaises(KeyboardInterrupt, memio.writelines, error_gen())

    def test_truncate(self):
        buf = self.buftype("1234567890")
        memio = self.ioclass(buf)

        self.assertRaises(ValueError, memio.truncate, -1)
        memio.seek(6)
        self.assertEqual(memio.truncate(), 6)
        self.assertEqual(memio.getvalue(), buf[:6])
        self.assertEqual(memio.truncate(4), 4)
        self.assertEqual(memio.getvalue(), buf[:4])
        self.assertEqual(memio.tell(), 4)
        memio.write(buf)
        self.assertEqual(memio.getvalue(), buf[:4] + buf)
        memio.close()
        memio.truncate(0)

    def test_init(self):
        buf = self.buftype("1234567890")
        memio = self.ioclass(buf)

    def test_read(self):
        buf = self.buftype("1234567890")
        memio = self.ioclass(buf)

        self.assertEqual(memio.read(0), self.EOF)
        self.assertEqual(memio.read(1), buf[:1])
        self.assertEqual(memio.read(4), buf[1:5])
        self.assertEqual(memio.read(900), buf[5:])
        self.assertEqual(memio.read(), self.EOF)
        memio.seek(0)
        self.assertEqual(memio.read(), buf)
        self.assertEqual(memio.read(), self.EOF)
        self.assertEqual(memio.tell(), 10)
        memio.seek(0)
        self.assertEqual(memio.read(-1), buf)
        memio.close()
        memio.read()

    def test_readline(self):
        buf = self.buftype("1234567890\n")
        memio = self.ioclass(buf * 2)

        self.assertEqual(memio.readline(0), self.EOF)
        self.assertEqual(memio.readline(), buf)
        self.assertEqual(memio.readline(), buf)
        self.assertEqual(memio.readline(), self.EOF)
        memio.seek(0)
        self.assertEqual(memio.readline(5), buf[:5])
        self.assertEqual(memio.readline(5), buf[5:10])
        self.assertEqual(memio.readline(5), buf[10:15])
        memio.seek(0)
        self.assertEqual(memio.readline(-1), buf)
        memio.seek(0)
        self.assertEqual(memio.readline(0), self.EOF)
        memio.close()
        memio.readline()

    def test_readlines(self):
        buf = self.buftype("1234567890\n")
        memio = self.ioclass(buf * 10)

        self.assertEqual(memio.readlines(), [buf] * 10)
        memio.seek(5)
        self.assertEqual(memio.readlines(), [buf[5:]] + [buf] * 9)
        memio.seek(0)
        self.assertEqual(memio.readlines(15), [buf] * 2)
        memio.seek(0)
        self.assertEqual(memio.readlines(-1), [buf] * 10)
        memio.seek(0)
        self.assertEqual(memio.readlines(0), [buf] * 10)
        memio.close()
        memio.readlines()

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
        memio = self.ioclass(buf * 2)
        memio.close()
        i = 0
        for line in memio:
            i += 1
        self.assertEqual(i, 2)

    def test_getvalue(self):
        buf = self.buftype("1234567890")
        memio = self.ioclass(buf)

        self.assertEqual(memio.getvalue(), buf)
        memio.read()
        self.assertEqual(memio.getvalue(), buf)
        memio = self.ioclass(buf * 1000)
        self.assertEqual(memio.getvalue()[-3:], self.buftype("890"))
        memio = self.ioclass(buf)
        memio.close()
        self.assertEqual(memio.getvalue(), buf)

    def test_seek(self):
        buf = self.buftype("1234567890")
        memio = self.ioclass(buf)

        memio.read(5)
        self.assertRaises(ValueError, memio.seek, -1)
        self.assertRaises(ValueError, memio.seek, 1, -1)
        self.assertRaises(ValueError, memio.seek, 1, 3)
        self.assertEqual(memio.seek(0), 0)
        self.assertEqual(memio.seek(0, 0), 0)
        self.assertEqual(memio.read(), buf)
        self.assertEqual(memio.seek(3), 3)
        self.assertEqual(memio.seek(0, 1), 3)
        self.assertEqual(memio.read(), buf[3:])
        self.assertEqual(memio.seek(len(buf)), len(buf))
        self.assertEqual(memio.read(), self.EOF)
        memio.seek(len(buf) + 1)
        self.assertEqual(memio.read(), self.EOF)
        self.assertEqual(memio.seek(0, 2), len(buf))
        self.assertEqual(memio.read(), self.EOF)
        memio.close()
        memio.seek(0)

    def test_overseek(self):
        buf = self.buftype("1234567890")
        memio = self.ioclass(buf)

        self.assertEqual(memio.seek(len(buf) + 1), 11)
        self.assertEqual(memio.read(), self.EOF)
        self.assertEqual(memio.tell(), 11)
        self.assertEqual(memio.getvalue(), buf)
        memio.write(self.EOF)
        self.assertEqual(memio.getvalue(), buf)
        memio.write(buf)
        self.assertEqual(memio.getvalue(), buf + self.buftype('\0') + buf)

    def test_tell(self):
        buf = self.buftype("1234567890")
        memio = self.ioclass(buf)

        self.assertEqual(memio.tell(), 0)
        memio.seek(5)
        self.assertEqual(memio.tell(), 5)
        memio.seek(10000)
        self.assertEqual(memio.tell(), 10000)
        memio.close()
        memio.tell()

    def test_flush(self):
        buf = self.buftype("1234567890")
        memio = self.ioclass(buf)

        self.assertEqual(memio.flush(), None)
        memio.close()
        memio.flush()

    def test_flags(self):
        memio = self.ioclass()

        self.assertEqual(memio.writable(), True)
        self.assertEqual(memio.readable(), True)
        self.assertEqual(memio.seekable(), True)
        self.assertEqual(memio.isatty(), False)
        self.assertEqual(memio.closed, False)
        memio.close()
        self.assertEqual(memio.writable(), True)
        self.assertEqual(memio.readable(), True)
        self.assertEqual(memio.seekable(), True)
        self.assertEqual(memio.isatty(), False)
        self.assertEqual(memio.closed, False)

    def test_subclassing(self):
        buf = self.buftype("1234567890")
        def test1():
            class MemIO(self.ioclass):
                pass
            m = MemIO(buf)
            return m.getvalue()
        def test2():
            class MemIO(self.ioclass):
                def __init__(me, a, b):
                    self.ioclass.__init__(me, a)
            m = MemIO(buf, None)
            return m.getvalue()
        self.assertEqual(test1(), buf)
        self.assertEqual(test2(), buf)

    def test_widechar(self):
        buf = self.buftype("\U0002030a\U00020347")
        memio = self.ioclass(buf)

        self.assertEqual(memio.getvalue(), buf)
        self.assertEqual(memio.write(buf), len(buf))
        self.assertEqual(memio.getvalue(), buf)
        self.assertEqual(memio.write(buf), len(buf))
        self.assertEqual(memio.getvalue(), buf + buf)

    def test_none_arg(self):
        memio = self.ioclass(None)

        self.assertEqual(memio.read(None), self.EOF)
        self.assertEqual(memio.readline(None), self.EOF)
        self.assertEqual(memio.readlines(None), [])
        self.assertEqual(memio.truncate(None), 0)


class PyBytesIOTest(MemoryTestMixin, unittest.TestCase):
    buftype = bytes
    ioclass = io._BytesIO
    EOF = b""

    def test_read1(self):
        buf = self.buftype("1234567890")
        memio = self.ioclass(buf)

        self.assertRaises(TypeError, memio.read1)
        self.assertEqual(memio.read(), buf)

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
        memio.seek(0)
        self.assertEqual(memio.readinto(b), 10)
        self.assertEqual(b, b"1234567890d")
        b = bytes()
        memio.seek(0)
        self.assertEqual(memio.readinto(b), 0)
        self.assertEqual(b, b"")
        memio.close()
        memio.readinto(b)

    def test_relative_seek(self):
        buf = self.buftype("1234567890")
        memio = self.ioclass(buf)

        self.assertEqual(memio.seek(-1, 1), 0)
        self.assertEqual(memio.seek(3, 1), 3)
        self.assertEqual(memio.seek(-4, 1), 0)
        self.assertEqual(memio.seek(-1, 2), 9)
        self.assertEqual(memio.seek(1, 1), 10)
        self.assertEqual(memio.seek(1, 2), 11)
        memio.seek(-3, 2)
        self.assertEqual(memio.read(), buf[-3:])
        memio.seek(0)
        memio.seek(1, 1)
        self.assertEqual(memio.read(), buf[1:])

    def test_unicode(self):
        buf = "1234567890"
        memio = self.ioclass(buf)

        self.assertEqual(memio.getvalue(), self.buftype(buf))
        self.assertEqual(memio.write(buf), len(buf))
        self.assertEqual(memio.getvalue(), self.buftype(buf))
        self.assertEqual(memio.write(buf), len(buf))
        self.assertEqual(memio.getvalue(), self.buftype(buf + buf))
        self.write_ops(self.ioclass(), str)


class PyStringIOTest(MemoryTestMixin, unittest.TestCase):
    buftype = str
    ioclass = io._StringIO
    EOF = ""

    def test_str8(self):
        buf = str8("1234567890")
        memio = self.ioclass(buf)

        self.assertEqual(memio.getvalue(), self.buftype(buf))
        self.assertEqual(memio.write(buf), len(buf))
        self.assertEqual(memio.getvalue(), self.buftype(buf))
        self.assertEqual(memio.write(buf), len(buf))
        self.assertEqual(memio.getvalue(), self.buftype(buf + buf))
        self.write_ops(self.ioclass(), str8)

    def test_relative_seek(self):
        memio = self.ioclass()

        self.assertRaises(IOError, memio.seek, -1, 1)
        self.assertRaises(IOError, memio.seek, 3, 1)
        self.assertRaises(IOError, memio.seek, -3, 1)
        self.assertRaises(IOError, memio.seek, -1, 2)
        self.assertRaises(IOError, memio.seek, 1, 1)
        self.assertRaises(IOError, memio.seek, 1, 2)

if has_c_implementation:
    class CBytesIOTest(PyBytesIOTest):
        ioclass = _bytesio.BytesIO

    class CStringIOTest(PyStringIOTest):
        ioclass = _stringio.StringIO


def test_main():
    tests = [PyBytesIOTest, PyStringIOTest]
    if has_c_implementation:
        tests.extend([CBytesIOTest, CStringIOTest])
    test_support.run_unittest(*tests)

if __name__ == '__main__':
    test_main()
