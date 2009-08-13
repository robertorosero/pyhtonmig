'''
   Test cases for pyclbr.py
   Nick Mathewson
'''
import os
import sys
import pyclbr
import shutil
from types import ClassType, FunctionType, MethodType, BuiltinFunctionType
from unittest import TestCase
from functools import partial
from test.test_support import TESTFN, run_unittest

StaticMethodType = type(staticmethod(lambda: None))
ClassMethodType = type(classmethod(lambda c: None))

# This next line triggers an error on old versions of pyclbr.

from commands import getstatus

# Here we test the python class browser code.
#
# The main function in this suite, 'testModule', compares the output
# of pyclbr with the introspected members of a module.  Because pyclbr
# is imperfect (as designed), testModule is called with a set of
# members to ignore.

class PyclbrTest(TestCase):

    def assertListEq(self, l1, l2, ignore):
        ''' succeed iff {l1} - {ignore} == {l2} - {ignore} '''
        missing = (set(l1) ^ set(l2)) - set(ignore)
        if missing:
            print >>sys.stderr, "l1=%r\nl2=%r\nignore=%r" % (l1, l2, ignore)
            self.fail("%r missing" % missing.pop())

    def assertHasattr(self, obj, attr, ignore):
        ''' succeed iff hasattr(obj,attr) or attr in ignore. '''
        if attr in ignore: return
        if not hasattr(obj, attr): print "???", attr
        self.assertTrue(hasattr(obj, attr),
                        'expected hasattr(%r, %r)' % (obj, attr))


    def assertHaskey(self, obj, key, ignore):
        ''' succeed iff obj.has_key(key) or key in ignore. '''
        if key in ignore: return
        if not obj.has_key(key):
            print >>sys.stderr, "***",key
        self.assertTrue(obj.has_key(key))

    def assertEqualsOrIgnored(self, a, b, ignore):
        ''' succeed iff a == b or a in ignore or b in ignore '''
        if a not in ignore and b not in ignore:
            self.assertEquals(a, b)

    def checkModule(self, moduleName, module=None, ignore=()):
        ''' succeed iff pyclbr.readmodule_ex(modulename) corresponds
            to the actual module object, module.  Any identifiers in
            ignore are ignored.   If no module is provided, the appropriate
            module is loaded with __import__.'''

        if module is None:
            # Import it.
            # ('<silly>' is to work around an API silliness in __import__)
            module = __import__(moduleName, globals(), {}, ['<silly>'])

        dict = pyclbr.readmodule_ex(moduleName)

        def ismethod(oclass, obj, name):
            classdict = oclass.__dict__
            if isinstance(obj, FunctionType):
                if not isinstance(classdict[name], StaticMethodType):
                    return False
            else:
                if not  isinstance(obj, MethodType):
                    return False
                if obj.im_self is not None:
                    if (not isinstance(classdict[name], ClassMethodType) or
                        obj.im_self is not oclass):
                        return False
                else:
                    if not isinstance(classdict[name], FunctionType):
                        return False

            objname = obj.__name__
            if objname.startswith("__") and not objname.endswith("__"):
                objname = "_%s%s" % (obj.im_class.__name__, objname)
            return objname == name

        # Make sure the toplevel functions and classes are the same.
        for name, value in dict.items():
            if name in ignore:
                continue
            self.assertHasattr(module, name, ignore)
            py_item = getattr(module, name)
            if isinstance(value, pyclbr.Function):
                self.assertTrue(isinstance(py_item, (FunctionType, BuiltinFunctionType)))
                if py_item.__module__ != moduleName:
                    continue   # skip functions that came from somewhere else
                self.assertEquals(py_item.__module__, value.module)
            else:
                self.assertTrue(isinstance(py_item, (ClassType, type)))
                if py_item.__module__ != moduleName:
                    continue   # skip classes that came from somewhere else

                real_bases = [base.__name__ for base in py_item.__bases__]
                pyclbr_bases = [ getattr(base, 'name', base)
                                 for base in value.super ]

                try:
                    self.assertListEq(real_bases, pyclbr_bases, ignore)
                except:
                    print >>sys.stderr, "class=%s" % py_item
                    raise

                actualMethods = []
                for m in py_item.__dict__.keys():
                    if ismethod(py_item, getattr(py_item, m), m):
                        actualMethods.append(m)
                foundMethods = []
                for m in value.methods.keys():
                    if m[:2] == '__' and m[-2:] != '__':
                        foundMethods.append('_'+name+m)
                    else:
                        foundMethods.append(m)

                try:
                    self.assertListEq(foundMethods, actualMethods, ignore)
                    self.assertEquals(py_item.__module__, value.module)

                    self.assertEqualsOrIgnored(py_item.__name__, value.name,
                                               ignore)
                    # can't check file or lineno
                except:
                    print >>sys.stderr, "class=%s" % py_item
                    raise

        # Now check for missing stuff.
        def defined_in(item, module):
            if isinstance(item, ClassType):
                return item.__module__ == module.__name__
            if isinstance(item, FunctionType):
                return item.func_globals is module.__dict__
            return False
        for name in dir(module):
            item = getattr(module, name)
            if isinstance(item,  (ClassType, FunctionType)):
                if defined_in(item, module):
                    self.assertHaskey(dict, name, ignore)

    def test_easy(self):
        self.checkModule('pyclbr')
        self.checkModule('doctest', ignore=("DocTestCase",))
        self.checkModule('rfc822')
        self.checkModule('difflib')

    def test_decorators(self):
        # XXX: See comment in pyclbr_input.py for a test that would fail
        #      if it were not commented out.
        #
        self.checkModule('test.pyclbr_input')

    def test_nested(self):

        def clbr_from_tuple(t, store, parent=None, lineno=1):
            '''Create pyclbr objects from the given tuple t.'''
            name = t[0]
            obj = pickp(name)
            if parent is not None:
                store = store[parent].objects
            ob_name = name.split()[1]
            store[ob_name] = obj(name=ob_name, lineno=lineno, parent=parent)
            parent = ob_name

            for item in t[1:]:
                lineno += 1
                if isinstance(item, str):
                    obj = pickp(item)
                    ob_name = item.split()[1]
                    store[parent].objects[ob_name] = obj(
                            name=ob_name, lineno=lineno, parent=parent)
                else:
                    lineno = clbr_from_tuple(item, store, parent, lineno)

            return lineno

        def tuple_to_py(t, output, indent=0):
            '''Write python code to output according to the given tuple.'''
            name = t[0]
            output.write('%s%s():' % (' ' * indent, name))
            indent += 2

            if not t[1:]:
                output.write(' pass')
            output.write('\n')

            for item in t[1:]:
                if isinstance(item, str):
                    output.write('%s%s(): pass\n' % (' ' * indent, item))
                else:
                    tuple_to_py(item, output, indent)

        # Nested "thing" to test.
        sample = (
                ("class A",
                    ("class B",
                        "def a"),
                    "def b"),
                ("def c",
                    ("def d",
                        ("class C",
                            "def e")
                        ),
                    "def f")
                )

        pclass = partial(pyclbr.Class, module=None, file=None, super=None)
        pfunc = partial(pyclbr.Function, module=None, file=None)
        pickp = lambda name: pclass if name.startswith('class') else pfunc

        # Create a module for storing the Python code.
        dirname = os.path.abspath(TESTFN)
        modname = 'notsupposedtoexist'
        fname = os.path.join(dirname, modname) + os.extsep + 'py'
        os.mkdir(dirname)
        orig_syspath = list(sys.path)
        sys.path.insert(0, dirname)

        # Create pyclbr objects from the sample above, and also convert
        # the same sample above to Python code and write it to fname.
        d = {}
        lineno = 1
        with open(fname, 'w') as output:
            for t in sample:
                newlineno = clbr_from_tuple(t, d, lineno=lineno)
                lineno = newlineno + 1
                tuple_to_py(t, output)

        # Get the data returned by readmodule_ex to compare against
        # our generated data.
        try:
            d_cmp = pyclbr.readmodule_ex(modname)
        finally:
            sys.path[:] = orig_syspath
            try:
                os.remove(fname)
                shutil.rmtree(dirname)
            except OSError:
                pass

        # Finally perform the tests.
        def check_objects(ob1, ob2):
            self.assertEqual(ob1.lineno, ob2.lineno)
            if ob1.parent is None:
                self.assertIsNone(ob2.parent)
            else:
                # ob1 must come from our generated data since the parent
                # attribute is always a string, while the ob2 must come
                # from pyclbr which is always an Object instance.
                self.assertEqual(ob1.parent, ob2.parent.name)
            self.assertEqual(
                    ob1.__class__.__name__,
                    ob2.__class__.__name__)
            self.assertEqual(ob1.objects.keys(), ob2.objects.keys())
            for name, obj in ob1.objects.items():
                obj_cmp = ob2.objects.pop(name)
                del ob1.objects[name]
                check_objects(obj, obj_cmp)

        objs = []
        for name, obj in d.items():
            self.assertIn(name, d_cmp)
            obj_cmp = d_cmp.pop(name)
            del d[name]
            check_objects(obj, obj_cmp)
            self.assertFalse(obj.objects)
            self.assertFalse(obj_cmp.objects)
        self.assertFalse(d)
        self.assertFalse(d_cmp)

    def test_others(self):
        cm = self.checkModule

        # These were once about the 10 longest modules
        cm('random', ignore=('Random',))  # from _random import Random as CoreGenerator
        cm('cgi', ignore=('log',))      # set with = in module
        cm('urllib', ignore=('_CFNumberToInt32',
                             '_CStringFromCFString',
                             '_CFSetup',
                             'getproxies_registry',
                             'proxy_bypass_registry',
                             'proxy_bypass_macosx_sysconf',
                             'open_https',
                             'getproxies_macosx_sysconf',
                             'getproxies_internetconfig',)) # not on all platforms
        cm('pickle')
        cm('aifc', ignore=('openfp',))  # set with = in module
        cm('Cookie')
        cm('sre_parse', ignore=('dump',)) # from sre_constants import *
        cm('pdb')
        cm('pydoc')

        # Tests for modules inside packages
        cm('email.parser')
        cm('test.test_pyclbr')


def test_main():
    run_unittest(PyclbrTest)


if __name__ == "__main__":
    test_main()
