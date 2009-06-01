import unittest
import Tkinter
from test.test_support import requires, run_unittest
from ttk import setup_master

requires('gui')

class SpinboxTest(unittest.TestCase):

    def setUp(self):
        self.root = setup_master()
        self.sb = Tkinter.Spinbox(self.root)

    def tearDown(self):
        self.sb.destroy()


    def test_bbox(self):
        res = self.sb.bbox('end')
        self.assertTrue(isinstance(res, tuple))
        self.assertEqual(len(res), 4)

        self.assertRaises(Tkinter.TclError, self.sb.bbox, '2.0')

    def test_get_delete(self):
        self.sb.insert('0', 'hi there')
        self.sb.delete('0')
        self.assertEqual(self.sb.get(), 'i there')
        self.sb.delete(0, 1)
        self.assertEqual(self.sb.get(), ' there')
        self.sb.delete(0, 'end')
        self.assertEqual(self.sb.get(), '')

    def test_icursor(self):
        self.sb.insert(0, 'avocado')
        self.sb.icursor(3)
        self.sb.delete('insert', 'end')
        self.assertEqual(self.sb.get(), 'avo')

    def test_identify(self):
        self.sb.pack()
        self.sb.update_idletasks()

        # XXX Based on Tk's code it doesn't seem like the identify command
        # in Tcl can return the string 'none'. I also can't make it return
        # this value, so it's more like a documentation bug in tk or even a
        # tk bug that should be tested on tk side.
        self.assertEqual(self.sb.identify(0, 0), 'entry')

        buttons = ['buttonup', 'buttondown']
        for i in xrange(self.sb.winfo_width(), 0, -1):
            name = self.sb.identify(i, 1)
            if name in buttons:
                buttons.remove(name)
                break
        for j in xrange(self.sb.winfo_height(), 0, -1):
            name = self.sb.identify(i, j)
            if name in buttons:
                buttons.remove(name)
                break
        self.assertFalse(buttons)

    def test_index(self):
        self.assertEqual(self.sb.index('end'), 0)
        self.sb.insert(0, 'airplane')
        self.assertEqual(self.sb.index('end'), 8)

    def test_invoke(self):
        self.sb.configure(from_=1.0, to=10.0, increment=0.5)

        val = self.sb.get()
        self.sb.invoke('buttondown')
        self.assertEqual(self.sb.get(), val)
        self.sb.invoke('buttonup')
        self.assertEqual(self.sb.get(), str(float(val) + 0.5))

    def test_scan(self): pass
    def test_selection(self): pass

    # XXX Tkinter.Spinbox is missing set, validate, xview and some
    # selection methods.


tests_gui = (SpinboxTest, )

if __name__ == "__main__":
    run_unittest(*tests_gui)
