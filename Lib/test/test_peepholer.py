import dis
import sys
from cStringIO import StringIO
import unittest

def disassemble(func):
    f = StringIO()
    tmp = sys.stdout
    sys.stdout = f
    dis.dis(func)
    sys.stdout = tmp
    result = f.getvalue()
    f.close()
    return result

def dis_single(line):
    return disassemble(compile(line, '', 'single'))

class TestTranforms(unittest.TestCase):

    def test_elim_inversion_of_is_or_in(self):
        for line, elem in (
            ('not a is b', '(is not)',),
            ('not a in b', '(not in)',),
            ('not a is not b', '(is)',),
            ('not a not in b', '(in)',),
            ):
            asm = dis_single(line)
            self.assert_(elem in asm)

    def test_while_one(self):
        # Skip over:  LOAD_CONST trueconst  JUMP_IF_FALSE xx  POP_TOP
        def f():
            while 1:
                pass
            return list
        asm = disassemble(f)
        for elem in ('LOAD_CONST', 'JUMP_IF_FALSE'):
            self.assert_(elem not in asm)
        for elem in ('JUMP_ABSOLUTE',):
            self.assert_(elem in asm)

    def test_pack_unpack(self):
        for line, elem in (
            ('a, = a,', 'LOAD_CONST',),
            ('a, b = a, b', 'ROT_TWO',),
            ('a, b, c = a, b, c', 'ROT_THREE',),
            ):
            asm = dis_single(line)
            self.assert_(elem in asm)
            self.assert_('BUILD_TUPLE' not in asm)
            self.assert_('UNPACK_TUPLE' not in asm)

    def test_elim_jump_to_return(self):
        # JUMP_FORWARD to RETURN -->  RETURN
        def f(cond, true_value, false_value):
            return true_value if cond else false_value
        asm = disassemble(f)
        self.assert_('JUMP_FORWARD' not in asm)
        self.assert_('JUMP_ABSOLUTE' not in asm)
        self.assertEqual(asm.split().count('RETURN_VALUE'), 2)

def test_main(verbose=None):
    import sys
    from test import test_support
    test_classes = (TestTranforms,)
    test_support.run_unittest(*test_classes)

    # verify reference counting
    if verbose and hasattr(sys, "gettotalrefcount"):
        import gc
        counts = [None] * 5
        for i in xrange(len(counts)):
            test_support.run_unittest(*test_classes)
            gc.collect()
            counts[i] = sys.gettotalrefcount()
        print counts

if __name__ == "__main__":
    test_main(verbose=True)
