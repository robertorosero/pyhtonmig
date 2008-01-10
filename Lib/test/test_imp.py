import imp
import sys
import thread
import unittest
from test import test_support


class LockTests(unittest.TestCase):

    """Very basic test of import lock functions."""

    def verify_lock_state(self, expected):
        self.failUnlessEqual(imp.lock_held(), expected,
                             "expected imp.lock_held() to be %r" % expected)
    def testLock(self):
        LOOPS = 50

        # The import lock may already be held, e.g. if the test suite is run
        # via "import test.autotest".
        lock_held_at_start = imp.lock_held()
        self.verify_lock_state(lock_held_at_start)

        for i in range(LOOPS):
            imp.acquire_lock()
            self.verify_lock_state(True)

        for i in range(LOOPS):
            imp.release_lock()

        # The original state should be restored now.
        self.verify_lock_state(lock_held_at_start)

        if not lock_held_at_start:
            try:
                imp.release_lock()
            except RuntimeError:
                pass
            else:
                self.fail("release_lock() without lock should raise "
                            "RuntimeError")

class ImportTests(unittest.TestCase):

    def test_find_module_encoding(self):
        fd = imp.find_module("heapq")[0]
        self.assertEqual(fd.encoding, "iso-8859-1")

    def test_issue1267(self):
        fp, filename, info  = imp.find_module("pydoc")
        self.assertNotEqual(fp, None)
        self.assertEqual(fp.encoding, "iso-8859-1")
        self.assertEqual(fp.tell(), 0)
        self.assertEqual(fp.readline(), '#!/usr/bin/env python\n')
        fp.close()

        fp, filename, info = imp.find_module("tokenize")
        self.assertNotEqual(fp, None)
        self.assertEqual(fp.encoding, "utf-8")
        self.assertEqual(fp.tell(), 0)
        self.assertEqual(fp.readline(),
                         '"""Tokenization help for Python programs.\n')
        fp.close()

    def test_reload(self):
        import marshal
        imp.reload(marshal)
        import string
        imp.reload(string)
        ## import sys
        ## self.assertRaises(ImportError, reload, sys)

class CallBack:
    def __init__(self):
        self.mods = {}

    def __call__(self, mod):
        self.mods[mod.__name__] = mod

class PostImportHookTests(unittest.TestCase):

    def setUp(self):
        if "telnetlib" in sys.modules:
            del sys.modules["telnetlib"]
        self.pihr = sys.post_import_hooks.copy()

    def tearDown(self):
        if "telnetlib" in sys.modules:
            del sys.modules["telnetlib"]
        sys.post_import_hooks = self.pihr

    def test_registry(self):
        reg = sys.post_import_hooks
        self.assert_(isinstance(reg, dict))

    def test_invalid_registry(self):
        sys.post_import_hooks = []
        self.assertRaises(TypeError, imp.register_post_import_hook,
                          lambda mod: None, "sys")
        sys.post_import_hooks = {}
        imp.register_post_import_hook(lambda mod: None, "sys")

        sys.post_import_hooks["telnetlib"] = lambda mod: None
        self.assertRaises(TypeError, __import__, "telnetlib")
        sys.post_import_hooks = self.pihr

    def test_register_callback_existing(self):
        callback = CallBack()
        imp.register_post_import_hook(callback, "sys")

        # sys is already loaded and the callback is fired immediately
        self.assert_("sys" in callback.mods, callback.mods)
        self.assert_(callback.mods["sys"] is sys, callback.mods)
        self.failIf("telnetlib" in callback.mods, callback.mods)
        self.assertEqual(sys.post_import_hooks["sys"], None)

    def test_register_callback_new(self):
        callback = CallBack()
        # an arbitrary module
        if "telnetlib" in sys.modules:
            del sys.modules["telnetlib"]
        imp.register_post_import_hook(callback, "telnetlib")

        regc = sys.post_import_hooks.get("telnetlib")
        self.assert_(regc is not None, regc)
        self.assert_(isinstance(regc, list), regc)
        self.assert_(callback in regc, regc)

        import telnetlib
        self.assert_("telnetlib" in callback.mods, callback.mods)
        self.assert_(callback.mods["telnetlib"] is telnetlib, callback.mods)
        self.assertEqual(sys.post_import_hooks["telnetlib"], None)

    def test_post_import_notify(self):
        imp.notify_module_loaded(sys)
        self.failUnlessRaises(TypeError, imp.notify_module_loaded, None)
        self.failUnlessRaises(TypeError, imp.notify_module_loaded, object())
        # Should this fail?
        mod = imp.new_module("post_import_test_module")
        imp.notify_module_loaded(mod)


def test_main():
    test_support.run_unittest(
        LockTests,
        ImportTests,
        PostImportHookTests,
    )

if __name__ == "__main__":
    test_main()
