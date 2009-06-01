import unittest
import Tkinter
from test.test_support import run_unittest
from ttk import setup_master

class VariableTest(unittest.TestCase):

    var_class = Tkinter.Variable

    def setUp(self):
        self.root = setup_master(useTk=False)
        self.var = self.var_class(self.root)

    def tearDown(self):
        del self.var


    def test__str__(self):
        name = 'test123'
        myvar = Tkinter.Variable(self.root, name=name)
        self.assertEqual(str(myvar), name)
        del myvar
        self.assertRaises(Tkinter.TclError,
                self.root.tk.globalgetvar, name)

    def test_get_set(self):
        self.assertEqual(self.var.get(), self.var_class._default)
        self.var.set(1)
        self.assertEqual(self.var.get(), 1)
        self.var.set('1')
        self.assertEqual(self.var.get(), '1')

    def test_trace(self):
        self.assertFalse(self.var.trace_vinfo())

        read = [False]
        def test_read(name1, name2, op):
            self.assertEqual(op, 'r')
            read.pop()
        cb1 = self.var.trace_variable('r', test_read)
        self.var.get()
        self.assertFalse(read)

        read_vinfo = self.var.trace_vinfo()

        self.assertEqual(len(read_vinfo), 1)
        cb2 = self.var.trace_variable('w', test_read)
        self.assertEqual(len(self.var.trace_vinfo()), 2)

        self.var.trace_vdelete('w', cb2)
        self.assertEqual(self.var.trace_vinfo(), read_vinfo)
        self.assertNotIn(cb2, self.var._master._tclCommands)
        self.var.trace_vdelete('r', cb1)
        self.assertNotIn(cb1, self.var._master._tclCommands)


class StringVarTest(VariableTest):

    var_class = Tkinter.StringVar

    def test_get_set(self):
        self.assertEqual(self.var.get(), self.var_class._default)
        self.var.set(1)
        self.assertEqual(self.var.get(), '1')
        self.var.set('1')
        self.assertEqual(self.var.get(), '1')


class IntVarTest(VariableTest):

    var_class = Tkinter.IntVar

    def test_get_set(self):
        self.assertEqual(self.var.get(), self.var_class._default)
        self.var.set(1)
        self.assertEqual(self.var.get(), 1)
        self.var.set('1')
        self.assertEqual(self.var.get(), 1)
        self.var.set(True)
        self.assertEqual(self.var.get(), int(True))

        self.var.set([1, 2, 3])
        self.assertRaises(ValueError, self.var.get)


class DoubleVarTest(VariableTest):

    var_class = Tkinter.DoubleVar

    def test_get_set(self):
        self.assertEqual(self.var.get(), self.var_class._default)
        self.var.set(1.1)
        self.assertEqual(self.var.get(), 1.1)
        self.var.set('1.32')
        self.assertEqual(self.var.get(), 1.32)

        self.var.set({})
        self.assertRaises(ValueError, self.var.get)


class BooleanVarTest(VariableTest):

    var_class = Tkinter.BooleanVar

    def test_get_set(self):
        self.assertEqual(self.var.get(), self.var_class._default)
        for val in ['on', 1, True, '1', 'true']:
            self.var.set(val)
            self.assertEqual(self.var.get(), True)
        for val in ['off', 0, False, '0', 'false']:
            self.var.set(val)
            self.assertEqual(self.var.get(), False)

        self.var.set({})
        self.assertRaises(Tkinter.TclError, self.var.get)


tests_nogui = (VariableTest, StringVarTest, IntVarTest,
        DoubleVarTest, BooleanVarTest)

if __name__ == "__main__":
    run_unittest(*tests_nogui)
