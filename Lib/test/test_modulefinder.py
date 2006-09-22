import __future__
import sys, os
import unittest
import distutils.dir_util

from test import test_support

try: set
except NameError: from sets import Set as set

import modulefinder

# XXX To test modulefinder with Python 2.2, sets.py and
# modulefinder.py must be available - they are not in the standard
# library.

# XXX FIXME: do NOT create files in the current directory
TEST_DIR = os.path.abspath("testing")
TEST_PATH = [TEST_DIR, os.path.dirname(__future__.__file__)]

# Each test description is a list of 4 items:
#
# 1. a module name that will be imported by modulefinder
# 2. a list of module names that modulefinder is required to find
# 3. a list of module names that modulefinder should complain
#    about because they are not found
# 4. a string specifying a package to create; the format is obvious imo.
#
# Each package will be created in TEST_DIR, and TEST_DIR will be
# removed after the tests again.
# Modulefinder searches in a path that contains TEST_DIR, plus
# the standard Lib directory.

package_test = [
    "a.module",
    ["a", "a.b", "a.c", "a.module", "mymodule", "sys"],
    ["blahblah"],
    """\
mymodule.py
a/__init__.py
                                import blahblah
                                from a import b
                                import c
a/module.py
                                import sys
                                from a import b as x
                                from a.c import sillyname
a/b.py
a/c.py
                                from a.module import x
                                import mymodule as sillyname
"""]

absolute_import_test = [
    "a.module",
    ["a", "a.module",
     "b", "b.x", "b.y", "b.z",
     "__future__", "sys", "exceptions"],
    ["blahblah"],
    """\
mymodule.py
a/__init__.py
a/module.py
                                from __future__ import absolute_import
                                import sys # sys
                                import blahblah # fails
                                import exceptions # exceptions
                                import b.x # b.x
                                from b import y # b.y
                                from b.z import * # b.z.*
a/exceptions.py
a/sys.py
                                import mymodule
a/b/__init__.py
a/b/x.py
a/b/y.py
a/b/z.py
b/__init__.py
                                import z
b/unused.py
b/x.py
b/y.py
b/z.py
"""]

relative_import_test = [
    "a.module",
    ["__future__",
     "a", "a.module",
     "a.b", "a.b.y", "a.b.z",
     "a.b.c", "a.b.c.moduleC",
     "a.b.c.d", "a.b.c.e",
     "exceptions"],
    [],
# The 'from ... import name' constructs stil fail'
    """\
mymodule.py
a/__init__.py
                                ##from . import sys # a.sys
a/module.py
                                from __future__ import absolute_import # __future__
                                import exceptions # exceptions
                                #from . import x # a.x
                                from .b import y, z
                                #from . import sys # a.sys
a/exceptions.py
a/sys.py
a/b/__init__.py
                                #from .c import moduleC
                                from a.b.c import moduleC
a/b/x.py
a/b/y.py
a/b/z.py
a/b/c/__init__.py
                                from ..c import e # a.b.c.e
a/b/c/moduleC.py
                                #
                                #from .. import c
                                #from .. import x # a.b.x
                                from ..c import d # a.b.c.d
a/b/c/d.py
a/b/c/e.py
"""]

def open_file(path):
    ##print "#", os.path.abspath(path)
    dirname = os.path.dirname(path)
    distutils.dir_util.mkpath(dirname)
    return open(path, "w")

def create_package(source):
    ofi = None
    for line in source.splitlines():
        if line.startswith(" ") or line.startswith("\t"):
            ofi.write(line.strip() + "\n")
        else:
            ofi = open_file(os.path.join(TEST_DIR, line.strip()))

class ModuleFinderTest(unittest.TestCase):
    def _do_test(self, info, report=False):
        import_this, modules, missing, source = info
        create_package(source)
        try:
            mf = modulefinder.ModuleFinder(path=TEST_PATH)
            mf.import_hook(import_this)
            if report:
                mf.report()

                opath = sys.path[:]
                sys.path = TEST_PATH
                try:
                    __import__(import_this)
                except:
                    import traceback; traceback.print_exc()
                sys.path = opath

            modules = set(modules)
            found = set(mf.modules.keys())
            more = list(found - modules)
            less = list(modules - found)
            # check if we found what we expected, not more, not less
            self.failUnlessEqual((more, less), ([], []))

            # check if missing modules are reported correctly
            bad = mf.badmodules.keys()
            self.failUnlessEqual(bad, missing)
        finally:
            distutils.dir_util.remove_tree(TEST_DIR)

    def test_package(self):
        self._do_test(package_test)

    if getattr(__future__, "absolute_import", None):

        def test_absolute_imports(self):
            self._do_test(absolute_import_test)

        def test_relative_imports(self):
            self._do_test(relative_import_test)

def test_main():
    test_support.run_unittest(ModuleFinderTest)

if __name__ == "__main__":
    unittest.main()
