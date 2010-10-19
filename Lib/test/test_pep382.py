import unittest, sys, os
import contextlib
import zipfile

from test import support

d1 = "d1"
d2 = "d2"
d3 = "d3"
d4 = "d4"
test_namespace_prefix = 'pep382test'

@contextlib.contextmanager
def restoring_sys_modules(module_prefix):
    # after running, removes any modules in sys.modules that start with module_prefix

    def matching_modules():
        # return module names that match the prefix
        return [key for key in sys.modules if key.startswith(module_prefix)]

    # just make sure no matching modules are already present
    for key in matching_modules():
        assert False, '{} already in sys.modules'.format(key)

    yield

    # delete all matching modules
    for key in matching_modules():
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
            self.assertEqual(pep382test.D3, "d3")
            self.assertIn("/does/not/exist", pep382test.__path__)
            self.assertIn("/does/not/exist/either", pep382test.__path__)

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
            self.assertEqual(pep382test.D3, "d3")


class PthFilesystemTests(PthTestsBase):
    """Test namespace packages that live solely on the filesystem."""

    def add_to_syspath(self, *items):
        base = os.path.dirname(__file__)
        return support.DirsOnSysPath(*[os.path.join(base, "pep382", dir) for dir in items])


class ZipTester:
    ignored_dirs = ['.svn']

    created_zips = None

    def create_zips(self, base=None):
        self.created_zips = {}
        if base is None:
            base = os.path.join(os.path.dirname(__file__), 'pep382')
        for test_dir in os.listdir(base):
            if test_dir in self.ignored_dirs: continue
            source_name = os.path.join(base, test_dir)
            self.create(source_name)

    def create(self, source):
        """
        Create a ziptest from a source directory.
        """
        filename = support.TESTFN + os.path.basename(source) + '.zip'
        self.zip_dir(source, filename)
        self.created_zips[os.path.basename(source)] = filename

    def cleanup_zips(self):
        # use in teardown
        for file in self.created_zips.values():
            os.unlink(file)

    @staticmethod
    def zip_dir(source_dir, archive_name, compression=zipfile.ZIP_DEFLATED):
        """
        Take a source directory and add all of its contents to a zip
        archive called archive_name
        """
        assert os.path.isdir(source_dir)
        with contextlib.closing(zipfile.ZipFile(archive_name, "w", compression)) as z:
            for root, dirs, files in os.walk(source_dir):
                if '.svn' in dirs: dirs.remove('.svn')
                #NOTE: ignore empty directories
                for fn in files:
                    absfn = os.path.join(root, fn)
                    zfn = os.path.relpath(absfn, source_dir)
                    z.write(absfn, zfn)


class PthZipFileTests(PthTestsBase, ZipTester):
    """Test namespace packages that live solely in zip files."""

    def add_to_syspath(self, *items):
        self.create_zips()
        self.addCleanup(self.cleanup_zips)
        return support.DirsOnSysPath(*[self.created_zips[name]
                                       for name in items])


def test_main():
    tests = [PthFilesystemTests, PthZipFileTests]
    support.run_unittest(*tests)


if __name__ == "__main__":
    test_main()
