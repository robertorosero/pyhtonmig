import unittest
import Tkinter
from test.test_support import requires, run_unittest
from ttk import setup_master

requires('gui')

class EntryTest(unittest.TestCase):

    def setUp(self):
        self.root = setup_master()
        self.entry = Tkinter.Entry(self.root)

    def tearDown(self):
        self.entry.destroy()


    def test_delete(self):
        self.entry.insert('end', 'avocado')
        self.entry.delete(0)
        self.assertEqual(self.entry.get(), 'vocado')

        # verify that the last index isn't removed by delete.
        self.entry.delete(0, 1)
        self.assertEqual(self.entry.get(), 'ocado')

        self.entry.delete(2, 0)
        self.assertEqual(self.entry.get(), 'ocado')
        self.assertRaises(Tkinter.TclError, self.entry.delete, 'last')

    def test_get_insert(self):
        self.assertEqual(self.entry.get(), '')
        self.assertIs(self.entry.insert('end', 'hi'), None)
        self.assertEqual(self.entry.get(), 'hi')
        self.entry.insert(1, 'ea')
        self.assertEqual(self.entry.get(), 'heai')

        self.entry.delete(0, 'end')
        self.entry.insert(0, u'\u1234')
        x = self.entry.get()
        self.assertEqual(x, u'\u1234')
        self.assertEqual(len(x), 1)

    def test_icursor(self):
        self.assertEqual(self.entry.index('insert'), 0)
        self.assertIs(self.entry.icursor(2), None)
        self.assertEqual(self.entry.index('insert'), 0)

        self.entry.insert(0, 'testing')
        self.entry.icursor(2)
        self.assertEqual(self.entry.index('insert'), 2)

    def test_index(self):
        i = self.entry.index(0)
        self.assertEqual(i, 0)
        i = self.entry.index('0')
        self.assertEqual(i, 0)
        self.entry.insert('end', '.. abc ..')
        self.assertTrue(isinstance(self.entry.index('end'), int))

    def test_scan(self): pass

    def test_selection(self):
        self.entry.insert(0, 'airplane')
        self.assertFalse(self.entry.selection_present())
        self.entry.selection_range(0, 1)
        self.assertTrue(self.entry.selection_present())
        self.assertEqual(self.entry.index('sel.first'), 0)
        self.assertEqual(self.entry.index('sel.last'), 1)
        self.assertIs(self.entry.selection_clear(), None)
        # sel.last doesn't exist now.
        self.assertRaises(Tkinter.TclError, self.entry.index, 'sel.last')

    def test_xview(self):
        view = self.entry.xview()
        self.assertTrue(isinstance(view, tuple))
        self.assertEqual(len(view), 2)
        for item in view:
            self.assertTrue(isinstance(item, float))


tests_gui = (EntryTest, )

if __name__ == "__main__":
    run_unittest(*tests_gui)
