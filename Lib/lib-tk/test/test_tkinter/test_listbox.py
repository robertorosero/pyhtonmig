import unittest
import Tkinter
from test.test_support import requires, run_unittest
from ttk import setup_master

requires('gui')

class ListboxTest(unittest.TestCase):

    def setUp(self):
        self.root = setup_master()
        self.lb = Tkinter.Listbox(self.root)

    def tearDown(self):
        self.lb.destroy()

    def test_activate(self): pass

    def test_bbox(self):
        self.assertRaises(Tkinter.TclError, self.lb.bbox, '@1.2')
        self.assertIs(self.lb.bbox(0), None)

        self.lb.insert(0, 'hi')
        bbox = self.lb.bbox(0)
        self.assertTrue(isinstance(bbox, tuple))
        self.assertEqual(len(bbox), 4)
        for item in bbox:
            self.assertTrue(isinstance(item, int))

        self.assertEqual(self.lb.bbox(('0',)), self.lb.bbox(0))
        self.assertEqual(self.lb.bbox(0), self.lb.bbox('0'))

    def test_curselection(self):
        self.assertEqual(self.lb.curselection(), ())
        self.lb.insert(0, *[i for i in range(15)])
        self.lb.selection_set(0, 9)
        indices = self.lb.curselection()
        self.assertTrue(isinstance(indices, tuple))
        for item in indices:
            self.assertTrue(isinstance(item, int))

    def test_delete(self):
        self.assertRaises(Tkinter.TclError, self.lb.delete, '@1.2')

        self.lb.insert(0, 1, '2', 3)
        self.lb.delete(0)
        self.assertEqual(self.lb.get(0, 'end'), ('2', 3))
        self.lb.delete(0, 'end')
        self.assertEqual(self.lb.get(0, 'end'), ())

    def test_get(self):
        self.assertFalse(self.lb.get(0))
        self.lb.insert(0, 'a', 'b', 'c d e')
        self.assertEqual(self.lb.get(2), 'c d e')
        self.assertEqual(self.lb.get(1, 'end'), ('b', 'c d e'))
        self.assertEqual(self.lb.get(3, 2), ())
        self.assertEqual(self.lb.get(2, 0), ())
        # 'last' is not a valid index in listbox
        self.assertRaises(Tkinter.TclError, self.lb.get, 'last')

    def test_index(self):
        self.assertEqual(self.lb.index('end'), 0)
        self.lb.insert(0, *range(5))
        self.assertEqual(self.lb.index('end'), 5)
        self.assertEqual(self.lb.index('2'), 2)
        # XXX how can I get a None using the index method ?

    def test_nearest(self): pass
    def test_scan(self): pass

    def test_see(self):
        self.lb['height'] = 5
        self.lb.insert(0, *range(7))
        self.lb.pack()
        self.lb.update_idletasks()

        self.assertEqual(self.lb.index('@5,%d' % (self.lb.winfo_height() - 5)),
                4)
        self.lb.see('end')
        self.assertEqual(self.lb.index('@5,%d' % (self.lb.winfo_height() - 5)),
                6)

    def test_selection(self):
        self.lb.insert(0, *range(20))

        # Verifying that selection_set includes the end point.
        self.assertFalse(self.lb.select_includes(0))
        self.assertFalse(self.lb.select_includes(5))
        self.lb.selection_set(0, 5)
        self.assertTrue(self.lb.select_includes(0))
        self.assertTrue(self.lb.select_includes(5))
        self.assertEqual(self.lb.curselection(), tuple(range(6)))

        # Verifying that setting a new selection doesn't remove the old
        # selection.
        self.lb.selection_set(7, 9)
        self.assertEqual(self.lb.curselection(), (0, 1, 2, 3, 4, 5, 7, 8, 9))
        self.lb.selection_set(6)
        self.assertEqual(self.lb.curselection(), (0, 1, 2, 3, 4, 5, 6, 7, 8, 9))
        self.lb.selection_set(3, 6)
        self.assertEqual(self.lb.curselection(), (0, 1, 2, 3, 4, 5, 6, 7, 8, 9))

        # Verifying that selection_clear includes the end point.
        self.assertTrue(self.lb.selection_includes(0))
        self.assertTrue(self.lb.selection_includes(5))
        self.lb.selection_clear(0, 5)
        self.assertFalse(self.lb.selection_includes(0))
        self.assertFalse(self.lb.selection_includes(5))
        self.assertEqual(self.lb.curselection(), (6, 7, 8, 9))
        self.lb.selection_clear(7)
        self.assertEqual(self.lb.curselection(), (6, 8, 9))
        self.lb.selection_clear(0, 'end')
        self.assertEqual(self.lb.curselection(), ())

    def test_xview(self, method='xview'):
        view = getattr(self.lb, method)()
        self.assertTrue(isinstance(view, tuple))
        self.assertEqual(len(view), 2)
        for item in view:
            self.assertTrue(isinstance(item, float))

    def test_yview(self):
        self.test_xview('yview')

    def test_itemcget(self):
        self.lb.insert(0, 0)
        self.lb.itemconfigure(0, bg='blue')
        self.assertEqual(self.lb.itemcget(0, 'bg'), 'blue')
        # invalid index:
        self.assertRaises(Tkinter.TclError, self.lb.itemcget, 2, 'bg')
        # invalid option:
        self.assertRaises(Tkinter.TclError, self.lb.itemcget, 0, 'hi')

    def test_itemconfigure(self):
        self.lb.insert(0, 0)
        self.lb.itemconfigure(0, background='blue')
        self.assertTrue(isinstance(self.lb.itemconfigure(0), dict))
        self.assertEqual(self.lb.itemconfigure(0, 'background'),
                self.lb.itemconfigure(0)['background'])

        self.assertRaises(Tkinter.TclError, self.lb.itemconfigure, 0, 'hi')
        self.assertRaises(Tkinter.TclError, self.lb.itemconfigure, 2)


tests_gui = (ListboxTest, )

if __name__ == "__main__":
    run_unittest(*tests_gui)
