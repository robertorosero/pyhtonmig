import unittest
import Tkinter
from test.test_support import requires, run_unittest
from ttk import setup_master

requires('gui')

class TextTest(unittest.TestCase):

    def setUp(self):
        self.root = setup_master()
        self.text = Tkinter.Text(self.root)

    def tearDown(self):
        self.text.destroy()


    def test_bbox(self):
        self.text.insert('1.0', 'o')
        bbox = self.text.bbox('1.0')
        self.assertTrue(isinstance(bbox, tuple))
        self.assertEqual(len(bbox), 4)

        self.assertIs(self.text.bbox('2.0'), None)

        bbox = self.text.bbox(('1.0', '-1c', '+1c'))
        self.assertTrue(isinstance(bbox, tuple))
        self.assertEqual(len(bbox), 4)
        self.assertEqual(bbox, self.text.bbox('1.0 -1c +1c'))

        # Invalid index.
        self.assertRaises(Tkinter.TclError, self.text.bbox, '1,0')

        # The following used to raise Tkinter.TclError since text.bbox allowed
        # passing multiple args.
        self.assertRaises(TypeError, self.text.bbox, '1.0', '-1c')

    def test_compare(self):
        self.text.insert('1.0', 'avocado')
        self.assertEqual(self.text.compare(
            '1.%d' % len('avocado'), '==', '1.%d' % len('avocado')), 1)
        self.assertRaises(Tkinter.TclError,
                self.text.compare, 'end', None, 'end')
        self.assertRaises(Tkinter.TclError,
                self.text.compare, 'end', '', 'end')

    def test_debug(self):
        self.assertFalse(self.text.debug())
        self.text.debug(True)
        self.assertTrue(self.text.debug())
        self.text.debug('0')
        self.assertFalse(self.text.debug())

    def test_delete(self):
        word = 'avocado'
        self.text.insert('1.0', word)
        end_insert = len(word) - 1
        # 'delete' doesn't include the character at index2, so word[-1] will
        # not be removed.
        self.text.delete('1.0', '1.%d' % end_insert)
        self.assertEqual(self.text.get('1.0'), word[-1])
        self.text.delete('1.0')
        self.assertEqual(self.text.get('1.0').strip(), '')

    def test_dlineinfo(self):
        self.text.insert('1.0', 'hi')
        info = self.text.dlineinfo('1.0')
        self.assertTrue(isinstance(info, tuple))
        self.assertEqual(len(info), 5)

        self.assertIs(self.text.dlineinfo('3.0'), None)

    def test_dump(self):
        text = 'hi there'
        self.text.insert('1.0', text)
        output = self.text.dump('1.0', '1.5')
        self.assertTrue(isinstance(output, list))
        self.assertEqual(len(output), 1)
        self.assertTrue(isinstance(output[0], tuple))
        test = output[0]
        self.assertEqual(test, ('text', text[:5], '1.0'))

        self.assertEqual(self.text.dump('1.0', 'end'),
                self.text.dump('1.0', 'end', image=True, mark=True, tag=True,
                    text=True, window=True))

        result = []
        def cmd(key, value, index):
            result.append((key, value, index))
        self.assertIs(self.text.dump('1.0', command=cmd), None)
        self.assertEqual(result, [('text', text[0], '1.0')])
        self.assertFalse(self.text._tclCommands)

    def test_edit(self):
        self.text['undo'] = False
        self.assertFalse(self.text['undo'])

        self.text.insert('1.0', 'hi')
        self.text.edit_undo()
        self.assertEqual(self.text.get('1.0', '1.2'), 'hi')
        self.text.delete('1.0', 'end')

        self.text['undo'] = True
        self.assertTrue(self.text['undo'])
        self.text.insert('1.0', 'hi')
        self.assertEqual(self.text.get('1.0', '1.2'), 'hi')
        self.assertTrue(self.text.edit_modified())
        self.text.edit_undo()
        self.assertFalse(self.text.get('1.0', '1.2'))
        # There is nothing to undo now
        self.assertRaises(Tkinter.TclError, self.text.edit_undo)

        self.text.edit_redo()
        self.assertEqual(self.text.get('1.0', '1.2'), 'hi')

        self.text.edit_reset()
        self.assertRaises(Tkinter.TclError, self.text.edit_undo)
        self.assertRaises(Tkinter.TclError, self.text.edit_redo)

    def test_get(self):
        word = 'avocado'
        self.text.insert('1.0', word)
        self.assertEqual(self.text.get('1.0'), word[0])
        self.assertEqual(self.text.get('1.0', '1.2'), word[:2])

    def test_image_cget(self):
        pass

    def test_image_configure(self):
        pass

    def test_image_names(self):
        pass

    def test_index(self):
        self.assertTrue(isinstance(self.text.index('end'), str))
        self.assertRaises(Tkinter.TclError, self.text.index, '')

    def test_insert(self):
        self.text.insert('1.0', 'abc')
        self.text.insert('1.1', 'bcd a', 'mytag')
        self.assertEqual(self.text.get('1.0', 'end').strip(), 'abcd abc')
        self.assertIn('mytag', self.text.tag_names())

    def test_mark(self):
        pass

    def test_scan_mark(self):
        pass

    def test_scan_dragto(self):
        pass

    def test_search(self):
        text = self.text

        # pattern and index are obligatory arguments.
        self.failUnlessRaises(Tkinter.TclError, text.search, None, '1.0')
        self.failUnlessRaises(Tkinter.TclError, text.search, 'a', None)
        self.failUnlessRaises(Tkinter.TclError, text.search, None, None)

        # Invalid text index.
        self.failUnlessRaises(Tkinter.TclError, text.search, '', 0)

        # Check if we are getting the indices as strings -- you are likely
        # to get Tcl_Obj under Tk 8.5 if Tkinter doesn't convert it.
        text.insert('1.0', 'hi-test')
        self.failUnlessEqual(text.search('-test', '1.0', 'end'), '1.2')
        self.failUnlessEqual(text.search('test', '1.0', 'end'), '1.3')

    def test_see(self):
        pass

    def test_tag(self):
        pass

    def test_window(self):
        pass

    def test_xview(self, method='xview'):
        view = getattr(self.text, method)()
        self.assertTrue(isinstance(view, tuple))
        self.assertEqual(len(view), 2)
        for item in view:
            self.assertTrue(isinstance(item, float))

    def test_yview(self):
        self.test_xview('yview')


tests_gui = (TextTest, )

if __name__ == "__main__":
    run_unittest(*tests_gui)
