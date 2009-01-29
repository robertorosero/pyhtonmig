"""Unit tests for io.py."""

import os
import sys
import time
import array
import threading
import random
import unittest
import weakref
import gc
from itertools import chain, cycle, count
from collections import deque
from test import support

import codecs
import io  # The module under test


def _default_chunk_size():
    """Get the default TextIOWrapper chunk size"""
    with open(__file__, "r", encoding="latin1") as f:
        return f._CHUNK_SIZE


class MockRawIO(io.RawIOBase):

    def __init__(self, read_stack=()):
        self._read_stack = list(read_stack)
        self._write_stack = []
        self._reads = 0

    def read(self, n=None):
        self._reads += 1
        try:
            return self._read_stack.pop(0)
        except:
            return b""

    def write(self, b):
        self._write_stack.append(bytes(b))
        return len(b)

    def writable(self):
        return True

    def fileno(self):
        return 42

    def readable(self):
        return True

    def seekable(self):
        return True

    def seek(self, pos, whence):
        return 0   # wrong but we gotta return something

    def tell(self):
        return 0   # same comment as above

    def readinto(self, buf):
        self._reads += 1
        max_len = len(buf)
        try:
            data = self._read_stack[0]
        except IndexError:
            return 0
        if data is None:
            del self._read_stack[0]
            return None
        n = len(data)
        if len(data) <= max_len:
            del self._read_stack[0]
            buf[:n] = data
            return n
        else:
            buf[:] = data[:max_len]
            self._read_stack[0] = data[max_len:]
            return max_len

    def truncate(self, pos=None):
        return pos


class MisbehavedRawIO(MockRawIO):
    def write(self, b):
        return MockRawIO.write(self, b) * 2

    def read(self, n=None):
        return MockRawIO.read(self, n) * 2

    def seek(self, pos, whence):
        return -123

    def tell(self):
        return -456

    def readinto(self, buf):
        MockRawIO.readinto(self, buf)
        return len(buf) * 5


class CloseFailureIO(MockRawIO):

    def close(self):
        raise IOError


class MockFileIO(io.BytesIO):

    def __init__(self, data):
        self.read_history = []
        io.BytesIO.__init__(self, data)

    def read(self, n=None):
        res = io.BytesIO.read(self, n)
        self.read_history.append(None if res is None else len(res))
        return res

    def readinto(self, b):
        res = io.BytesIO.readinto(self, b)
        self.read_history.append(res)
        return res

class MockNonBlockWriterIO(io.RawIOBase):

    def __init__(self):
        self._write_stack = []
        self._blocker_char = None

    def pop_written(self):
        s = b"".join(self._write_stack)
        self._write_stack[:] = []
        return s

    def block_on(self, char):
        """Block when a given char is encountered."""
        self._blocker_char = char

    def readable(self):
        return True

    def seekable(self):
        return True

    def writable(self):
        return True

    def write(self, b):
        b = bytes(b)
        n = -1
        if self._blocker_char:
            try:
                n = b.index(self._blocker_char)
            except ValueError:
                pass
            else:
                self._blocker_char = None
                self._write_stack.append(b[:n])
                raise io.BlockingIOError(0, "test blocking", n)
        self._write_stack.append(b)
        return len(b)


class IOTest(unittest.TestCase):

    def setUp(self):
        support.unlink(support.TESTFN)

    def tearDown(self):
        support.unlink(support.TESTFN)

    def write_ops(self, f):
        self.assertEqual(f.write(b"blah."), 5)
        self.assertEqual(f.seek(0), 0)
        self.assertEqual(f.write(b"Hello."), 6)
        self.assertEqual(f.tell(), 6)
        self.assertEqual(f.seek(-1, 1), 5)
        self.assertEqual(f.tell(), 5)
        self.assertEqual(f.write(bytearray(b" world\n\n\n")), 9)
        self.assertEqual(f.seek(0), 0)
        self.assertEqual(f.write(b"h"), 1)
        self.assertEqual(f.seek(-1, 2), 13)
        self.assertEqual(f.tell(), 13)
        self.assertEqual(f.truncate(12), 12)
        self.assertEqual(f.tell(), 12)
        self.assertRaises(TypeError, f.seek, 0.0)

    def read_ops(self, f, buffered=False):
        data = f.read(5)
        self.assertEqual(data, b"hello")
        data = bytearray(data)
        self.assertEqual(f.readinto(data), 5)
        self.assertEqual(data, b" worl")
        self.assertEqual(f.readinto(data), 2)
        self.assertEqual(len(data), 5)
        self.assertEqual(data[:2], b"d\n")
        self.assertEqual(f.seek(0), 0)
        self.assertEqual(f.read(20), b"hello world\n")
        self.assertEqual(f.read(1), b"")
        self.assertEqual(f.readinto(bytearray(b"x")), 0)
        self.assertEqual(f.seek(-6, 2), 6)
        self.assertEqual(f.read(5), b"world")
        self.assertEqual(f.read(0), b"")
        self.assertEqual(f.readinto(bytearray()), 0)
        self.assertEqual(f.seek(-6, 1), 5)
        self.assertEqual(f.read(5), b" worl")
        self.assertEqual(f.tell(), 10)
        self.assertRaises(TypeError, f.seek, 0.0)
        if buffered:
            f.seek(0)
            self.assertEqual(f.read(), b"hello world\n")
            f.seek(6)
            self.assertEqual(f.read(), b"world\n")
            self.assertEqual(f.read(), b"")

    LARGE = 2**31

    def large_file_ops(self, f):
        assert f.readable()
        assert f.writable()
        self.assertEqual(f.seek(self.LARGE), self.LARGE)
        self.assertEqual(f.tell(), self.LARGE)
        self.assertEqual(f.write(b"xxx"), 3)
        self.assertEqual(f.tell(), self.LARGE + 3)
        self.assertEqual(f.seek(-1, 1), self.LARGE + 2)
        self.assertEqual(f.truncate(), self.LARGE + 2)
        self.assertEqual(f.tell(), self.LARGE + 2)
        self.assertEqual(f.seek(0, 2), self.LARGE + 2)
        self.assertEqual(f.truncate(self.LARGE + 1), self.LARGE + 1)
        self.assertEqual(f.tell(), self.LARGE + 1)
        self.assertEqual(f.seek(0, 2), self.LARGE + 1)
        self.assertEqual(f.seek(-1, 2), self.LARGE)
        self.assertEqual(f.read(2), b"x")

    def test_raw_file_io(self):
        f = io.open(support.TESTFN, "wb", buffering=0)
        self.assertEqual(f.readable(), False)
        self.assertEqual(f.writable(), True)
        self.assertEqual(f.seekable(), True)
        self.write_ops(f)
        f.close()
        f = io.open(support.TESTFN, "rb", buffering=0)
        self.assertEqual(f.readable(), True)
        self.assertEqual(f.writable(), False)
        self.assertEqual(f.seekable(), True)
        self.read_ops(f)
        f.close()

    def test_buffered_file_io(self):
        f = io.open(support.TESTFN, "wb")
        self.assertEqual(f.readable(), False)
        self.assertEqual(f.writable(), True)
        self.assertEqual(f.seekable(), True)
        self.write_ops(f)
        f.close()
        f = io.open(support.TESTFN, "rb")
        self.assertEqual(f.readable(), True)
        self.assertEqual(f.writable(), False)
        self.assertEqual(f.seekable(), True)
        self.read_ops(f, True)
        f.close()

    def test_readline(self):
        f = io.open(support.TESTFN, "wb")
        f.write(b"abc\ndef\nxyzzy\nfoo\x00bar\nanother line")
        f.close()
        f = io.open(support.TESTFN, "rb")
        self.assertEqual(f.readline(), b"abc\n")
        self.assertEqual(f.readline(10), b"def\n")
        self.assertEqual(f.readline(2), b"xy")
        self.assertEqual(f.readline(4), b"zzy\n")
        self.assertEqual(f.readline(), b"foo\x00bar\n")
        self.assertEqual(f.readline(), b"another line")
        f.close()

    def test_raw_bytes_io(self):
        f = io.BytesIO()
        self.write_ops(f)
        data = f.getvalue()
        self.assertEqual(data, b"hello world\n")
        f = io.BytesIO(data)
        self.read_ops(f, True)

    def test_large_file_ops(self):
        # On Windows and Mac OSX this test comsumes large resources; It takes
        # a long time to build the >2GB file and takes >2GB of disk space
        # therefore the resource must be enabled to run this test.
        if sys.platform[:3] == 'win' or sys.platform == 'darwin':
            if not support.is_resource_enabled("largefile"):
                print("\nTesting large file ops skipped on %s." % sys.platform,
                      file=sys.stderr)
                print("It requires %d bytes and a long time." % self.LARGE,
                      file=sys.stderr)
                print("Use 'regrtest.py -u largefile test_io' to run it.",
                      file=sys.stderr)
                return
        f = io.open(support.TESTFN, "w+b", 0)
        self.large_file_ops(f)
        f.close()
        f = io.open(support.TESTFN, "w+b")
        self.large_file_ops(f)
        f.close()

    def test_with_open(self):
        for bufsize in (0, 1, 100):
            f = None
            with open(support.TESTFN, "wb", bufsize) as f:
                f.write(b"xxx")
            self.assertEqual(f.closed, True)
            f = None
            try:
                with open(support.TESTFN, "wb", bufsize) as f:
                    1/0
            except ZeroDivisionError:
                self.assertEqual(f.closed, True)
            else:
                self.fail("1/0 didn't raise an exception")

    # issue 5008
    def test_append_mode_tell(self):
        with io.open(support.TESTFN, "wb") as f:
            f.write(b"xxx")
        with io.open(support.TESTFN, "ab", buffering=0) as f:
            self.assertEqual(f.tell(), 3)
        with io.open(support.TESTFN, "ab") as f:
            self.assertEqual(f.tell(), 3)
        with io.open(support.TESTFN, "a") as f:
            self.assert_(f.tell() > 0)

    def test_destructor(self):
        record = []
        class MyFileIO(io.FileIO):
            def __del__(self):
                record.append(1)
                try:
                    f = io.FileIO.__del__
                except AttributeError:
                    pass
                else:
                    f(self)
            def close(self):
                record.append(2)
                io.FileIO.close(self)
            def flush(self):
                record.append(3)
                io.FileIO.flush(self)
        f = MyFileIO(support.TESTFN, "wb")
        f.write(b"xxx")
        del f
        self.assertEqual(record, [1, 2, 3])

    def test_IOBase_destructor(self):
        record = []
        class MyIO(io.IOBase):
            def __init__(self):
                pass
            def __del__(self):
                record.append(1)
                try:
                    f = io.IOBase.__del__
                except AttributeError:
                    pass
                else:
                    f(self)
            def close(self):
                record.append(2)
                io.IOBase.close(self)
            def flush(self):
                record.append(3)
                io.IOBase.flush(self)
        f = MyIO()
        del f
        self.assertEqual(record, [1, 2, 3])

    def test_close_flushes(self):
        f = io.open(support.TESTFN, "wb")
        f.write(b"xxx")
        f.close()
        f = io.open(support.TESTFN, "rb")
        self.assertEqual(f.read(), b"xxx")
        f.close()

    def test_array_writes(self):
        a = array.array('i', range(10))
        n = len(a.tostring())
        f = io.open(support.TESTFN, "wb", 0)
        self.assertEqual(f.write(a), n)
        f.close()
        f = io.open(support.TESTFN, "wb")
        self.assertEqual(f.write(a), n)
        f.close()

    def test_closefd(self):
        self.assertRaises(ValueError, io.open, support.TESTFN, 'w',
                          closefd=False)

    def testReadClosed(self):
        with io.open(support.TESTFN, "w") as f:
            f.write("egg\n")
        with io.open(support.TESTFN, "r") as f:
            file = io.open(f.fileno(), "r", closefd=False)
            self.assertEqual(file.read(), "egg\n")
            file.seek(0)
            file.close()
            self.assertRaises(ValueError, file.read)

    def test_no_closefd_with_filename(self):
        # can't use closefd in combination with a file name
        self.assertRaises(ValueError, io.open, support.TESTFN, "r", closefd=False)

    def test_closefd_attr(self):
        with io.open(support.TESTFN, "wb") as f:
            f.write(b"egg\n")
        with io.open(support.TESTFN, "r") as f:
            self.assertEqual(f.buffer.raw.closefd, True)
            file = io.open(f.fileno(), "r", closefd=False)
            self.assertEqual(file.buffer.raw.closefd, False)

    def test_garbage_collection(self):
        # FileIO objects are collected, and collecting them flushes
        # all data to disk.
        f = io.FileIO(support.TESTFN, "wb")
        f.write(b"abcxxx")
        f.f = f
        wr = weakref.ref(f)
        del f
        gc.collect()
        self.assert_(wr() is None, wr)
        with open(support.TESTFN, "rb") as f:
            self.assertEqual(f.read(), b"abcxxx")


class MemorySeekTestMixin:

    def testInit(self):
        buf = self.buftype("1234567890")
        bytesIo = self.ioclass(buf)

    def testRead(self):
        buf = self.buftype("1234567890")
        bytesIo = self.ioclass(buf)

        self.assertEquals(buf[:1], bytesIo.read(1))
        self.assertEquals(buf[1:5], bytesIo.read(4))
        self.assertEquals(buf[5:], bytesIo.read(900))
        self.assertEquals(self.EOF, bytesIo.read())

    def testReadNoArgs(self):
        buf = self.buftype("1234567890")
        bytesIo = self.ioclass(buf)

        self.assertEquals(buf, bytesIo.read())
        self.assertEquals(self.EOF, bytesIo.read())

    def testSeek(self):
        buf = self.buftype("1234567890")
        bytesIo = self.ioclass(buf)

        bytesIo.read(5)
        bytesIo.seek(0)
        self.assertEquals(buf, bytesIo.read())

        bytesIo.seek(3)
        self.assertEquals(buf[3:], bytesIo.read())
        self.assertRaises(TypeError, bytesIo.seek, 0.0)

    def testTell(self):
        buf = self.buftype("1234567890")
        bytesIo = self.ioclass(buf)

        self.assertEquals(0, bytesIo.tell())
        bytesIo.seek(5)
        self.assertEquals(5, bytesIo.tell())
        bytesIo.seek(10000)
        self.assertEquals(10000, bytesIo.tell())


class BytesIOTest(MemorySeekTestMixin, unittest.TestCase):
    @staticmethod
    def buftype(s):
        return s.encode("utf-8")
    ioclass = io.BytesIO
    EOF = b""


class StringIOTest(MemorySeekTestMixin, unittest.TestCase):
    buftype = str
    ioclass = io.StringIO
    EOF = ""


class CommonBufferedTests:
    # Tests common to BufferedReader, BufferedWriter and BufferedRandom

    def testFileno(self):
        rawio = MockRawIO()
        bufio = self.tp(rawio)

        self.assertEquals(42, bufio.fileno())

    def testFilenoNoFileno(self):
        # XXX will we always have fileno() function? If so, kill
        # this test. Else, write it.
        pass

    def testInvalidArgs(self):
        rawio = MockRawIO()
        bufio = self.tp(rawio)
        # Invalid whence
        self.assertRaises(ValueError, bufio.seek, 0, -1)
        self.assertRaises(ValueError, bufio.seek, 0, 3)

    def testOverrideDestructor(self):
        tp = self.tp
        record = []
        class MyBufferedIO(tp):
            def __del__(self):
                record.append(1)
                try:
                    f = tp.__del__
                except AttributeError:
                    pass
                else:
                    f(self)
            def close(self):
                record.append(2)
                tp.close(self)
            def flush(self):
                record.append(3)
                tp.flush(self)
        rawio = MockRawIO()
        bufio = MyBufferedIO(rawio)
        writable = bufio.writable()
        del bufio
        if writable:
            self.assertEqual(record, [1, 2, 3])
        else:
            self.assertEqual(record, [1, 2])

    def testContext(self):
        # Test usability as a context manager
        rawio = MockRawIO()
        bufio = self.tp(rawio)
        def _with():
            with bufio:
                pass
        _with()
        # bufio should now be closed, and using it a second time should raise
        # a ValueError.
        self.assertRaises(ValueError, _with)

    def testErrorThroughDestructor(self):
        # Test that the exception state is not modified by a destructor,
        # even if close() fails.
        rawio = CloseFailureIO()
        def f():
            self.tp(rawio).xyzzy
        self.assertRaises(AttributeError, f)


class BufferedReaderTest(unittest.TestCase, CommonBufferedTests):
    tp = io.BufferedReader
    read_mode = "rb"

    def testConstructor(self):
        rawio = MockRawIO([b"abc"])
        bufio = self.tp(rawio)
        bufio.__init__(rawio)
        bufio.__init__(rawio, buffer_size=1024)
        bufio.__init__(rawio, buffer_size=16)
        self.assertEquals(b"abc", bufio.read())
        self.assertRaises(ValueError, bufio.__init__, rawio, buffer_size=0)
        self.assertRaises(ValueError, bufio.read)
        self.assertRaises(ValueError, bufio.__init__, rawio, buffer_size=-16)
        self.assertRaises(ValueError, bufio.read)
        self.assertRaises(ValueError, bufio.__init__, rawio, buffer_size=-1)
        self.assertRaises(ValueError, bufio.read)
        if sys.maxsize > 0x7FFFFFFF:
            # The allocation can succeed on 32-bit builds, e.g. with more
            # than 2GB RAM and a 64-bit kernel.
            self.assertRaises((OverflowError, MemoryError, ValueError),
                bufio.__init__, rawio, sys.maxsize)
        rawio = MockRawIO([b"abc"])
        bufio.__init__(rawio)
        self.assertEquals(b"abc", bufio.read())

    def testRead(self):
        rawio = MockRawIO((b"abc", b"d", b"efg"))
        bufio = self.tp(rawio)
        self.assertEquals(b"abcdef", bufio.read(6))
        # Invalid args
        self.assertRaises(ValueError, bufio.read, -2)

    def testRead1(self):
        rawio = MockRawIO((b"abc", b"d", b"efg"))
        bufio = self.tp(rawio)
        self.assertEquals(b"a", bufio.read(1))
        self.assertEquals(b"b", bufio.read1(1))
        self.assertEquals(rawio._reads, 1)
        self.assertEquals(b"c", bufio.read1(100))
        self.assertEquals(rawio._reads, 1)
        self.assertEquals(b"d", bufio.read1(100))
        self.assertEquals(rawio._reads, 2)
        self.assertEquals(b"efg", bufio.read1(100))
        self.assertEquals(rawio._reads, 3)
        self.assertEquals(b"", bufio.read1(100))
        # Invalid args
        self.assertRaises(ValueError, bufio.read1, -1)

    def testReadinto(self):
        rawio = MockRawIO((b"abc", b"d", b"efg"))
        bufio = self.tp(rawio)
        b = bytearray(2)
        self.assertEquals(bufio.readinto(b), 2)
        self.assertEquals(b, b"ab")
        self.assertEquals(bufio.readinto(b), 2)
        self.assertEquals(b, b"cd")
        self.assertEquals(bufio.readinto(b), 2)
        self.assertEquals(b, b"ef")
        self.assertEquals(bufio.readinto(b), 1)
        self.assertEquals(b, b"gf")
        self.assertEquals(bufio.readinto(b), 0)
        self.assertEquals(b, b"gf")

    def testBuffering(self):
        data = b"abcdefghi"
        dlen = len(data)

        tests = [
            [ 100, [ 3, 1, 4, 8 ], [ dlen, 0 ] ],
            [ 100, [ 3, 3, 3],     [ dlen ]    ],
            [   4, [ 1, 2, 4, 2 ], [ 4, 4, 1 ] ],
        ]

        for bufsize, buf_read_sizes, raw_read_sizes in tests:
            rawio = MockFileIO(data)
            bufio = self.tp(rawio, buffer_size=bufsize)
            pos = 0
            for nbytes in buf_read_sizes:
                self.assertEquals(bufio.read(nbytes), data[pos:pos+nbytes])
                pos += nbytes
            # this is mildly implementation-dependent
            self.assertEquals(rawio.read_history, raw_read_sizes)

    def testReadNonBlocking(self):
        # Inject some None's in there to simulate EWOULDBLOCK
        rawio = MockRawIO((b"abc", b"d", None, b"efg", None, None, None))
        bufio = self.tp(rawio)

        self.assertEquals(b"abcd", bufio.read(6))
        self.assertEquals(b"e", bufio.read(1))
        self.assertEquals(b"fg", bufio.read())
        self.assertEquals(b"", bufio.peek(1))
        self.assert_(None is bufio.read())
        self.assertEquals(b"", bufio.read())

    def testReadToEof(self):
        rawio = MockRawIO((b"abc", b"d", b"efg"))
        bufio = self.tp(rawio)

        self.assertEquals(b"abcdefg", bufio.read(9000))

    def testReadNoArgs(self):
        rawio = MockRawIO((b"abc", b"d", b"efg"))
        bufio = self.tp(rawio)

        self.assertEquals(b"abcdefg", bufio.read())

    def testThreads(self):
        try:
            # Write out many bytes with exactly the same number of 0's,
            # 1's... 255's. This will help us check that concurrent reading
            # doesn't duplicate or forget contents.
            N = 1000
            l = list(range(256)) * N
            random.shuffle(l)
            s = bytes(bytearray(l))
            with io.open(support.TESTFN, "wb") as f:
                f.write(s)
            with io.open(support.TESTFN, self.read_mode, buffering=0) as raw:
                bufio = self.tp(raw, 8)
                errors = []
                results = []
                def f():
                    try:
                        # Intra-buffer read then buffer-flushing read
                        for n in cycle([1, 19]):
                            s = bufio.read(n)
                            if not s:
                                break
                            # list.append() is atomic
                            results.append(s)
                    except Exception as e:
                        errors.append(e)
                        raise
                threads = [threading.Thread(target=f) for x in range(20)]
                for t in threads:
                    t.start()
                time.sleep(0.02) # yield
                for t in threads:
                    t.join()
                self.assertFalse(errors,
                    "the following exceptions were caught: %r" % errors)
                s = b''.join(results)
                for i in range(256):
                    c = bytes(bytearray([i]))
                    self.assertEqual(s.count(c), N)
        finally:
            support.unlink(support.TESTFN)

    def testMisbehavedRawIO(self):
        rawio = MisbehavedRawIO((b"abc", b"d", b"efg"))
        bufio = self.tp(rawio)
        self.assertRaises(IOError, bufio.seek, 0)
        self.assertRaises(IOError, bufio.tell)
        self.assertRaises(IOError, bufio.read, 10)

    def test_garbage_collection(self):
        # BufferedReader objects are collected
        rawio = io.FileIO(support.TESTFN, "w+b")
        f = self.tp(rawio)
        f.f = f
        wr = weakref.ref(f)
        del f
        gc.collect()
        self.assert_(wr() is None, wr)


class BufferedWriterTest(unittest.TestCase, CommonBufferedTests):
    tp = io.BufferedWriter
    write_mode = "wb"

    def testConstructor(self):
        rawio = MockRawIO()
        bufio = self.tp(rawio)
        bufio.__init__(rawio)
        bufio.__init__(rawio, buffer_size=1024)
        bufio.__init__(rawio, buffer_size=16)
        self.assertEquals(3, bufio.write(b"abc"))
        bufio.flush()
        self.assertRaises(ValueError, bufio.__init__, rawio, buffer_size=0)
        self.assertRaises(ValueError, bufio.write, b"def")
        self.assertRaises(ValueError, bufio.__init__, rawio, buffer_size=-16)
        self.assertRaises(ValueError, bufio.write, b"def")
        self.assertRaises(ValueError, bufio.__init__, rawio, buffer_size=-1)
        self.assertRaises(ValueError, bufio.write, b"def")
        if sys.maxsize > 0x7FFFFFFF:
            # The allocation can succeed on 32-bit builds, e.g. with more
            # than 2GB RAM and a 64-bit kernel.
            self.assertRaises((OverflowError, MemoryError, ValueError),
                bufio.__init__, rawio, sys.maxsize)
        bufio.__init__(rawio)
        self.assertEquals(3, bufio.write(b"ghi"))
        bufio.flush()
        self.assertEquals(b"".join(rawio._write_stack), b"abcghi")

    def testWrite(self):
        # Write to the buffered IO but don't overflow the buffer.
        writer = MockRawIO()
        bufio = self.tp(writer, 8)
        bufio.write(b"abc")
        self.assertFalse(writer._write_stack)

    def testWriteOverflow(self):
        writer = MockRawIO()
        bufio = self.tp(writer, 8)
        contents = b"abcdefghijklmnop"
        for n in range(0, len(contents), 3):
            bufio.write(contents[n:n+3])
        flushed = b"".join(writer._write_stack)
        # At least (total - 8) bytes were implicitly flushed, perhaps more
        # depending on the implementation.
        self.assert_(flushed.startswith(contents[:-8]), flushed)

    def check_writes(self, intermediate_func):
        # Lots of writes, test the flushed output is as expected.
        contents = bytes(range(256)) * 1000
        n = 0
        writer = MockRawIO()
        bufio = self.tp(writer, 13)
        # Generator of write sizes: repeat each N 15 times then proceed to N+1
        def gen_sizes():
            for size in count(1):
                for i in range(15):
                    yield size
        sizes = gen_sizes()
        while n < len(contents):
            size = min(next(sizes), len(contents) - n)
            self.assertEquals(bufio.write(contents[n:n+size]), size)
            intermediate_func(bufio)
            n += size
        bufio.flush()
        self.assertEquals(contents, b"".join(writer._write_stack))

    def testWrites(self):
        self.check_writes(lambda bufio: None)

    def testWritesAndFlushes(self):
        self.check_writes(lambda bufio: bufio.flush())

    def testWritesAndSeeks(self):
        def _seekabs(bufio):
            pos = bufio.tell()
            bufio.seek(pos + 1, 0)
            bufio.seek(pos - 1, 0)
            bufio.seek(pos, 0)
        self.check_writes(_seekabs)
        def _seekrel(bufio):
            pos = bufio.seek(0, 1)
            bufio.seek(+1, 1)
            bufio.seek(-1, 1)
            bufio.seek(pos, 0)
        self.check_writes(_seekrel)

    def testWritesAndTruncates(self):
        self.check_writes(lambda bufio: bufio.truncate(bufio.tell()))

    def testWriteNonBlocking(self):
        raw = MockNonBlockWriterIO()
        bufio = self.tp(raw, 8, 8)

        self.assertEquals(bufio.write(b"abcd"), 4)
        self.assertEquals(bufio.write(b"efghi"), 5)
        # 1 byte will be written, the rest will be buffered
        raw.block_on(b"k")
        self.assertEquals(bufio.write(b"jklmn"), 5)

        # 8 bytes will be written, 8 will be buffered and the rest will be lost
        raw.block_on(b"0")
        try:
            bufio.write(b"opqrwxyz0123456789")
        except io.BlockingIOError as e:
            written = e.characters_written
        else:
            self.fail("BlockingIOError should have been raised")
        self.assertEquals(written, 16)
        self.assertEquals(raw.pop_written(),
            b"abcdefghijklmnopqrwxyz")

        self.assertEquals(bufio.write(b"ABCDEFGHI"), 9)
        s = raw.pop_written()
        # Previously buffered bytes were flushed
        self.assertTrue(s.startswith(b"01234567A"), s)

    def testWriteAndRewind(self):
        raw = io.BytesIO()
        bufio = self.tp(raw, 4)
        self.assertEqual(bufio.write(b"abcdef"), 6)
        self.assertEqual(bufio.tell(), 6)
        bufio.seek(0, 0)
        self.assertEqual(bufio.write(b"XY"), 2)
        bufio.seek(6, 0)
        self.assertEqual(raw.getvalue(), b"XYcdef")
        self.assertEqual(bufio.write(b"123456"), 6)
        bufio.flush()
        self.assertEqual(raw.getvalue(), b"XYcdef123456")

    def testFlush(self):
        writer = MockRawIO()
        bufio = self.tp(writer, 8)
        bufio.write(b"abc")
        bufio.flush()
        self.assertEquals(b"abc", writer._write_stack[0])

    def testDestructor(self):
        writer = MockRawIO()
        bufio = self.tp(writer, 8)
        bufio.write(b"abc")
        del bufio
        self.assertEquals(b"abc", writer._write_stack[0])

    def testTruncate(self):
        # Truncate implicitly flushes the buffer.
        with io.open(support.TESTFN, self.write_mode, buffering=0) as raw:
            bufio = self.tp(raw, 8)
            bufio.write(b"abcdef")
            self.assertEqual(bufio.truncate(3), 3)
            self.assertEqual(bufio.tell(), 3)
        with io.open(support.TESTFN, "rb", buffering=0) as f:
            self.assertEqual(f.read(), b"abc")

    def testThreads(self):
        try:
            # Write out many bytes from many threads and test they were
            # all flushed.
            N = 1000
            contents = bytes(range(256)) * N
            sizes = cycle([1, 19])
            n = 0
            queue = deque()
            while n < len(contents):
                size = next(sizes)
                queue.append(contents[n:n+size])
                n += size
            del contents
            # We use a real file object because it allows us to
            # exercise situations where the GIL is released before
            # writing the buffer to the raw streams. This is in addition
            # to concurrency issues due to switching threads in the middle
            # of Python code.
            with io.open(support.TESTFN, self.write_mode, buffering=0) as raw:
                bufio = self.tp(raw, 8)
                errors = []
                def f():
                    try:
                        while True:
                            try:
                                s = queue.popleft()
                            except IndexError:
                                return
                            bufio.write(s)
                    except Exception as e:
                        errors.append(e)
                        raise
                threads = [threading.Thread(target=f) for x in range(20)]
                for t in threads:
                    t.start()
                time.sleep(0.02) # yield
                for t in threads:
                    t.join()
                self.assertFalse(errors,
                    "the following exceptions were caught: %r" % errors)
                bufio.close()
            with io.open(support.TESTFN, "rb") as f:
                s = f.read()
            for i in range(256):
                self.assertEquals(s.count(bytes([i])), N)
        finally:
            support.unlink(support.TESTFN)

    def testMisbehavedRawIO(self):
        rawio = MisbehavedRawIO()
        bufio = self.tp(rawio, 5)
        self.assertRaises(IOError, bufio.seek, 0)
        self.assertRaises(IOError, bufio.tell)
        self.assertRaises(IOError, bufio.write, b"abcdef")

    def test_garbage_collection(self):
        # BufferedWriter objects are collected, and collecting them flushes
        # all data to disk.
        rawio = io.FileIO(support.TESTFN, "w+b")
        f = self.tp(rawio)
        f.write(b"123xxx")
        f.x = f
        wr = weakref.ref(f)
        del f
        gc.collect()
        self.assert_(wr() is None, wr)
        with open(support.TESTFN, "rb") as f:
            self.assertEqual(f.read(), b"123xxx")


class BufferedRWPairTest(unittest.TestCase):

    def testRWPair(self):
        r = MockRawIO(())
        w = MockRawIO()
        pair = io.BufferedRWPair(r, w)
        self.assertFalse(pair.closed)

        # XXX More Tests


class BufferedRandomTest(BufferedReaderTest, BufferedWriterTest):
    tp = io.BufferedRandom
    read_mode = "rb+"
    write_mode = "wb+"

    def testConstructor(self):
        BufferedReaderTest.testConstructor(self)
        BufferedWriterTest.testConstructor(self)

    def testReadAndWrite(self):
        raw = MockRawIO((b"asdf", b"ghjk"))
        rw = self.tp(raw, 8, 12)

        self.assertEqual(b"as", rw.read(2))
        rw.write(b"ddd")
        rw.write(b"eee")
        self.assertFalse(raw._write_stack) # Buffer writes
        self.assertEqual(b"ghjk", rw.read())
        self.assertEquals(b"dddeee", raw._write_stack[0])

    def testSeekAndTell(self):
        raw = io.BytesIO(b"asdfghjkl")
        rw = self.tp(raw)

        self.assertEquals(b"as", rw.read(2))
        self.assertEquals(2, rw.tell())
        rw.seek(0, 0)
        self.assertEquals(b"asdf", rw.read(4))

        rw.write(b"asdf")
        rw.seek(0, 0)
        self.assertEquals(b"asdfasdfl", rw.read())
        self.assertEquals(9, rw.tell())
        rw.seek(-4, 2)
        self.assertEquals(5, rw.tell())
        rw.seek(2, 1)
        self.assertEquals(7, rw.tell())
        self.assertEquals(b"fl", rw.read(11))
        self.assertRaises(TypeError, rw.seek, 0.0)

    def check_flush_and_read(self, read_func):
        raw = io.BytesIO(b"abcdefghi")
        bufio = self.tp(raw)

        self.assertEquals(b"ab", read_func(bufio, 2))
        bufio.write(b"12")
        self.assertEquals(b"ef", read_func(bufio, 2))
        self.assertEquals(6, bufio.tell())
        bufio.flush()
        self.assertEquals(6, bufio.tell())
        self.assertEquals(b"ghi", read_func(bufio))
        raw.seek(0, 0)
        raw.write(b"XYZ")
        # flush() resets the read buffer
        bufio.flush()
        bufio.seek(0, 0)
        self.assertEquals(b"XYZ", read_func(bufio, 3))

    def testFlushAndRead(self):
        self.check_flush_and_read(lambda bufio, *args: bufio.read(*args))

    def testFlushAndReadinto(self):
        def _readinto(bufio, n=-1):
            b = bytearray(n if n >= 0 else 9999)
            n = bufio.readinto(b)
            return bytes(b[:n])
        self.check_flush_and_read(_readinto)

    def testFlushAndPeek(self):
        def _peek(bufio, n=-1):
            # This relies on the fact that the buffer can contain the whole
            # raw stream, otherwise peek() can return less.
            b = bufio.peek(n)
            if n != -1:
                b = b[:n]
            bufio.seek(len(b), 1)
            return b
        self.check_flush_and_read(_peek)

    def testFlushAndWrite(self):
        raw = io.BytesIO(b"abcdefghi")
        bufio = self.tp(raw)

        bufio.write(b"123")
        bufio.flush()
        bufio.write(b"45")
        bufio.flush()
        bufio.seek(0, 0)
        self.assertEquals(b"12345fghi", raw.getvalue())
        self.assertEquals(b"12345fghi", bufio.read())

    def testThreads(self):
        BufferedReaderTest.testThreads(self)
        BufferedWriterTest.testThreads(self)

    def testWritesAndPeeks(self):
        def _peek(bufio):
            bufio.peek(1)
        self.check_writes(_peek)
        def _peek(bufio):
            pos = bufio.tell()
            bufio.seek(-1, 1)
            bufio.peek(1)
            bufio.seek(pos, 0)
        self.check_writes(_peek)

    def testWritesAndReads(self):
        def _read(bufio):
            bufio.seek(-1, 1)
            bufio.read(1)
        self.check_writes(_read)

    def testWritesAndRead1s(self):
        def _read1(bufio):
            bufio.seek(-1, 1)
            bufio.read1(1)
        self.check_writes(_read1)

    def testWritesAndReadintos(self):
        def _read(bufio):
            bufio.seek(-1, 1)
            bufio.readinto(bytearray(1))
        self.check_writes(_read)

    def testMisbehavedRawIO(self):
        BufferedReaderTest.testMisbehavedRawIO(self)
        BufferedWriterTest.testMisbehavedRawIO(self)

    def test_garbage_collection(self):
        BufferedReaderTest.test_garbage_collection(self)
        BufferedWriterTest.test_garbage_collection(self)


# To fully exercise seek/tell, the StatefulIncrementalDecoder has these
# properties:
#   - A single output character can correspond to many bytes of input.
#   - The number of input bytes to complete the character can be
#     undetermined until the last input byte is received.
#   - The number of input bytes can vary depending on previous input.
#   - A single input byte can correspond to many characters of output.
#   - The number of output characters can be undetermined until the
#     last input byte is received.
#   - The number of output characters can vary depending on previous input.

class StatefulIncrementalDecoder(codecs.IncrementalDecoder):
    """
    For testing seek/tell behavior with a stateful, buffering decoder.

    Input is a sequence of words.  Words may be fixed-length (length set
    by input) or variable-length (period-terminated).  In variable-length
    mode, extra periods are ignored.  Possible words are:
      - 'i' followed by a number sets the input length, I (maximum 99).
        When I is set to 0, words are space-terminated.
      - 'o' followed by a number sets the output length, O (maximum 99).
      - Any other word is converted into a word followed by a period on
        the output.  The output word consists of the input word truncated
        or padded out with hyphens to make its length equal to O.  If O
        is 0, the word is output verbatim without truncating or padding.
    I and O are initially set to 1.  When I changes, any buffered input is
    re-scanned according to the new I.  EOF also terminates the last word.
    """

    def __init__(self, errors='strict'):
        codecs.IncrementalDecoder.__init__(self, errors)
        self.reset()

    def __repr__(self):
        return '<SID %x>' % id(self)

    def reset(self):
        self.i = 1
        self.o = 1
        self.buffer = bytearray()

    def getstate(self):
        i, o = self.i ^ 1, self.o ^ 1 # so that flags = 0 after reset()
        return bytes(self.buffer), i*100 + o

    def setstate(self, state):
        buffer, io = state
        self.buffer = bytearray(buffer)
        i, o = divmod(io, 100)
        self.i, self.o = i ^ 1, o ^ 1

    def decode(self, input, final=False):
        output = ''
        for b in input:
            if self.i == 0: # variable-length, terminated with period
                if b == ord('.'):
                    if self.buffer:
                        output += self.process_word()
                else:
                    self.buffer.append(b)
            else: # fixed-length, terminate after self.i bytes
                self.buffer.append(b)
                if len(self.buffer) == self.i:
                    output += self.process_word()
        if final and self.buffer: # EOF terminates the last word
            output += self.process_word()
        return output

    def process_word(self):
        output = ''
        if self.buffer[0] == ord('i'):
            self.i = min(99, int(self.buffer[1:] or 0)) # set input length
        elif self.buffer[0] == ord('o'):
            self.o = min(99, int(self.buffer[1:] or 0)) # set output length
        else:
            output = self.buffer.decode('ascii')
            if len(output) < self.o:
                output += '-'*self.o # pad out with hyphens
            if self.o:
                output = output[:self.o] # truncate to output length
            output += '.'
        self.buffer = bytearray()
        return output

    codecEnabled = False

    @classmethod
    def lookupTestDecoder(cls, name):
        if cls.codecEnabled and name == 'test_decoder':
            latin1 = codecs.lookup('latin-1')
            return codecs.CodecInfo(
                name='test_decoder', encode=latin1.encode, decode=None,
                incrementalencoder=None,
                streamreader=None, streamwriter=None,
                incrementaldecoder=cls)

# Register the previous decoder for testing.
# Disabled by default, tests will enable it.
codecs.register(StatefulIncrementalDecoder.lookupTestDecoder)


class StatefulIncrementalDecoderTest(unittest.TestCase):
    """
    Make sure the StatefulIncrementalDecoder actually works.
    """

    test_cases = [
        # I=1, O=1 (fixed-length input == fixed-length output)
        (b'abcd', False, 'a.b.c.d.'),
        # I=0, O=0 (variable-length input, variable-length output)
        (b'oiabcd', True, 'abcd.'),
        # I=0, O=0 (should ignore extra periods)
        (b'oi...abcd...', True, 'abcd.'),
        # I=0, O=6 (variable-length input, fixed-length output)
        (b'i.o6.x.xyz.toolongtofit.', False, 'x-----.xyz---.toolon.'),
        # I=2, O=6 (fixed-length input < fixed-length output)
        (b'i.i2.o6xyz', True, 'xy----.z-----.'),
        # I=6, O=3 (fixed-length input > fixed-length output)
        (b'i.o3.i6.abcdefghijklmnop', True, 'abc.ghi.mno.'),
        # I=0, then 3; O=29, then 15 (with longer output)
        (b'i.o29.a.b.cde.o15.abcdefghijabcdefghij.i3.a.b.c.d.ei00k.l.m', True,
         'a----------------------------.' +
         'b----------------------------.' +
         'cde--------------------------.' +
         'abcdefghijabcde.' +
         'a.b------------.' +
         '.c.------------.' +
         'd.e------------.' +
         'k--------------.' +
         'l--------------.' +
         'm--------------.')
    ]

    def testDecoder(self):
        # Try a few one-shot test cases.
        for input, eof, output in self.test_cases:
            d = StatefulIncrementalDecoder()
            self.assertEquals(d.decode(input, eof), output)

        # Also test an unfinished decode, followed by forcing EOF.
        d = StatefulIncrementalDecoder()
        self.assertEquals(d.decode(b'oiabcd'), '')
        self.assertEquals(d.decode(b'', 1), 'abcd.')

class TextIOWrapperTest(unittest.TestCase):

    def setUp(self):
        self.testdata = b"AAA\r\nBBB\rCCC\r\nDDD\nEEE\r\n"
        self.normalized = b"AAA\nBBB\nCCC\nDDD\nEEE\n".decode("ascii")
        support.unlink(support.TESTFN)

    def tearDown(self):
        support.unlink(support.TESTFN)

    def testConstructor(self):
        r = io.BytesIO(b"\xc3\xa9\n\n")
        b = io.BufferedReader(r, 1000)
        t = io.TextIOWrapper(b)
        t.__init__(b, encoding="latin1", newline="\r\n")
        t.__init__(b, encoding="utf8")
        self.assertEquals("\xe9\n", t.readline())
        self.assertRaises(TypeError, t.__init__, b, newline=42)
        self.assertRaises(ValueError, t.read)
        self.assertRaises(ValueError, t.__init__, b, newline='xyzzy')
        self.assertRaises(ValueError, t.read)

    def testLineBuffering(self):
        r = io.BytesIO()
        b = io.BufferedWriter(r, 1000)
        t = io.TextIOWrapper(b, newline="\n", line_buffering=True)
        t.write("X")
        self.assertEquals(r.getvalue(), b"")  # No flush happened
        t.write("Y\nZ")
        self.assertEquals(r.getvalue(), b"XY\nZ")  # All got flushed
        t.write("A\rB")
        self.assertEquals(r.getvalue(), b"XY\nZA\rB")

    def testEncoding(self):
        # Check the encoding attribute is always set, and valid
        b = io.BytesIO()
        t = io.TextIOWrapper(b, encoding="utf8")
        self.assertEqual(t.encoding, "utf8")
        t = io.TextIOWrapper(b)
        self.assert_(t.encoding is not None)
        codecs.lookup(t.encoding)

    def testEncodingErrorsReading(self):
        # (1) default
        b = io.BytesIO(b"abc\n\xff\n")
        t = io.TextIOWrapper(b, encoding="ascii")
        self.assertRaises(UnicodeError, t.read)
        # (2) explicit strict
        b = io.BytesIO(b"abc\n\xff\n")
        t = io.TextIOWrapper(b, encoding="ascii", errors="strict")
        self.assertRaises(UnicodeError, t.read)
        # (3) ignore
        b = io.BytesIO(b"abc\n\xff\n")
        t = io.TextIOWrapper(b, encoding="ascii", errors="ignore")
        self.assertEquals(t.read(), "abc\n\n")
        # (4) replace
        b = io.BytesIO(b"abc\n\xff\n")
        t = io.TextIOWrapper(b, encoding="ascii", errors="replace")
        self.assertEquals(t.read(), "abc\n\ufffd\n")

    def testEncodingErrorsWriting(self):
        # (1) default
        b = io.BytesIO()
        t = io.TextIOWrapper(b, encoding="ascii")
        self.assertRaises(UnicodeError, t.write, "\xff")
        # (2) explicit strict
        b = io.BytesIO()
        t = io.TextIOWrapper(b, encoding="ascii", errors="strict")
        self.assertRaises(UnicodeError, t.write, "\xff")
        # (3) ignore
        b = io.BytesIO()
        t = io.TextIOWrapper(b, encoding="ascii", errors="ignore",
                             newline="\n")
        t.write("abc\xffdef\n")
        t.flush()
        self.assertEquals(b.getvalue(), b"abcdef\n")
        # (4) replace
        b = io.BytesIO()
        t = io.TextIOWrapper(b, encoding="ascii", errors="replace",
                             newline="\n")
        t.write("abc\xffdef\n")
        t.flush()
        self.assertEquals(b.getvalue(), b"abc?def\n")

    def testNewlines(self):
        input_lines = [ "unix\n", "windows\r\n", "os9\r", "last\n", "nonl" ]

        tests = [
            [ None, [ 'unix\n', 'windows\n', 'os9\n', 'last\n', 'nonl' ] ],
            [ '', input_lines ],
            [ '\n', [ "unix\n", "windows\r\n", "os9\rlast\n", "nonl" ] ],
            [ '\r\n', [ "unix\nwindows\r\n", "os9\rlast\nnonl" ] ],
            [ '\r', [ "unix\nwindows\r", "\nos9\r", "last\nnonl" ] ],
        ]
        encodings = (
            'utf-8', 'latin-1',
            'utf-16', 'utf-16-le', 'utf-16-be',
            'utf-32', 'utf-32-le', 'utf-32-be',
        )

        # Try a range of buffer sizes to test the case where \r is the last
        # character in TextIOWrapper._pending_line.
        for encoding in encodings:
            # XXX: str.encode() should return bytes
            data = bytes(''.join(input_lines).encode(encoding))
            for do_reads in (False, True):
                for bufsize in range(1, 10):
                    for newline, exp_lines in tests:
                        bufio = io.BufferedReader(io.BytesIO(data), bufsize)
                        textio = io.TextIOWrapper(bufio, newline=newline,
                                                  encoding=encoding)
                        if do_reads:
                            got_lines = []
                            while True:
                                c2 = textio.read(2)
                                if c2 == '':
                                    break
                                self.assertEquals(len(c2), 2)
                                got_lines.append(c2 + textio.readline())
                        else:
                            got_lines = list(textio)

                        for got_line, exp_line in zip(got_lines, exp_lines):
                            self.assertEquals(got_line, exp_line)
                        self.assertEquals(len(got_lines), len(exp_lines))

    def testNewlinesInput(self):
        testdata = b"AAA\nBB\x00B\nCCC\rDDD\rEEE\r\nFFF\r\nGGG"
        normalized = testdata.replace(b"\r\n", b"\n").replace(b"\r", b"\n")
        for newline, expected in [
            (None, normalized.decode("ascii").splitlines(True)),
            ("", testdata.decode("ascii").splitlines(True)),
            ("\n", ["AAA\n", "BB\x00B\n", "CCC\rDDD\rEEE\r\n", "FFF\r\n", "GGG"]),
            ("\r\n", ["AAA\nBB\x00B\nCCC\rDDD\rEEE\r\n", "FFF\r\n", "GGG"]),
            ("\r",  ["AAA\nBB\x00B\nCCC\r", "DDD\r", "EEE\r", "\nFFF\r", "\nGGG"]),
            ]:
            buf = io.BytesIO(testdata)
            txt = io.TextIOWrapper(buf, encoding="ascii", newline=newline)
            self.assertEquals(txt.readlines(), expected)
            txt.seek(0)
            self.assertEquals(txt.read(), "".join(expected))

    def testNewlinesOutput(self):
        testdict = {
            "": b"AAA\nBBB\nCCC\nX\rY\r\nZ",
            "\n": b"AAA\nBBB\nCCC\nX\rY\r\nZ",
            "\r": b"AAA\rBBB\rCCC\rX\rY\r\rZ",
            "\r\n": b"AAA\r\nBBB\r\nCCC\r\nX\rY\r\r\nZ",
            }
        tests = [(None, testdict[os.linesep])] + sorted(testdict.items())
        for newline, expected in tests:
            buf = io.BytesIO()
            txt = io.TextIOWrapper(buf, encoding="ascii", newline=newline)
            txt.write("AAA\nB")
            txt.write("BB\nCCC\n")
            txt.write("X\rY\r\nZ")
            txt.flush()
            self.assertEquals(buf.closed, False)
            self.assertEquals(buf.getvalue(), expected)

    def testDestructor(self):
        l = []
        class MyBytesIO(io.BytesIO):
            def close(self):
                l.append(self.getvalue())
                io.BytesIO.close(self)
        b = MyBytesIO()
        t = io.TextIOWrapper(b, encoding="ascii")
        t.write("abc")
        del t
        self.assertEquals([b"abc"], l)

    def testOverrideDestructor(self):
        record = []
        class MyTextIO(io.TextIOWrapper):
            def __del__(self):
                record.append(1)
                try:
                    f = io.TextIOWrapper.__del__
                except AttributeError:
                    pass
                else:
                    f(self)
            def close(self):
                record.append(2)
                io.TextIOWrapper.close(self)
            def flush(self):
                record.append(3)
                io.TextIOWrapper.flush(self)
        b = io.BytesIO()
        t = MyTextIO(b, encoding="ascii")
        del t
        self.assertEqual(record, [1, 2, 3])

    def testErrorThroughDestructor(self):
        # Test that the exception state is not modified by a destructor,
        # even if close() fails.
        rawio = CloseFailureIO()
        def f():
            io.TextIOWrapper(rawio).xyzzy
        self.assertRaises(AttributeError, f)

    def test_garbage_collection(self):
        # TextIOWrapper objects are collected, and collecting them flushes
        # all data to disk.
        rawio = io.FileIO(support.TESTFN, "wb")
        b = io.BufferedWriter(rawio)
        t = io.TextIOWrapper(b, encoding="ascii")
        t.write("456def")
        t.x = t
        wr = weakref.ref(t)
        del t
        gc.collect()
        self.assert_(wr() is None, wr)
        with open(support.TESTFN, "rb") as f:
            self.assertEqual(f.read(), b"456def")

    # Systematic tests of the text I/O API

    def testBasicIO(self):
        for chunksize in (1, 2, 3, 4, 5, 15, 16, 17, 31, 32, 33, 63, 64, 65):
            for enc in "ascii", "latin1", "utf8" :# , "utf-16-be", "utf-16-le":
                f = io.open(support.TESTFN, "w+", encoding=enc)
                f._CHUNK_SIZE = chunksize
                self.assertEquals(f.write("abc"), 3)
                f.close()
                f = io.open(support.TESTFN, "r+", encoding=enc)
                f._CHUNK_SIZE = chunksize
                self.assertEquals(f.tell(), 0)
                self.assertEquals(f.read(), "abc")
                cookie = f.tell()
                self.assertEquals(f.seek(0), 0)
                self.assertEquals(f.read(2), "ab")
                self.assertEquals(f.read(1), "c")
                self.assertEquals(f.read(1), "")
                self.assertEquals(f.read(), "")
                self.assertEquals(f.tell(), cookie)
                self.assertEquals(f.seek(0), 0)
                self.assertEquals(f.seek(0, 2), cookie)
                self.assertEquals(f.write("def"), 3)
                self.assertEquals(f.seek(cookie), cookie)
                self.assertEquals(f.read(), "def")
                if enc.startswith("utf"):
                    self.multi_line_test(f, enc)
                f.close()

    def multi_line_test(self, f, enc):
        f.seek(0)
        f.truncate()
        sample = "s\xff\u0fff\uffff"
        wlines = []
        for size in (0, 1, 2, 3, 4, 5, 30, 31, 32, 33, 62, 63, 64, 65, 1000):
            chars = []
            for i in range(size):
                chars.append(sample[i % len(sample)])
            line = "".join(chars) + "\n"
            wlines.append((f.tell(), line))
            f.write(line)
        f.seek(0)
        rlines = []
        while True:
            pos = f.tell()
            line = f.readline()
            if not line:
                break
            rlines.append((pos, line))
        self.assertEquals(rlines, wlines)

    def testTelling(self):
        f = io.open(support.TESTFN, "w+", encoding="utf8")
        p0 = f.tell()
        f.write("\xff\n")
        p1 = f.tell()
        f.write("\xff\n")
        p2 = f.tell()
        f.seek(0)
        self.assertEquals(f.tell(), p0)
        self.assertEquals(f.readline(), "\xff\n")
        self.assertEquals(f.tell(), p1)
        self.assertEquals(f.readline(), "\xff\n")
        self.assertEquals(f.tell(), p2)
        f.seek(0)
        for line in f:
            self.assertEquals(line, "\xff\n")
            self.assertRaises(IOError, f.tell)
        self.assertEquals(f.tell(), p2)
        f.close()

    def testSeeking(self):
        chunk_size = _default_chunk_size()
        prefix_size = chunk_size - 2
        u_prefix = "a" * prefix_size
        prefix = bytes(u_prefix.encode("utf-8"))
        self.assertEquals(len(u_prefix), len(prefix))
        u_suffix = "\u8888\n"
        suffix = bytes(u_suffix.encode("utf-8"))
        line = prefix + suffix
        f = io.open(support.TESTFN, "wb")
        f.write(line*2)
        f.close()
        f = io.open(support.TESTFN, "r", encoding="utf-8")
        s = f.read(prefix_size)
        self.assertEquals(s, str(prefix, "ascii"))
        self.assertEquals(f.tell(), prefix_size)
        self.assertEquals(f.readline(), u_suffix)

    def testSeekingToo(self):
        # Regression test for a specific bug
        data = b'\xe0\xbf\xbf\n'
        f = io.open(support.TESTFN, "wb")
        f.write(data)
        f.close()
        f = io.open(support.TESTFN, "r", encoding="utf-8")
        f._CHUNK_SIZE  # Just test that it exists
        f._CHUNK_SIZE = 2
        f.readline()
        f.tell()

    def testSeekAndTell(self):
        """Test seek/tell using the StatefulIncrementalDecoder."""
        # Make test faster by doing smaller seeks
        CHUNK_SIZE = 256

        def testSeekAndTellWithData(data, min_pos=0):
            """Tell/seek to various points within a data stream and ensure
            that the decoded data returned by read() is consistent."""
            f = io.open(support.TESTFN, 'wb')
            f.write(data)
            f.close()
            f = io.open(support.TESTFN, encoding='test_decoder')
            f._CHUNK_SIZE = CHUNK_SIZE
            decoded = f.read()
            f.close()

            for i in range(min_pos, len(decoded) + 1): # seek positions
                for j in [1, 5, len(decoded) - i]: # read lengths
                    f = io.open(support.TESTFN, encoding='test_decoder')
                    self.assertEquals(f.read(i), decoded[:i])
                    cookie = f.tell()
                    self.assertEquals(f.read(j), decoded[i:i + j])
                    f.seek(cookie)
                    self.assertEquals(f.read(), decoded[i:])
                    f.close()

        # Enable the test decoder.
        StatefulIncrementalDecoder.codecEnabled = 1

        # Run the tests.
        try:
            # Try each test case.
            for input, _, _ in StatefulIncrementalDecoderTest.test_cases:
                testSeekAndTellWithData(input)

            # Position each test case so that it crosses a chunk boundary.
            #CHUNK_SIZE = _default_chunk_size()
            for input, _, _ in StatefulIncrementalDecoderTest.test_cases:
                offset = CHUNK_SIZE - len(input)//2
                prefix = b'.'*offset
                # Don't bother seeking into the prefix (takes too long).
                min_pos = offset*2
                testSeekAndTellWithData(prefix + input, min_pos)

        # Ensure our test decoder won't interfere with subsequent tests.
        finally:
            StatefulIncrementalDecoder.codecEnabled = 0

    def testEncodedWrites(self):
        data = "1234567890"
        tests = ("utf-16",
                 "utf-16-le",
                 "utf-16-be",
                 "utf-32",
                 "utf-32-le",
                 "utf-32-be")
        for encoding in tests:
            buf = io.BytesIO()
            f = io.TextIOWrapper(buf, encoding=encoding)
            # Check if the BOM is written only once (see issue1753).
            f.write(data)
            f.write(data)
            f.seek(0)
            self.assertEquals(f.read(), data * 2)
            self.assertEquals(buf.getvalue(), (data * 2).encode(encoding))

    def timingTest(self):
        timer = time.time
        enc = "utf8"
        line = "\0\x0f\xff\u0fff\uffff\U000fffff\U0010ffff"*3 + "\n"
        nlines = 10000
        nchars = len(line)
        nbytes = len(line.encode(enc))
        for chunk_size in (32, 64, 128, 256):
            f = io.open(support.TESTFN, "w+", encoding=enc)
            f._CHUNK_SIZE = chunk_size
            t0 = timer()
            for i in range(nlines):
                f.write(line)
            f.flush()
            t1 = timer()
            f.seek(0)
            for line in f:
                pass
            t2 = timer()
            f.seek(0)
            while f.readline():
                pass
            t3 = timer()
            f.seek(0)
            while f.readline():
                f.tell()
            t4 = timer()
            f.close()
            if support.verbose:
                print("\nTiming test: %d lines of %d characters (%d bytes)" %
                      (nlines, nchars, nbytes))
                print("File chunk size:          %6s" % f._CHUNK_SIZE)
                print("Writing:                  %6.3f seconds" % (t1-t0))
                print("Reading using iteration:  %6.3f seconds" % (t2-t1))
                print("Reading using readline(): %6.3f seconds" % (t3-t2))
                print("Using readline()+tell():  %6.3f seconds" % (t4-t3))

    def testReadOneByOne(self):
        txt = io.TextIOWrapper(io.BytesIO(b"AA\r\nBB"))
        reads = ""
        while True:
            c = txt.read(1)
            if not c:
                break
            reads += c
        self.assertEquals(reads, "AA\nBB")

    # read in amounts equal to TextIOWrapper._CHUNK_SIZE which is 128.
    def testReadByChunk(self):
        # make sure "\r\n" straddles 128 char boundary.
        txt = io.TextIOWrapper(io.BytesIO(b"A" * 127 + b"\r\nB"))
        reads = ""
        while True:
            c = txt.read(128)
            if not c:
                break
            reads += c
        self.assertEquals(reads, "A"*127+"\nB")

    def test_issue1395_1(self):
        txt = io.TextIOWrapper(io.BytesIO(self.testdata), encoding="ascii")

        # read one char at a time
        reads = ""
        while True:
            c = txt.read(1)
            if not c:
                break
            reads += c
        self.assertEquals(reads, self.normalized)

    def test_issue1395_2(self):
        txt = io.TextIOWrapper(io.BytesIO(self.testdata), encoding="ascii")
        txt._CHUNK_SIZE = 4

        reads = ""
        while True:
            c = txt.read(4)
            if not c:
                break
            reads += c
        self.assertEquals(reads, self.normalized)

    def test_issue1395_3(self):
        txt = io.TextIOWrapper(io.BytesIO(self.testdata), encoding="ascii")
        txt._CHUNK_SIZE = 4

        reads = txt.read(4)
        reads += txt.read(4)
        reads += txt.readline()
        reads += txt.readline()
        reads += txt.readline()
        self.assertEquals(reads, self.normalized)

    def test_issue1395_4(self):
        txt = io.TextIOWrapper(io.BytesIO(self.testdata), encoding="ascii")
        txt._CHUNK_SIZE = 4

        reads = txt.read(4)
        reads += txt.read()
        self.assertEquals(reads, self.normalized)

    def test_issue1395_5(self):
        txt = io.TextIOWrapper(io.BytesIO(self.testdata), encoding="ascii")
        txt._CHUNK_SIZE = 4

        reads = txt.read(4)
        pos = txt.tell()
        txt.seek(0)
        txt.seek(pos)
        self.assertEquals(txt.read(4), "BBB\n")

    def test_issue2282(self):
        buffer = io.BytesIO(self.testdata)
        txt = io.TextIOWrapper(buffer, encoding="ascii")

        self.assertEqual(buffer.seekable(), txt.seekable())

    def check_newline_decoder_utf8(self, decoder):
        # UTF-8 specific tests for a newline decoder
        def _check_decode(b, s, **kwargs):
            # We exercise getstate() / setstate() as well as decode()
            state = decoder.getstate()
            self.assertEquals(decoder.decode(b, **kwargs), s)
            decoder.setstate(state)
            self.assertEquals(decoder.decode(b, **kwargs), s)

        _check_decode(b'\xe8\xa2\x88', "\u8888")

        _check_decode(b'\xe8', "")
        _check_decode(b'\xa2', "")
        _check_decode(b'\x88', "\u8888")

        _check_decode(b'\xe8', "")
        _check_decode(b'\xa2', "")
        _check_decode(b'\x88', "\u8888")

        _check_decode(b'\xe8', "")
        self.assertRaises(UnicodeDecodeError, decoder.decode, b'', final=True)

        decoder.reset()
        _check_decode(b'\n', "\n")
        _check_decode(b'\r', "")
        _check_decode(b'', "\n", final=True)
        _check_decode(b'\r', "\n", final=True)

        _check_decode(b'\r', "")
        _check_decode(b'a', "\na")

        _check_decode(b'\r\r\n', "\n\n")
        _check_decode(b'\r', "")
        _check_decode(b'\r', "\n")
        _check_decode(b'\na', "\na")

        _check_decode(b'\xe8\xa2\x88\r\n', "\u8888\n")
        _check_decode(b'\xe8\xa2\x88', "\u8888")
        _check_decode(b'\n', "\n")
        _check_decode(b'\xe8\xa2\x88\r', "\u8888")
        _check_decode(b'\n', "\n")

    def check_newline_decoder(self, decoder, encoding):
        result = []
        encoder = codecs.getincrementalencoder(encoding)()
        def _decode_bytewise(s):
            for b in encoder.encode(s):
                result.append(decoder.decode(bytes([b])))
        self.assertEquals(decoder.newlines, None)
        _decode_bytewise("abc\n\r")
        self.assertEquals(decoder.newlines, '\n')
        _decode_bytewise("\nabc")
        self.assertEquals(decoder.newlines, ('\n', '\r\n'))
        _decode_bytewise("abc\r")
        self.assertEquals(decoder.newlines, ('\n', '\r\n'))
        _decode_bytewise("abc")
        self.assertEquals(decoder.newlines, ('\r', '\n', '\r\n'))
        _decode_bytewise("abc\r")
        self.assertEquals("".join(result), "abc\n\nabcabc\nabcabc")
        decoder.reset()
        self.assertEquals(decoder.decode("abc".encode(encoding)), "abc")
        self.assertEquals(decoder.newlines, None)

    def test_newline_decoder(self):
        encodings = (
            'utf-8', 'latin-1',
            'utf-16', 'utf-16-le', 'utf-16-be',
            'utf-32', 'utf-32-le', 'utf-32-be',
        )
        for enc in encodings:
            decoder = codecs.getincrementaldecoder(enc)()
            decoder = io.IncrementalNewlineDecoder(decoder, translate=True)
            self.check_newline_decoder(decoder, enc)
        decoder = codecs.getincrementaldecoder("utf-8")()
        decoder = io.IncrementalNewlineDecoder(decoder, translate=True)
        self.check_newline_decoder_utf8(decoder)


# XXX Tests for open()

class MiscIOTest(unittest.TestCase):

    def tearDown(self):
        support.unlink(support.TESTFN)

    def testImport__all__(self):
        for name in io.__all__:
            obj = getattr(io, name, None)
            self.assert_(obj is not None, name)
            if name == "open":
                continue
            elif "error" in name.lower():
                self.assert_(issubclass(obj, Exception), name)
            else:
                self.assert_(issubclass(obj, io.IOBase))

    def test_attributes(self):
        f = io.open(support.TESTFN, "wb", buffering=0)
        self.assertEquals(f.mode, "wb")
        f.close()

        f = io.open(support.TESTFN, "U")
        self.assertEquals(f.name,            support.TESTFN)
        self.assertEquals(f.buffer.name,     support.TESTFN)
        self.assertEquals(f.buffer.raw.name, support.TESTFN)
        self.assertEquals(f.mode,            "U")
        self.assertEquals(f.buffer.mode,     "rb")
        self.assertEquals(f.buffer.raw.mode, "rb")
        f.close()

        f = io.open(support.TESTFN, "w+")
        self.assertEquals(f.mode,            "w+")
        self.assertEquals(f.buffer.mode,     "rb+") # Does it really matter?
        self.assertEquals(f.buffer.raw.mode, "rb+")

        g = io.open(f.fileno(), "wb", closefd=False)
        self.assertEquals(g.mode,     "wb")
        self.assertEquals(g.raw.mode, "wb")
        self.assertEquals(g.name,     f.fileno())
        self.assertEquals(g.raw.name, f.fileno())
        f.close()
        g.close()

    def test_io_after_close(self):
        for kwargs in [
                {"mode": "w"},
                {"mode": "wb"},
                {"mode": "w", "buffering": 1},
                {"mode": "w", "buffering": 2},
                {"mode": "wb", "buffering": 0},
                {"mode": "r"},
                {"mode": "rb"},
                {"mode": "r", "buffering": 1},
                {"mode": "r", "buffering": 2},
                {"mode": "rb", "buffering": 0},
                {"mode": "w+"},
                {"mode": "w+b"},
                {"mode": "w+", "buffering": 1},
                {"mode": "w+", "buffering": 2},
                {"mode": "w+b", "buffering": 0},
            ]:
            f = io.open(support.TESTFN, **kwargs)
            f.close()
            self.assertRaises(ValueError, f.flush)
            self.assertRaises(ValueError, f.fileno)
            self.assertRaises(ValueError, f.isatty)
            self.assertRaises(ValueError, f.__iter__)
            if hasattr(f, "peek"):
                self.assertRaises(ValueError, f.peek, 1)
            self.assertRaises(ValueError, f.read)
            if hasattr(f, "read1"):
                self.assertRaises(ValueError, f.read1, 1024)
            if hasattr(f, "readinto"):
                self.assertRaises(ValueError, f.readinto, bytearray(1024))
            self.assertRaises(ValueError, f.readline)
            self.assertRaises(ValueError, f.readlines)
            self.assertRaises(ValueError, f.seek, 0)
            self.assertRaises(ValueError, f.tell)
            self.assertRaises(ValueError, f.truncate)
            self.assertRaises(ValueError, f.write,
                              b"" if "b" in kwargs['mode'] else "")
            self.assertRaises(ValueError, f.writelines, [])

    def test_blockingioerror(self):
        # Various BlockingIOError issues
        self.assertRaises(TypeError, io.BlockingIOError)
        self.assertRaises(TypeError, io.BlockingIOError, 1)
        self.assertRaises(TypeError, io.BlockingIOError, 1, 2, 3, 4)
        self.assertRaises(TypeError, io.BlockingIOError, 1, "", None)
        b = io.BlockingIOError(1, "")
        self.assertEqual(b.characters_written, 0)
        class C(str):
            pass
        c = C("")
        b = io.BlockingIOError(1, c)
        c.b = b
        b.c = c
        wr = weakref.ref(c)
        del c, b
        gc.collect()
        self.assert_(wr() is None, wr)

def test_main():
    support.run_unittest(IOTest, BytesIOTest, StringIOTest,
                         BufferedReaderTest, BufferedWriterTest,
                         BufferedRWPairTest, BufferedRandomTest,
                         StatefulIncrementalDecoderTest,
                         TextIOWrapperTest, MiscIOTest
                         )

if __name__ == "__main__":
    test_main()
