import unittest, sys, os
from test import support

d1="d1"
d2="d2"
d3="d3"
d4="d4"
test_namespace_prefix = 'pep382test'

class restoring_sys_modules:
    # in __exit__ removes any modules in sys.modules that start with module_prefix
    def __init__(self, module_prefix):
        self.module_prefix = module_prefix

    def __enter__(self):
        # just make sure no such modules are already present
        for key in sys.modules:
            if key.startswith(self.module_prefix):
                assert False, '{} already in sys.modules'.format(key)

        return self

    def __exit__(self, *ignore_exc):
        # delete all matching modules

        # convert sys.modules to a list because we're going to mutate it
        for key in list(sys.modules):
            if key.startswith(self.module_prefix):
                del sys.modules[key]


class PthTestsBase(unittest.TestCase):
    """Try various combinations of __init__ and .pth files.
    At the end, remove everything from sys.path and sys.modules.
    The namespace package package begin tested is always pep382test.
    Directory structure:
    - d1: wildcard pth file, plus d1.py
    - d2: wildcard pth file,
          plus containing some missing directories,
          plus d2.py
    - d3: __init__, setting D3 to "d3", plus d3.py
    - d4: __init__, plus wildcard pth, plus d4.py
    """

    def add_to_syspath(self, *items):
        # Must be implemented by subclasses to add the proper items
        # (e.g. directories or zipfiles) to sys.path.  This must
        # return a context manager which does any restoration, as
        # needed.  items will be a sequence of subpath names (e.g. d1,
        # d2, d3, d4) which the subclasses must map into the actual
        # thing to add.
        raise NotImplementedError

    def test_d1_d2_d3_d4(self):
        'All directories should show up in the __path__'
        with self.add_to_syspath(d1, d2, d3, d4), restoring_sys_modules(test_namespace_prefix):
            try:
                import pep382test
                import pep382test.d1
                import pep382test.d2
                import pep382test.d3
                import pep382test.d4
            except ImportError as e:
                self.fail(str(e))
            self.assertTrue(pep382test.d1.imported)
            self.assertTrue(pep382test.d2.imported)
            self.assertTrue(pep382test.d3.imported)
            self.assertTrue(pep382test.d4.imported)
            self.assertEquals(pep382test.D3, "d3")
            self.assertTrue("/does/not/exist" in pep382test.__path__)
            self.assertTrue("/does/not/exist/either" in pep382test.__path__)

    def test_d4_d3_d2_d1(self):
        'All directories should show up in the __path__'
        with self.add_to_syspath(d4, d3, d2, d1), restoring_sys_modules(test_namespace_prefix):
            try:
                import pep382test.d1
                import pep382test.d2
                import pep382test.d3
                import pep382test.d4
            except ImportError as e:
                self.fail(str(e))
            self.assertTrue(pep382test.d1.imported)
            self.assertTrue(pep382test.d2.imported)
            self.assertTrue(pep382test.d3.imported)
            self.assertTrue(pep382test.d4.imported)

    def test_d3_d4_d2_d1(self):
        'Only d3 should be imported, since there is no pth file'
        with self.add_to_syspath(d3, d4, d2, d1), restoring_sys_modules(test_namespace_prefix):
            try:
                import pep382test.d3
            except ImportError as e:
                self.fail(str(e))
            try:
                import pep382test.d1
            except ImportError as e:
                pass
            else:
                self.fail("Found d1 unexpectedly")
            self.assertTrue(pep382test.d3.imported)
            self.assertEquals(pep382test.D3, "d3")


class PthFilesystemTests(PthTestsBase):
    """Test namespace packages that live solely on the filesystem."""

    def add_to_syspath(self, *items):
        base = os.path.dirname(__file__)
        return support.DirsOnSysPath(*[os.path.join(base, "pep382", dir) for dir in items])

def test_main():
    tests = [PthFilesystemTests]
    support.run_unittest(*tests)


if __name__ == "__main__":
    test_main()
