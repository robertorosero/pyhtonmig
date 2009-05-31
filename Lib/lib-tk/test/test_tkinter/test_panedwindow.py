import unittest
import Tkinter
from test.test_support import requires, run_unittest
from ttk import setup_master

requires('gui')

class PanedWindowTest(unittest.TestCase):

    def setUp(self):
        self.root = setup_master()
        self.pwin = Tkinter.PanedWindow(self.root)

    def tearDown(self):
        self.pwin.destroy()


    def test_add(self):
        lbl = Tkinter.Label()
        self.pwin.add(lbl)
        self.assertIn(str(lbl), self.pwin.panes())

    def test_remove(self):
        self.test_add()
        self.pwin.remove(self.pwin.panes()[0])
        self.assertFalse(self.pwin.panes())
        self.test_add()
        self.pwin.forget(self.pwin.panes()[0])
        self.assertFalse(self.pwin.panes())

    def test_identify(self):
        lbl = Tkinter.Label(text='a')
        lbl2 = Tkinter.Label(text='b')
        self.pwin['orient'] = 'horizontal'
        self.pwin['showhandle'] = True
        self.pwin.add(lbl)
        self.pwin.add(lbl2)
        self.pwin.pack()
        self.pwin.update_idletasks()

        to_find = ['sash', 'handle']
        for i in xrange(self.pwin.winfo_width()):
            for j in xrange(self.pwin.winfo_height()):
                res = self.pwin.identify(i, j)
                if res:
                    self.assertTrue(isinstance(res, tuple))
                    self.assertTrue(isinstance(res[0], int))
                    self.assertTrue(isinstance(res[1], str))
                    if res[1] in to_find:
                        to_find.remove(res[1])
                    if not to_find:
                        return

    def test_proxy(self):
        self.pwin.add(Tkinter.Label(text='hi'))
        self.pwin.add(Tkinter.Label(text='hi'))

        self.pwin.proxy_place(10, 10)
        res = self.pwin.proxy_coord()
        self.assertTrue(isinstance(res, tuple))
        self.assertEqual(res[0], 10)
        self.pwin.proxy_forget()

    def test_sash(self): pass

    def test_panecget(self):
        lbl = Tkinter.Label()
        self.pwin.add(lbl, padx=6, pady=3)
        self.assertEqual(self.pwin.panecget(lbl, 'padx'), 6)
        self.assertEqual(self.pwin.panecget(lbl, 'pady'), 3)
        self.assertRaises(Tkinter.TclError, self.pwin.panecget, lbl, 'padz')

    def test_paneconfigure(self):
        lbl = Tkinter.Label()
        self.pwin.add(lbl, padx=6, pady=3)
        cfg = self.pwin.paneconfigure(lbl)
        self.assertEqual(cfg['padx'][-1], 6)
        self.assertEqual(cfg['pady'][-1], 3)
        self.assertTrue(isinstance(cfg, dict))

        self.pwin.paneconfigure(lbl, padx=3)
        self.assertEqual(self.pwin.paneconfigure(lbl)['padx'][-1],
                self.pwin.paneconfigure(lbl)['pady'][-1])
        self.assertRaises(Tkinter.TclError,
                self.pwin.paneconfigure, lbl, padz=3)

        r = self.pwin.paneconfigure(lbl, 'padx')
        self.assertTrue(isinstance(r, tuple))
        self.assertEqual(r[-1], 3)
        self.assertRaises(Tkinter.TclError,
                self.pwin.paneconfigure, lbl, 'padz')

        self.assertEqual(r, self.pwin.paneconfigure(lbl)['padx'])

    def test_panes(self):
        lbl1 = Tkinter.Label()
        lbl2 = Tkinter.Label()
        self.pwin.add(lbl1)
        self.pwin.add(lbl2)
        panes = self.pwin.panes()
        self.assertEqual(panes[0], str(lbl1))
        self.assertEqual(panes[1], str(lbl2))
        self.assertTrue(isinstance(panes, tuple))


tests_gui = (PanedWindowTest, )

if __name__ == "__main__":
    run_unittest(*tests_gui)
