import os
import imp
import sys
import thread
import unittest
import shutil
import tempfile
from test import test_support


def when_imported(name):
    def register(hook):
        imp.register_post_import_hook(hook, name)
    return register


class LockTests(unittest.TestCase):

    """Very basic test of import lock functions."""

    def verify_lock_state(self, expected):
        self.failUnlessEqual(imp.lock_held(), expected,
                             "expected imp.lock_held() to be %r" % expected)
    def XtestLock(self):
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

    def Xtest_find_module_encoding(self):
        fd = imp.find_module("heapq")[0]
        self.assertEqual(fd.encoding, "iso-8859-1")

    def Xtest_issue1267(self):
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

    def Xtest_reload(self):
        import marshal
        imp.reload(marshal)
        import string
        imp.reload(string)
        ## import sys
        ## self.assertRaises(ImportError, reload, sys)

# from test_pkg
def mkhier(descr):
    root = tempfile.mkdtemp()
    sys.path.insert(0, root)
    if not os.path.isdir(root):
        os.mkdir(root)
    for name, contents in descr:
        comps = name.split()
        fullname = root
        for c in comps:
            fullname = os.path.join(fullname, c)
        if contents is None:
            os.mkdir(fullname)
        else:
            with open(fullname+".py", "w") as f:
                f.write(contents)
                if contents and contents[-1] != '\n':
                    f.write('\n')
    return root

hier = [
    ("pih_test", None),
    ("pih_test __init__", "package = 0"),
    ("pih_test a", None),
    ("pih_test a __init__", "package = 1"),
    ("pih_test a b", None),
    ("pih_test a b __init__", "package = 2"),
]

mod_callback = """
import imp

def callback(mod):
    info = ("pih_test%(info)s", mod.__name__)
    imp.test_callbacks.append(info)

def callback_added(mod):
    info = ("ADDED in pih_test%(info)s", mod.__name__)
    imp.test_callbacks.append(info)

def callback_add(mod):
    callback(mod)
    imp.register_post_import_hook(callback_added, "pih_test.a")

imp.register_post_import_hook(callback, "pih_test")
imp.register_post_import_hook(callback_add, "pih_test.a")
imp.register_post_import_hook(callback, "pih_test.a.b")
"""

hier_withregister = [
    ("pih_test", None),
    ("pih_test __init__", mod_callback % {'info':''}),
    ("pih_test a", None),
    ("pih_test a __init__", mod_callback % {'info':'.a'}),
    ("pih_test a b", None),
    ("pih_test a b __init__", mod_callback % {'info':'.a.b'}),
]

class CallBack:
    def __init__(self):
        self.mods = {}
        self.names = []

    def __call__(self, mod):
        self.mods[mod.__name__] = mod
        self.names.append(mod.__name__)

class PostImportHookTests(unittest.TestCase):

    def setUp(self):
        self.sys_pih = sys.post_import_hooks.copy()
        self.module_names = set(sys.modules)
        self.sys_path = list(sys.path)
        imp.test_callbacks = []
        self.tmpdir = None

    def tearDown(self):
        sys.post_import_hooks = self.sys_pih
        sys.path = self.sys_path
        del imp.test_callbacks
        for name in list(sys.modules):
            if name not in self.module_names:
                del sys.modules[name]
        if self.tmpdir:
            shutil.rmtree(self.tmpdir)

    def test_registry(self):
        reg = sys.post_import_hooks
        self.assert_(isinstance(reg, dict))

    def test_invalid_registry(self):
        sys.post_import_hooks = []
        self.assertRaises(TypeError, imp.register_post_import_hook,
                          lambda mod: None, "sys")
        sys.post_import_hooks = {}
        imp.register_post_import_hook(lambda mod: None, "sys")

        self.tmpdir = mkhier([("pih_test_module", "example = 23")])
        sys.post_import_hooks["pih_test_module"] = lambda mod: None
        self.assertRaises(TypeError, __import__, "pih_test_module")
        sys.post_import_hooks = self.sys_pih

    def test_register_callback_existing(self):
        callback = CallBack()
        imp.register_post_import_hook(callback, "sys")

        # sys is already loaded and the callback is fired immediately
        self.assert_("sys" in callback.mods, callback.mods)
        self.assert_(callback.mods["sys"] is sys, callback.mods)
        self.failIf("pih_test_module" in callback.mods, callback.mods)
        self.assertEqual(sys.post_import_hooks["sys"], None)

    def test_register_callback_new(self):
        self.tmpdir = mkhier([("pih_test_module", "example = 23")])
        self.assert_("pih_test_module" not in sys.post_import_hooks)
        self.assert_("pih_test_module" not in sys.modules)

        callback = CallBack()
        imp.register_post_import_hook(callback, "pih_test_module")

        regc = sys.post_import_hooks.get("pih_test_module")
        self.assertNotEqual(regc, None, (regc, callback.mods))
        self.assert_(isinstance(regc, list), regc)
        self.assert_(callback in regc, regc)

        import pih_test_module
        self.assertEqual(pih_test_module.example, 23)
        self.assert_("pih_test_module" in callback.mods, callback.mods)
        self.assert_(callback.mods["pih_test_module"] is pih_test_module,
                     callback.mods)
        self.assertEqual(sys.post_import_hooks["pih_test_module"], None)

    def test_post_import_notify(self):
        imp.notify_module_loaded(sys)
        self.failUnlessRaises(TypeError, imp.notify_module_loaded, None)
        self.failUnlessRaises(TypeError, imp.notify_module_loaded, object())
        # Should this fail?
        mod = imp.new_module("post_import_test_module")
        self.failUnlessRaises(KeyError, imp.notify_module_loaded, mod)
        sys.modules['pih_test'] = None
        self.failUnlessRaises(TypeError, imp.notify_module_loaded, 'pih_test')
        class Example(object):
            __name__ = 'pih_test'
        sys.modules['pih_test'] = Example
        self.failUnlessRaises(TypeError, imp.notify_module_loaded, 'pih_test')
        sys.modules['pih_test'] = Example()
        self.failUnlessRaises(TypeError, imp.notify_module_loaded, 'pih_test')
        del sys.modules['pih_test']
        self.failUnlessRaises(KeyError, imp.notify_module_loaded, 'pih_test')

    def test_hook_hirarchie(self):
        self.tmpdir = mkhier(hier)
        callback = CallBack()
        imp.register_post_import_hook(callback, "pih_test")
        imp.register_post_import_hook(callback, "pih_test.a")
        imp.register_post_import_hook(callback, "pih_test.a.b")

        import pih_test
        self.assertEqual(callback.names, ["pih_test"])
        import pih_test.a
        self.assertEqual(callback.names, ["pih_test", "pih_test.a"])
        from pih_test import a
        self.assertEqual(callback.names, ["pih_test", "pih_test.a"])
        import pih_test.a.b
        self.assertEqual(callback.names,
                         ["pih_test", "pih_test.a", "pih_test.a.b"])
        from pih_test.a import b
        self.assertEqual(callback.names,
                         ["pih_test", "pih_test.a", "pih_test.a.b"])

    def test_hook_hirarchie_recursive(self):
        self.tmpdir = mkhier(hier)
        callback = CallBack()
        imp.register_post_import_hook(callback, "pih_test")
        imp.register_post_import_hook(callback, "pih_test.a")
        imp.register_post_import_hook(callback, "pih_test.a.b")

        import pih_test.a.b
        self.assertEqual(callback.names,
                         ["pih_test", "pih_test.a", "pih_test.a.b"])
        import pih_test
        self.assertEqual(callback.names,
                         ["pih_test", "pih_test.a", "pih_test.a.b"])
        import pih_test.a
        self.assertEqual(callback.names,
                         ["pih_test", "pih_test.a", "pih_test.a.b"])

    def test_hook_hirarchie_withregister(self):
        self.tmpdir = mkhier(hier_withregister)

        def callback(mod):
            info = ('', mod.__name__)
            imp.test_callbacks.append(info)

        imp.register_post_import_hook(callback, "pih_test")
        imp.register_post_import_hook(callback, "pih_test.a")
        imp.register_post_import_hook(callback, "pih_test.a.b")

        expected = []
        self.assertEqual(imp.test_callbacks, expected)

        import pih_test
        expected.append(("", "pih_test"))
        expected.append(("pih_test", "pih_test"))
        self.assertEqual(imp.test_callbacks, expected)

        import pih_test.a
        expected.append(("pih_test.a", "pih_test"))
        expected.append(("", "pih_test.a"))
        expected.append(("pih_test", "pih_test.a"))
        expected.append(("pih_test.a", "pih_test.a"))
        # delayed
        expected.append(("ADDED in pih_test", "pih_test.a"))
        expected.append(("ADDED in pih_test.a", "pih_test.a"))
        self.assertEqual(imp.test_callbacks, expected)

        import pih_test.a.b
        expected.append(("pih_test.a.b", "pih_test"))
        expected.append(("pih_test.a.b", "pih_test.a"))
        # This one is called immediately because pih_test.a is already laoded
        expected.append(("ADDED in pih_test.a.b", "pih_test.a"))
        expected.append(("", "pih_test.a.b"))
        expected.append(("pih_test", "pih_test.a.b"))
        expected.append(("pih_test.a", "pih_test.a.b"))
        expected.append(("pih_test.a.b", "pih_test.a.b"))
        self.assertEqual(imp.test_callbacks, expected)

    def test_notifyloaded_byname(self):
        callback = CallBack()

        imp.register_post_import_hook(callback, "pih_test")
        imp.register_post_import_hook(callback, "pih_test.a")
        imp.register_post_import_hook(callback, "pih_test.a.b")
        self.assertEqual(callback.names, [])

        for name in ("pih_test", "pih_test.a", "pih_test.a.b"):
            mod = imp.new_module(name)
            sys.modules[name] = mod
        self.assertEqual(callback.names, [])

        self.assert_("pih_test.a.b" in sys.modules)
        mod2 = imp.notify_module_loaded("pih_test.a.b")
        self.failUnless(mod is mod2, (mod, mod2))
        self.assertEqual(mod.__name__,  "pih_test.a.b")

        self.assertEqual(callback.names,
                         ["pih_test", "pih_test.a", "pih_test.a.b"])

    def test_tricky(self):
        called = []
        def func_a2(mod):
            called.append("func_a2")
        def func_ab1(mod):
            called.append("func_ab1")
        def func_ab2(mod):
            called.append("func_ab2")
        def func_ab3(mod):
            called.append("func_ab3")

        when_imported('a.b')(func_ab1)
        when_imported('a.b')(func_ab2)

        @when_imported('a')
        def func_a1(module_a):
            called.append("func_a1")
            when_imported('a.b')(func_ab3)
            # this is here to foil trivial implementations
            imp.notify_module_loaded('a.b')

        when_imported('a')(func_a2)

        # insert the modules into sys.modules to fake a 3rd party import
        a = imp.new_module('a')
        ab = imp.new_module('a.b')
        a.b = ab
        sys.modules["a"] = a
        sys.modules["a.b"] = ab
        # notify
        imp.notify_module_loaded('a.b')

        expected = ["func_a1", "func_a2", "func_ab1", "func_ab2", "func_ab3"]
        self.assertEqual(called, expected)

def test_main():
    test_support.run_unittest(
        #LockTests,
        #ImportTests,
        PostImportHookTests,
    )

if __name__ == "__main__":
    test_main()
