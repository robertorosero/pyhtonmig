import errno
import os
import select
import socket
import sys
import unittest

from test import support

class HierarchyTest(unittest.TestCase):

    def test_builtin_errors(self):
        self.assertEqual(IOError.__name__, 'IOError')
        self.assertIs(OSError, IOError)
        self.assertIs(EnvironmentError, IOError)

    def test_socket_errors(self):
        self.assertIs(socket.error, IOError)
        self.assertIs(socket.gaierror.__base__, IOError)
        self.assertIs(socket.herror.__base__, IOError)
        self.assertIs(socket.timeout.__base__, IOError)

    def test_select_error(self):
        self.assertIs(select.error, IOError)

    # mmap.error is tested in test_mmap

    def test_windows_error(self):
        if os.name == "nt":
            self.assertIn('winerror', dir(IOError))
        else:
            self.assertNotIn('winerror', dir(IOError))

    # XXX VMSError not tested


def test_main():
    support.run_unittest(__name__)

if __name__=="__main__":
    test_main()
