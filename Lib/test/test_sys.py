# -*- coding: iso-8859-1 -*-
import unittest, test.test_support
import sys, cStringIO, os

class SysModuleTest(unittest.TestCase):

    def test_original_displayhook(self):
        import __builtin__
        savestdout = sys.stdout
        out = cStringIO.StringIO()
        sys.stdout = out

        dh = sys.__displayhook__

        self.assertRaises(TypeError, dh)
        if hasattr(__builtin__, "_"):
            del __builtin__._

        dh(None)
        self.assertEqual(out.getvalue(), "")
        self.assert_(not hasattr(__builtin__, "_"))
        dh(42)
        self.assertEqual(out.getvalue(), "42\n")
        self.assertEqual(__builtin__._, 42)

        del sys.stdout
        self.assertRaises(RuntimeError, dh, 42)

        sys.stdout = savestdout

    def test_lost_displayhook(self):
        olddisplayhook = sys.displayhook
        del sys.displayhook
        code = compile("42", "<string>", "single")
        self.assertRaises(RuntimeError, eval, code)
        sys.displayhook = olddisplayhook

    def test_custom_displayhook(self):
        olddisplayhook = sys.displayhook
        def baddisplayhook(obj):
            raise ValueError
        sys.displayhook = baddisplayhook
        code = compile("42", "<string>", "single")
        self.assertRaises(ValueError, eval, code)
        sys.displayhook = olddisplayhook

    def test_original_excepthook(self):
        savestderr = sys.stderr
        err = cStringIO.StringIO()
        sys.stderr = err

        eh = sys.__excepthook__

        self.assertRaises(TypeError, eh)
        try:
            raise ValueError(42)
        except ValueError, exc:
            eh(*sys.exc_info())

        sys.stderr = savestderr
        self.assert_(err.getvalue().endswith("ValueError: 42\n"))

    # FIXME: testing the code for a lost or replaced excepthook in
    # Python/pythonrun.c::PyErr_PrintEx() is tricky.

    def test_exc_clear(self):
        self.assertRaises(TypeError, sys.exc_clear, 42)

        # Verify that exc_info is present and matches exc, then clear it, and
        # check that it worked.
        def clear_check(exc):
            typ, value, traceback = sys.exc_info()
            self.assert_(typ is not None)
            self.assert_(value is exc)
            self.assert_(traceback is not None)

            sys.exc_clear()

            typ, value, traceback = sys.exc_info()
            self.assert_(typ is None)
            self.assert_(value is None)
            self.assert_(traceback is None)

        def clear():
            try:
                raise ValueError, 42
            except ValueError, exc:
                clear_check(exc)

        # Raise an exception and check that it can be cleared
        clear()

        # Verify that a frame currently handling an exception is
        # unaffected by calling exc_clear in a nested frame.
        try:
            raise ValueError, 13
        except ValueError, exc:
            typ1, value1, traceback1 = sys.exc_info()
            clear()
            typ2, value2, traceback2 = sys.exc_info()

            self.assert_(typ1 is typ2)
            self.assert_(value1 is exc)
            self.assert_(value1 is value2)
            self.assert_(traceback1 is traceback2)

        # Check that an exception can be cleared outside of an except block
        clear_check(exc)

    def test_exit(self):
        self.assertRaises(TypeError, sys.exit, 42, 42)

        # call without argument
        try:
            sys.exit(0)
        except SystemExit, exc:
            self.assertEquals(exc.code, 0)
        except:
            self.fail("wrong exception")
        else:
            self.fail("no exception")

        # call with tuple argument with one entry
        # entry will be unpacked
        try:
            sys.exit(42)
        except SystemExit, exc:
            self.assertEquals(exc.code, 42)
        except:
            self.fail("wrong exception")
        else:
            self.fail("no exception")

        # call with integer argument
        try:
            sys.exit((42,))
        except SystemExit, exc:
            self.assertEquals(exc.code, 42)
        except:
            self.fail("wrong exception")
        else:
            self.fail("no exception")

        # call with string argument
        try:
            sys.exit("exit")
        except SystemExit, exc:
            self.assertEquals(exc.code, "exit")
        except:
            self.fail("wrong exception")
        else:
            self.fail("no exception")

        # call with tuple argument with two entries
        try:
            sys.exit((17, 23))
        except SystemExit, exc:
            self.assertEquals(exc.code, (17, 23))
        except:
            self.fail("wrong exception")
        else:
            self.fail("no exception")

        # test that the exit machinery handles SystemExits properly
        import subprocess
        # both unnormalized...
        rc = subprocess.call([sys.executable, "-c",
                              "raise SystemExit, 46"])
        self.assertEqual(rc, 46)
        # ... and normalized
        rc = subprocess.call([sys.executable, "-c",
                              "raise SystemExit(47)"])
        self.assertEqual(rc, 47)


    def test_getdefaultencoding(self):
        if test.test_support.have_unicode:
            self.assertRaises(TypeError, sys.getdefaultencoding, 42)
            # can't check more than the type, as the user might have changed it
            self.assert_(isinstance(sys.getdefaultencoding(), str))

    # testing sys.settrace() is done in test_trace.py
    # testing sys.setprofile() is done in test_profile.py

    def test_setcheckinterval(self):
        self.assertRaises(TypeError, sys.setcheckinterval)
        orig = sys.getcheckinterval()
        for n in 0, 100, 120, orig: # orig last to restore starting state
            sys.setcheckinterval(n)
            self.assertEquals(sys.getcheckinterval(), n)

    def test_recursionlimit(self):
        self.assertRaises(TypeError, sys.getrecursionlimit, 42)
        oldlimit = sys.getrecursionlimit()
        self.assertRaises(TypeError, sys.setrecursionlimit)
        self.assertRaises(ValueError, sys.setrecursionlimit, -42)
        sys.setrecursionlimit(10000)
        self.assertEqual(sys.getrecursionlimit(), 10000)
        sys.setrecursionlimit(oldlimit)

    def test_getwindowsversion(self):
        if hasattr(sys, "getwindowsversion"):
            v = sys.getwindowsversion()
            self.assert_(isinstance(v, tuple))
            self.assertEqual(len(v), 5)
            self.assert_(isinstance(v[0], int))
            self.assert_(isinstance(v[1], int))
            self.assert_(isinstance(v[2], int))
            self.assert_(isinstance(v[3], int))
            self.assert_(isinstance(v[4], str))

    def test_dlopenflags(self):
        if hasattr(sys, "setdlopenflags"):
            self.assert_(hasattr(sys, "getdlopenflags"))
            self.assertRaises(TypeError, sys.getdlopenflags, 42)
            oldflags = sys.getdlopenflags()
            self.assertRaises(TypeError, sys.setdlopenflags)
            sys.setdlopenflags(oldflags+1)
            self.assertEqual(sys.getdlopenflags(), oldflags+1)
            sys.setdlopenflags(oldflags)

    def test_refcount(self):
        self.assertRaises(TypeError, sys.getrefcount)
        c = sys.getrefcount(None)
        n = None
        self.assertEqual(sys.getrefcount(None), c+1)
        del n
        self.assertEqual(sys.getrefcount(None), c)
        if hasattr(sys, "gettotalrefcount"):
            self.assert_(isinstance(sys.gettotalrefcount(), int))

    def test_getframe(self):
        self.assertRaises(TypeError, sys._getframe, 42, 42)
        self.assertRaises(ValueError, sys._getframe, 2000000000)
        self.assert_(
            SysModuleTest.test_getframe.im_func.func_code \
            is sys._getframe().f_code
        )

    # sys._current_frames() is a CPython-only gimmick.
    def test_current_frames(self):
        have_threads = True
        try:
            import thread
        except ImportError:
            have_threads = False

        if have_threads:
            self.current_frames_with_threads()
        else:
            self.current_frames_without_threads()

    # Test sys._current_frames() in a WITH_THREADS build.
    def current_frames_with_threads(self):
        import threading, thread
        import traceback

        # Spawn a thread that blocks at a known place.  Then the main
        # thread does sys._current_frames(), and verifies that the frames
        # returned make sense.
        entered_g = threading.Event()
        leave_g = threading.Event()
        thread_info = []  # the thread's id

        def f123():
            g456()

        def g456():
            thread_info.append(thread.get_ident())
            entered_g.set()
            leave_g.wait()

        t = threading.Thread(target=f123)
        t.start()
        entered_g.wait()

        # At this point, t has finished its entered_g.set(), although it's
        # impossible to guess whether it's still on that line or has moved on
        # to its leave_g.wait().
        self.assertEqual(len(thread_info), 1)
        thread_id = thread_info[0]

        d = sys._current_frames()

        main_id = thread.get_ident()
        self.assert_(main_id in d)
        self.assert_(thread_id in d)

        # Verify that the captured main-thread frame is _this_ frame.
        frame = d.pop(main_id)
        self.assert_(frame is sys._getframe())

        # Verify that the captured thread frame is blocked in g456, called
        # from f123.  This is a litte tricky, since various bits of
        # threading.py are also in the thread's call stack.
        frame = d.pop(thread_id)
        stack = traceback.extract_stack(frame)
        for i, (filename, lineno, funcname, sourceline) in enumerate(stack):
            if funcname == "f123":
                break
        else:
            self.fail("didn't find f123() on thread's call stack")

        self.assertEqual(sourceline, "g456()")

        # And the next record must be for g456().
        filename, lineno, funcname, sourceline = stack[i+1]
        self.assertEqual(funcname, "g456")
        self.assert_(sourceline in ["leave_g.wait()", "entered_g.set()"])

        # Reap the spawned thread.
        leave_g.set()
        t.join()

    # Test sys._current_frames() when thread support doesn't exist.
    def current_frames_without_threads(self):
        # Not much happens here:  there is only one thread, with artificial
        # "thread id" 0.
        d = sys._current_frames()
        self.assertEqual(len(d), 1)
        self.assert_(0 in d)
        self.assert_(d[0] is sys._getframe())

    def test_attributes(self):
        self.assert_(isinstance(sys.api_version, int))
        self.assert_(isinstance(sys.argv, list))
        self.assert_(sys.byteorder in ("little", "big"))
        self.assert_(isinstance(sys.builtin_module_names, tuple))
        self.assert_(isinstance(sys.copyright, basestring))
        self.assert_(isinstance(sys.exec_prefix, basestring))
        self.assert_(isinstance(sys.executable, basestring))
        self.assertEqual(len(sys.float_info), 11)
        self.assertEqual(sys.float_info.radix, 2)
        self.assert_(isinstance(sys.hexversion, int))
        self.assert_(isinstance(sys.maxint, int))
        if test.test_support.have_unicode:
            self.assert_(isinstance(sys.maxunicode, int))
        self.assert_(isinstance(sys.platform, basestring))
        self.assert_(isinstance(sys.prefix, basestring))
        self.assert_(isinstance(sys.version, basestring))
        vi = sys.version_info
        self.assert_(isinstance(vi, tuple))
        self.assertEqual(len(vi), 5)
        self.assert_(isinstance(vi[0], int))
        self.assert_(isinstance(vi[1], int))
        self.assert_(isinstance(vi[2], int))
        self.assert_(vi[3] in ("alpha", "beta", "candidate", "final"))
        self.assert_(isinstance(vi[4], int))

    def test_43581(self):
        # Can't use sys.stdout, as this is a cStringIO object when
        # the test runs under regrtest.
        self.assert_(sys.__stdout__.encoding == sys.__stderr__.encoding)

    def test_sys_flags(self):
        self.failUnless(sys.flags)
        attrs = ("debug", "py3k_warning", "division_warning", "division_new",
                 "inspect", "interactive", "optimize", "dont_write_bytecode",
                 "no_site", "ignore_environment", "tabcheck", "verbose",
                 "unicode", "bytes_warning")
        for attr in attrs:
            self.assert_(hasattr(sys.flags, attr), attr)
            self.assertEqual(type(getattr(sys.flags, attr)), int, attr)
        self.assert_(repr(sys.flags))

    def test_clear_type_cache(self):
        sys._clear_type_cache()

    def test_compact_freelists(self):
        sys._compact_freelists()
        r = sys._compact_freelists()
##        # freed blocks shouldn't change
##        self.assertEqual(r[0][2], 0)
##        self.assertEqual(r[1][2], 0)
##        # fill freelists
##        ints = list(range(10000))
##        floats = [float(i) for i in ints]
##        del ints
##        del floats
##        # should free more than 200 blocks each
##        r = sys._compact_freelists()
##        self.assert_(r[0][1] > 100, r[0][1])
##        self.assert_(r[1][2] > 100, r[1][1])
##
##        self.assert_(r[0][2] > 100, r[0][2])
##        self.assert_(r[1][2] > 100, r[1][2])

    def test_ioencoding(self):
        import subprocess,os
        env = dict(os.environ)

        # Test character: cent sign, encoded as 0x4A (ASCII J) in CP424,
        # not representable in ASCII.

        env["PYTHONIOENCODING"] = "cp424"
        p = subprocess.Popen([sys.executable, "-c", 'print unichr(0xa2)'],
                             stdout = subprocess.PIPE, env=env)
        out = p.stdout.read().strip()
        self.assertEqual(out, unichr(0xa2).encode("cp424"))

        env["PYTHONIOENCODING"] = "ascii:replace"
        p = subprocess.Popen([sys.executable, "-c", 'print unichr(0xa2)'],
                             stdout = subprocess.PIPE, env=env)
        out = p.stdout.read().strip()
        self.assertEqual(out, '?')


class SizeOfHelper(object):
    "Helper class for sizeof tests"

    def __init__(self, default_n=None):
        """default_n can be used to set the default alignment offset.
        If it is not set, n must be provided on every call of sizeof()."""
        self.default_n = default_n

    def align(self, value, n):
        """ Align value to multiple of n."""
        mod = value % n
        if mod != 0:
            return value - mod + n
        else:
            return value

    def sizeof(self, members, n=None):
        """ Calculate size of a struct in bytes, i.e. simulate sizeof().
        members is the ordered list of members of the struct. n is the
        offset used for the alignment. If n is not set in the constructor
        it must be set here.
        This function is recursive."""
        res = 0
        if n == None:
            n = self.default_n
        if n <=0:
            raise ValueError("n has to be larger than 0")
        if len(members) == 1:
            res = self.align(members[0], n)
            return res
        else:
            if (members[0] % n) == 0:
                # perfect match: add first member and continue with the rest
                res =  members[0] + self.sizeof(members[1:len(members)], n)
                return res
            else:
                # space left, check if next member would still fit
                index = 0
                sum = members[index]
                index += 1
                while (index < len(members)) and ((sum + members[index] <= n)):
                    sum +=  members[index]
                    index += 1
                res = self.align(sum, n)
                if index != len(members):
                    res += self.sizeof(members[index:len(members)], n)
                return res

class SizeOfHelperTest(unittest.TestCase):

    def setUp(self):
        self.sim = SizeOfHelper()

    def test_align(self):
        n = 8
        self.assertEqual(self.sim.align(0, n) % n, 0)
        self.assertEqual(self.sim.align(1, n) % n, 0)
        self.assertEqual(self.sim.align(3, n) % n, 0)
        self.assertEqual(self.sim.align(4, n) % n, 0)
        self.assertEqual(self.sim.align(7, n) % n, 0)
        self.assertEqual(self.sim.align(8, n) % n, 0)
        self.assertEqual(self.sim.align(9, n) % n, 0)

    def test_zero(self):
        members = [0]
        self.assertRaises(ValueError, self.sim.sizeof, members, max(members))

    def test_char(self):
        members = [1]
        self.assertEqual(self.sim.sizeof(members, 1), 1)

    def test_xchar(self):
        import random
        rand = random.randint(2,300)
        members = [1] * rand
        self.assertEqual(self.sim.sizeof(members, 1), rand*1)

    def test_membermerge(self):
        # test the merge of equal size members into one, eg [4, 4, 2]=[2*4, 2]
        members = [2*4, 4]
        self.assertEqual(self.sim.sizeof(members, 4), 12)

    def init_types(self, i, l, p):
        """Initialize types for testing, whereas i=int, l=long, and p=pointer.
        The dict consists of a descriptive name as key and two list items. The
        first is the ordered list of the type members, the second the expected
        value for sizeof. The expected value has to be set in the caller
        afterwards."""
        types = {"3ip": [[i, i, i, p ], -1],\
                 "ip": [[i, p], -1],\
                 "pplpl": [[2*p, l, p, l], -1]}
        return types

    def test_ILP32(self):
        i = l = p = 4
        types = self.init_types(i, l, p)
        types["3ip"][1] = 16
        types["ip"][1] = 8
        types["pplpl"][1] = 20
        for v in types.values():
            self.assertEqual(self.sim.sizeof(v[0], p), v[1])

    def test_LP64(self):
        i = 4
        l = p = 8
        types = self.init_types(i, l, p)
        types["3ip"][1] = 24
        types["ip"][1] = 16
        types["pplpl"][1] = 40
        for v in types.values():
            self.assertEqual(self.sim.sizeof(v[0], p), v[1])

    def test_LLP64(self):
        i = l = 4
        p = 8
        types = self.init_types(i, l, p)
        types["3ip"][1] = 24
        types["ip"][1] = 16
        types["pplpl"][1] = 40
        for v in types.values():
            self.assertEqual(self.sim.sizeof(v[0], p), v[1])

    def test_ILP64(self):
        i = l = p = 8
        types = self.init_types(i, l, p)
        types["3ip"][1] = 32
        types["ip"][1] = 16
        types["pplpl"][1] = 40
        for v in types.values():
            self.assertEqual(self.sim.sizeof(v[0], p), v[1])

class SizeofTest(unittest.TestCase):

    def setUp(self):
        import struct
        # determine size of basic types
        self.i = len(struct.pack('i', 0))
        self.l = len(struct.pack('l', 0))
        self.p = len(struct.pack('P', 0))
        self.header = [self.l, self.p]
        if hasattr(sys, "gettotalrefcount"):
            self.header.extend([2*self.p])
        self.helper = SizeOfHelper(default_n=self.p)
        self.file = open(test.test_support.TESTFN, 'wb')

    def tearDown(self):
        self.file.close()
        test.test_support.unlink(test.test_support.TESTFN)

    def check_sizeof(self, o, size):
        result = sys.getsizeof(o)
        msg = 'wrong size for %s: got %d, expected %d' \
            % (type(o), result, size)
        self.assertEqual(result, size, msg)

    def test_standardtypes(self):
        i = self.i
        l = self.l
        p = self.p
        h = self.header
        size = self.helper.sizeof

        # bool
        self.check_sizeof(True, size(h + [l]))
        # buffer
        self.check_sizeof(buffer(''), size(h + [2*p, 2*l, i, l]))
        # cell
        def get_cell():
            x = 42
            def inner():
                return x
            return inner
        self.check_sizeof(get_cell().func_closure[0], size(h + [p]))
        # old-style class
        class class_oldstyle():
            def method():
                pass
        self.check_sizeof(class_oldstyle, size(h + [6*p]))
        # instance
        self.check_sizeof(class_oldstyle(), size(h + [3*p]))
        # method
        self.check_sizeof(class_oldstyle().method, size(h + [4*p]))
        # code
        self.check_sizeof(get_cell().func_code, size(h +\
                            [4*i, 8*p, i, 2*p]))
        # complex
        self.check_sizeof(complex(0,1), size(h + [2*8]))
        # enumerate
        self.check_sizeof(enumerate([]), size(h + [l, 3*p]))
        # reverse
        self.check_sizeof(reversed(''), size(h + [l, p]))
        # file
        self.check_sizeof(self.file, size(h + [4*p, 2*i, 4*p,\
                            3*i, 3*p, i]))
        # float
        self.check_sizeof(float(0), size(h + [8]))
        # function
        def func(): pass
        self.check_sizeof(func, size(h + [9*l]))
        class c():
            @staticmethod
            def foo():
                pass
            @classmethod
            def bar(cls):
                pass
            # staticmethod
            self.check_sizeof(foo, size(h + [l]))
            # classmethod
            self.check_sizeof(bar, size(h + [l]))
        # generator
        def get_gen(): yield 1
        self.check_sizeof(get_gen(), size(h + [p, i, 2*p]))
        # integer
        self.check_sizeof(1, size(h + [l]))
        # builtin_function_or_method
        self.check_sizeof(abs, size(h + [3*p]))
        # module
        self.check_sizeof(unittest, size(h + [p]))
        # xrange
        self.check_sizeof(xrange(1), size(h + [3*p]))
        # slice
        self.check_sizeof(slice(0), size(h + [3*p]))

        h.append(l)
        # new-style class
        class class_newstyle(object):
            def method():
                pass
        # type (PyTypeObject + PyNumberMethods +  PyMappingMethods +
        #       PySequenceMethods +  PyBufferProcs)
        typeobject_members = [p, 2*l, 15*p, l, 4*p, l, 9*p, l, 11*p]
        self.check_sizeof(class_newstyle, size(h + typeobject_members +\
                              [42*p, 10*p, 3*p, 6*p]))

    def test_specialtypes(self):
        i = self.i
        l = self.l
        p = self.p
        h = self.header
        size = self.helper.sizeof

        # dict
        self.check_sizeof({}, size(h + [3*l, 3*p, 8*size([l, 2*p])]))
        longdict = {1:1, 2:2, 3:3, 4:4, 5:5, 6:6, 7:7, 8:8}
        self.check_sizeof(longdict, size(h + [3*l, 3*p, \
                                     8*size([l, 2*p]), 16*size([l, 2*p])]))
        # list
        self.check_sizeof([], size(h + [l, p, l]))
        self.check_sizeof([1, 2, 3], size(h + [l, p, l, 3*l]))

        h.append(l)
        # long
        self.check_sizeof(0L, size(h + [2]))
        self.check_sizeof(1L, size(h + [2]))
        self.check_sizeof(-1L, size(h + [2]))
        self.check_sizeof(32768L, size(h + [2]) + 2)
        self.check_sizeof(32768L*32768L-1, size(h + [2]) +2)
        self.check_sizeof(32768L*32768L, size(h + [2]) + 4)
        # string
        self.check_sizeof('', size(h + [l, i + 1]))
        self.check_sizeof('abc', size(h + [l, i + 1]) + 3)


def test_main():
    test_classes = (SysModuleTest, SizeOfHelperTest, SizeofTest)

    test.test_support.run_unittest(*test_classes)

if __name__ == "__main__":
    test_main()
