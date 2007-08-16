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
        self.assertRaises(ValueError, memio.truncate, -1)

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
        memio.seek(0)
        self.assertEqual(memio.read(), buf)
        self.assertEqual(memio.read(), self.EOF)
        self.assertEqual(memio.tell(), 10)

    def test_readline(self):
        buf = self.buftype("1234567890\n")
        memio = self.ioclass(buf * 2)

        self.assertEqual(memio.readline(), buf)
        self.assertEqual(memio.readline(), buf)
        self.assertEqual(memio.readline(), self.EOF)
        memio.seek(0)
        self.assertEqual(memio.readline(5), buf[:5])
        self.assertEqual(memio.readline(5), buf[5:10])
        self.assertEqual(memio.readline(5), buf[10:15])

    def test_readlines(self):
        buf = self.buftype("1234567890\n")
        memio = self.ioclass(buf * 10)

        self.assertEqual(memio.readlines(), [buf] * 10)
        memio.seek(5)
        self.assertEqual(memio.readlines(), [buf[5:]] + [buf] * 9)
        memio.seek(0)
        self.assertEqual(memio.readlines(15), [buf] * 2)

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
        memio.read()
        self.assertEqual(memio.getvalue(), buf)
        memio = self.ioclass(buf * 1000)
        self.assertEqual(memio.getvalue()[-3:], self.buftype("890"))

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

        self.assertEqual(memio.tell(), 0)
        memio.seek(5)
        self.assertEqual(memio.tell(), 5)
        memio.seek(10000)
        self.assertEqual(memio.tell(), 10000)

    def test_flush(self):
        buf = self.buftype("1234567890")
        memio = self.ioclass(buf)

        self.assertEqual(memio.flush(), None)

    def test_flags(self):
        memio = self.ioclass()

        self.assertEqual(memio.writable(), True)
        self.assertEqual(memio.readable(), True)
        self.assertEqual(memio.seekable(), True)
        self.assertEqual(memio.isatty(), False)
        self.assertEqual(memio.closed, False)

    def test_subclassing(self):
        buf = self.buftype("1234567890")
        def test():
            class MemIO(self.ioclass):
                pass
            m = MemIO(buf)
            return m.getvalue()
        self.assertEqual(test(), buf)

    def test_widechar(self):
        buf = self.buftype("\U0002030a\U00020347")
        memio = self.ioclass(buf)

        self.assertEqual(memio.getvalue(), buf)
        self.assertEqual(memio.write(buf), len(buf))
        self.assertEqual(memio.getvalue(), buf)
        self.assertEqual(memio.write(buf), len(buf))
        self.assertEqual(memio.getvalue(), buf + buf)

    def test_close(self):
        buf = self.buftype("1234567890")
        memio = self.ioclass(buf)

        # close() does nothing.
        memio.close()
        self.assertEqual(memio.getvalue(), buf)

        # Just check if methods are still functioning.
        memio.write(buf)
        memio.truncate(len(buf))
        memio.seek(2)
        memio.flush()
        memio.tell()
        memio.read()
        memio.writelines([buf + self.buftype("\n")] * 5)
        memio.readline()
        memio.readlines()

        self.assertEqual(memio.writable(), True)
        self.assertEqual(memio.readable(), True)
        self.assertEqual(memio.seekable(), True)
        self.assertEqual(memio.isatty(), False)
        self.assertEqual(memio.closed, False)


class PyBytesIOTest(MemoryTestMixin, unittest.TestCase):
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
        memio.seek(0)
        self.assertEqual(memio.readinto(b), 10)
        self.assertEqual(b, b"1234567890d")
        b = bytes()
        memio.seek(0)
        self.assertEqual(memio.readinto(b), 0)
        self.assertEqual(b, b"")
        memio.close()
        self.assertRaises(ValueError, memio.readinto, b)

    def test_overseek(self):
        buf = self.buftype("1234567890")
        memio = self.ioclass()

        memio.seek(2)
        memio.write(buf)
        self.assertEqual(memio.getvalue(), self.buftype('\0\0') + buf)

    def test_unicode(self):
        buf = "1234567890"
        memio = self.ioclass(buf)

        self.assertEqual(memio.getvalue(), self.buftype(buf))
        self.assertEqual(memio.write(buf), len(buf))
        self.assertEqual(memio.getvalue(), self.buftype(buf))
        self.assertEqual(memio.write(buf), len(buf))
        self.assertEqual(memio.getvalue(), self.buftype(buf + buf))


class PyStringIOTest(MemoryTestMixin, unittest.TestCase):
    buftype = str
    ioclass = io._StringIO
    EOF = ""

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
