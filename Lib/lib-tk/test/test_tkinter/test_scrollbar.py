import unittest
import Tkinter
from test.test_support import requires, run_unittest
from ttk import setup_master

requires('gui')

class ScrollbarTest(unittest.TestCase):

    def setUp(self):
        self.root = setup_master()
        self.text = Tkinter.Text(self.root, width=50, height=10)
        self.sb = Tkinter.Scrollbar(self.root)
        self.text.pack(side='left', fill='both', expand=True)
        self.sb.pack(side='right', fill='y')
        self.text['yscrollcommand'] = self.sb.set
        self.sb['command'] = self.text.yview

    def tearDown(self):
        self.sb.destroy()
        self.text.destroy()


    def test_activate(self):
        self.assertIs(self.sb.activate(), None)
        self.sb.activate('arrow2')
        self.assertEqual(self.sb.activate(), 'arrow2')
        self.sb.activate('something invalid')
        self.assertIs(self.sb.activate(), None)

    def test_delta(self):
        self.sb.update_idletasks()
        self.assertTrue(isinstance(self.sb.delta(5, 5), float))
        self.assertTrue(isinstance(self.sb.delta('1', '2'), float))
        self.assertRaises(Tkinter.TclError, self.sb.delta, 1, {})

    def test_fraction(self):
        self.sb.update_idletasks()
        self.assertTrue(isinstance(self.sb.fraction(5, 5), float))
        self.assertTrue(isinstance(self.sb.fraction('1', '2'), float))
        self.assertRaises(Tkinter.TclError, self.sb.fraction, 1, {})

    def test_identify(self):
        self.sb.update_idletasks()
        x_mid = self.sb.winfo_width() / 2
        y_max = self.sb.winfo_height()

        values = ['', 'arrow1', 'arrow2', 'slider']
        empty = self.sb.identify(-1, -1)
        self.assertEqual(empty, '')
        values.remove(empty)
        self.assertEqual(values, ['arrow1', 'arrow2', 'slider'])
        arrow1 = self.sb.identify(x_mid, 5)
        self.assertIn(arrow1, values)
        values.remove(arrow1)
        self.assertEqual(values, ['arrow2', 'slider'])
        arrow2 = self.sb.identify(x_mid, y_max - 5)
        self.assertIn(arrow2, values)
        values.remove(arrow2)
        self.assertEqual(values, ['slider'])

        for i in range(5, y_max):
            if self.sb.identify(x_mid, i) in values:
                values.remove('slider')
        self.assertFalse(values)

    def test_get_set(self):
        self.sb.update_idletasks()
        curr = self.sb.get()
        self.assertTrue(isinstance(curr, tuple))
        self.assertEqual(len(curr), 2)
        for item in curr:
            self.assertTrue(isinstance(item, float))

        self.sb.set(2, 2)
        self.assertEqual(self.sb.get(), (1.0, 1.0))
        self.sb.set(-1, 2)
        self.assertEqual(self.sb.get(), (0, 1))


tests_gui = (ScrollbarTest, )

if __name__ == "__main__":
    run_unittest(*tests_gui)
