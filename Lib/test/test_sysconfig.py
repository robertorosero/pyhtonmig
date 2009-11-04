"""Tests for 'site'.

Tests assume the initial paths in sys.path once the interpreter has begun
executing have not been removed.

"""
import unittest
import sys
import test
import os
from test.test_support import run_unittest, TESTFN
from sysconfig import *
from sysconfig import _INSTALL_SCHEMES, _SCHEME_KEYS

class TestSysConfig(unittest.TestCase):

    def setUp(self):
        """Make a copy of sys.path"""
        super(TestSysConfig, self).setUp()
        self.sys_path = sys.path[:]
        self.makefile = None

    def tearDown(self):
        """Restore sys.path"""
        sys.path[:] = self.sys_path
        if self.makefile is not None:
            os.unlink(self.makefile)
        self._cleanup_testfn()
        super(TestSysConfig, self).tearDown()

    def _cleanup_testfn(self):
        path = test.test_support.TESTFN
        if os.path.isfile(path):
            os.remove(path)
        elif os.path.isdir(path):
            shutil.rmtree(path)

    def test_get_paths(self):
        # XXX make it os independant
        scheme = get_paths()
        wanted = {'purelib': '/usr/local/lib/python',
                  'headers': '/usr/local/include/python/',
                  'platlib': '/usr/local/lib/python',
                  'data': '/usr/local',
                  'scripts': '/usr/local/bin'}

        wanted = wanted.items()
        wanted.sort()
        scheme = scheme.items()
        scheme.sort()
        self.assertEquals(scheme, wanted)

    def test_get_path(self):
        for key in _INSTALL_SCHEMES:
            for name in _SCHEME_KEYS:
                res = get_path(key, name)

    def test_get_config_h_filename(self):
        config_h = get_config_h_filename()
        self.assertTrue(os.path.isfile(config_h), config_h)

    def test_get_python_lib(self):
        lib_dir = get_python_lib()
        # XXX doesn't work on Linux when Python was never installed before
        #self.assertTrue(os.path.isdir(lib_dir), lib_dir)
        # test for pythonxx.lib?
        self.assertNotEqual(get_python_lib(),
                            get_python_lib(prefix=TESTFN))

    def test_get_python_inc(self):
        inc_dir = get_python_inc()
        # This is not much of a test.  We make sure Python.h exists
        # in the directory returned by get_python_inc() but we don't know
        # it is the correct file.
        self.assertTrue(os.path.isdir(inc_dir), inc_dir)
        python_h = os.path.join(inc_dir, "Python.h")
        self.assertTrue(os.path.isfile(python_h), python_h)

    def test_get_config_vars(self):
        cvars = get_config_vars()
        self.assertTrue(isinstance(cvars, dict))
        self.assertTrue(cvars)

def test_main():
    run_unittest(TestSysConfig)

if __name__ == "__main__":
    test_main()
