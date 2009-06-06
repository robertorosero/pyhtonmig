import unittest
import Tkinter
from test.test_support import requires, run_unittest
from ttk import setup_master

requires('gui')

class ToplevelTest(unittest.TestCase):

    def setUp(self):
        self.root = setup_master()


    def test_initialization(self):
        self.root.title('hi')
        tl = Tkinter.Toplevel(self.root,
                **{'class_': 'test', 'colormap': 'new'})
        self.assertEqual(tl['class'], 'test')
        self.assertEqual(tl.title(), 'hi')
        tl.destroy()

        self.assertRaises(Tkinter.TclError, Tkinter.Toplevel, self.root, a='b')


tests_gui = (ToplevelTest, )

if __name__ == "__main__":
    run_unittest(*tests_gui)
