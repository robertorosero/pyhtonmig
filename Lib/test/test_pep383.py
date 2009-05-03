from test import support
import unittest, shutil, os, sys

class Utf8bTest(unittest.TestCase):

    def test_utf8(self):
        # Bad byte
        self.assertEqual(b"foo\x80bar".decode("utf-8", "utf8b"),
                         "foo\udc80bar")
        self.assertEqual("foo\udc80bar".encode("utf-8", "utf8b"),
                         b"foo\x80bar")
        # bad-utf-8 encoded surrogate
        self.assertEqual(b"\xed\xb0\x80".decode("utf-8", "utf8b"),
                         "\udced\udcb0\udc80")
        self.assertEqual("\udced\udcb0\udc80".encode("utf-8", "utf8b"),
                         b"\xed\xb0\x80")

    def test_ascii(self):
        # bad byte
        self.assertEqual(b"foo\x80bar".decode("ascii", "utf8b"),
                         "foo\udc80bar")
        self.assertEqual("foo\udc80bar".encode("ascii", "utf8b"),
                         b"foo\x80bar")

    def test_charmap(self):
        # bad byte: \xa5 is unmapped in iso-8859-3
        self.assertEqual(b"foo\xa5bar".decode("iso-8859-3", "utf8b"),
                         "foo\udca5bar")
        self.assertEqual("foo\udca5bar".encode("iso-8859-4", "utf8b"),
                         b"foo\xa5bar")

class FileTests(unittest.TestCase):
    if os.name != 'win32':
        filenames = [b'foo\xf6bar', b'foo\xf6bar']
    else:
        # PEP 383 has no effect on file name handling on Windows
        filenames = []

    def setUp(self):
        self.fsencoding = sys.getfilesystemencoding()
        sys.setfilesystemencoding("utf-8")
        self.dir = support.TESTFN
        self.bdir = self.dir.encode("utf-8", "utf8b")
        os.mkdir(self.dir)
        self.unicodefn = []
        for fn in self.filenames:
            f = open(self.bdir + b"/" + fn, "w")
            f.close()
            self.unicodefn.append(fn.decode("utf-8", "utf8b"))

    def tearDown(self):
        shutil.rmtree(self.dir)
        sys.setfilesystemencoding(self.fsencoding)

    def test_listdir(self):
        expected = set(self.unicodefn)
        found = set(os.listdir(support.TESTFN))
        self.assertEquals(found, expected)

    def test_open(self):
        for fn in self.unicodefn:
            f = open(os.path.join(self.dir, fn))
            f.close()

    def test_stat(self):
        for fn in self.unicodefn:
            os.stat(os.path.join(self.dir, fn))

def test_main():
    support.run_unittest(
        Utf8bTest,
        FileTests)


if __name__ == "__main__":
    test_main()
