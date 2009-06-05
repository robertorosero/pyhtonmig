import unittest
import Tkinter
from test.test_support import requires, run_unittest
from ttk import setup_master

requires('gui')

class CanvasTest(unittest.TestCase):

    def setUp(self):
        self.root = setup_master()
        self.canvas = Tkinter.Canvas(self.root)

    def tearDown(self):
        self.canvas.destroy()


    def test_addtag(self): pass

    def test_bbox(self):
        self.assertIs(self.canvas.bbox('a'), None)
        self.assertRaises(Tkinter.TclError, self.canvas.bbox)

        self.canvas.create_line(1, 1, 10, 10, tags='a')
        self.canvas.create_line(5, 5, 12, 12, tags='b')
        res = self.canvas.bbox('a', 'b')
        self.assertTrue(isinstance(res, tuple))
        self.assertEqual(len(res), 4)
        for item in res:
            self.assertTrue(isinstance(item, int))

        ba = self.canvas.bbox('a')
        bb = self.canvas.bbox('b')
        for indx, item in enumerate(zip(ba[:2], bb[:2])):
            self.assertEqual(min(item), res[indx])
        for indx, item in enumerate(zip(ba[2:], bb[2:])):
            self.assertEqual(max(item), res[indx + 2])

    def test_tag(self): pass
    def test_canvasxy(self): pass

    def test_coords(self):
        coords = self.canvas.coords('x')
        self.assertFalse(coords)
        self.assertEqual(coords, [])

        item_id = self.canvas.create_line(5, 5, 10, 10)
        coords = self.canvas.coords(item_id)
        self.assertTrue(isinstance(coords, list))
        self.assertEqual(len(coords), 4)
        for item in coords:
            self.assertTrue(isinstance(item, float))

        t = (1, 2, 3, 4)
        self.canvas.coords(item_id, 1, 2, 3, 4)
        for item in zip(self.canvas.coords(item_id), t):
            self.assertAlmostEqual(item[0], item[1])

        self.canvas.coords(item_id, t)
        for item in zip(self.canvas.coords(item_id), t):
            self.assertAlmostEqual(item[0], item[1])

    def test_create(self):
        self.assertIs(self.canvas.type('x'), None)
        self.assertRaises(Tkinter.TclError, self.canvas.create_arc)
        self.assertRaises(Tkinter.TclError, self.canvas.create_arc,
                1, 2, 3, 4, badoption=2)

        arcid = self.canvas.create_arc(1, 2, 3, 4, tags=('a', 'b', 'c'))
        self.assertEqual(self.canvas.type(arcid), 'arc')
        self.assertEqual(self.canvas.gettags(arcid), ('a', 'b', 'c'))

        bmpid = self.canvas.create_bitmap(1, 2, state='hidden', anchor='n')
        self.assertEqual(self.canvas.type(bmpid), 'bitmap')
        self.assertEqual(self.canvas.itemcget(bmpid, 'state'), 'hidden')
        self.assertEqual(self.canvas.itemcget(bmpid, 'anchor'), 'n')

        imgid = self.canvas.create_image(1, 2)
        self.assertEqual(self.canvas.type(imgid), 'image')
        # line needs at least 4 coordinates
        self.assertRaises(Tkinter.TclError, self.canvas.create_line, 1, 10)
        lineid = self.canvas.create_line(1, 10, 30, 10, 60, 30,
                width=3, smooth=True)
        self.assertEqual(self.canvas.type(lineid), 'line')
        ovalid = self.canvas.create_oval(1, 2, 3, 4)
        self.assertEqual(self.canvas.type(ovalid), 'oval')
        rectid = self.canvas.create_rectangle(1, 2, 3, 4)
        self.assertEqual(self.canvas.type(rectid), 'rectangle')
        textid = self.canvas.create_text(1, 2)
        self.assertEqual(self.canvas.type(textid), 'text')
        winid = self.canvas.create_window(1, 2)
        self.assertEqual(self.canvas.type(winid), 'window')

        for key, val in locals().iteritems():
            if key.endswith('id'):
                self.assertTrue(isinstance(val, int))

    def test_dchars(self):
        tid = self.canvas.create_text(10, 10, text='testing', tags='t')
        self.canvas.dchars('t', 4)
        self.assertEqual(self.canvas.itemcget(tid, 'text'), 'testng')
        self.assertRaises(Tkinter.TclError, self.canvas.dchars, 't', 'badindex')
        self.canvas.dchars('t', 4, 'end')
        self.assertEqual(self.canvas.itemcget(tid, 'text'), 'test')

        lid = self.canvas.create_line(10, 10, 20, 20, tags='t')
        self.assertEqual(self.canvas.coords(lid), [10, 10, 20, 20])
        self.canvas.dchars('t', 2, 3)
        self.assertEqual(self.canvas.coords(lid), [10, 10])
        self.assertEqual(self.canvas.itemcget(tid, 'text'), 'te')

        self.canvas.dchars('t', 0, 'end')
        self.assertEqual(self.canvas.coords(lid), [])
        self.assertEqual(self.canvas.itemcget(tid, 'text'), '')

    def test_delete(self):
        t1 = self.canvas.create_text(5, 5, tags='a')
        t2 = self.canvas.create_text(5, 5, tags='a')
        t3 = self.canvas.create_text(20, 20, tags='a')
        self.assertTrue(self.canvas.itemconfigure(t1), {})
        self.canvas.delete(t1)
        self.assertEqual(self.canvas.itemconfigure(t1), {})

        t4 = self.canvas.create_text(15, 15, tags='b')
        self.assertTrue(self.canvas.itemconfigure(t2))
        self.canvas.delete('b', 'a', t3)
        self.assertEqual(self.canvas.itemconfigure(t2),
                self.canvas.itemconfigure(t3))
        self.assertEqual(self.canvas.itemconfigure(t3),
                self.canvas.itemconfigure(t4))
        self.assertEqual(self.canvas.itemconfigure(t4), {})

    def test_dtag(self): pass
    def test_find(self): pass
    def test_focus(self): pass
    def test_gettags(self): pass
    def test_icursor(self): pass
    def test_index(self): pass
    def test_insert(self): pass
    def test_itemcget(self): pass
    def test_itemconfigure(self): pass
    def test_move(self): pass
    def test_postscript(self): pass # XXX
    def test_scale(self): pass
    def test_scan(self): pass
    def test_select(self): pass

    def test_xview(self, method='xview'):
        view = getattr(self.canvas, method)()
        self.assertTrue(isinstance(view, tuple))
        self.assertEqual(len(view), 2)
        for item in view:
            self.assertTrue(isinstance(item, float))

    def test_yview(self):
        self.test_xview('yview')


tests_gui = (CanvasTest, )

if __name__ == "__main__":
    run_unittest(*tests_gui)
