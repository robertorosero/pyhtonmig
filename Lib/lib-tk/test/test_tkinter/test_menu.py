import sys
import unittest
import Tkinter
from test.test_support import requires, run_unittest
from ttk import setup_master

requires('gui')

class MenuTest(unittest.TestCase):

    def setUp(self):
        self.root = setup_master()
        self.menu = Tkinter.Menu(self.root, tearoff=False)

    def tearDown(self):
        self.menu.destroy()


    def test_menu_creation(self):
        test1 = Tkinter.Menu(self.root, tearoff=False)
        test1.add_checkbutton(label='Check', indicatoron=True)
        test1.add_radiobutton(label='Radio', indicatoron=True)
        test1.add_separator()
        test1.add_command(label='Cmd', accelerator='C', underline=0)
        self.assertRaises(Tkinter.TclError, test1.add_separator, label='sep')
        self.assertRaises(Tkinter.TclError, test1.add_separator,
                accelerator='X')

        test2 = Tkinter.Menu(self.root, tearoff=True)
        test2.insert_checkbutton(1, label='Check')
        test2.insert_separator(2)
        test2.insert_radiobutton(1, label='Radio')
        test2.insert_command(4, label='Cmd')

        self.menu.add_cascade(label='Test1', menu=test1)
        self.assertRaises(Tkinter.TclError, self.menu.insert_cascade, -1)
        self.menu.insert_cascade(1, label='Test2', menu=test2)

        self.assertEqual(self.menu.type(0), 'cascade')
        m1 = self.menu.nametowidget(self.menu.entrycget(0, 'menu'))
        self.assertEqual(m1, test1)
        self.assertEqual(m1.type(0), 'checkbutton')
        self.assertEqual(m1.type(1), 'radiobutton')
        self.assertEqual(m1.type(2), 'separator')
        self.assertEqual(m1.type(3), 'command')
        m2 = self.menu.nametowidget(self.menu.entrycget(1, 'menu'))
        self.assertEqual(m2, test2)
        self.assertEqual(m2.type(0), 'tearoff')

        self.assertEqual(m1.entrycget(3, 'accelerator'), 'C')
        self.assertEqual(m1.entrycget(3, 'underline'), 0)

    def test_delete(self):
        self.menu['tearoff'] = True
        self.assertRaises(Tkinter.TclError, self.menu.delete, -1)

        self.menu.add_command(label='C1', command=lambda: None)
        self.menu.add_command(label='C2', command=lambda: None)

        self.assertIs(self.menu.index('active'), None)
        cfg1 = self.menu.entryconfigure(1)
        cfg2 = self.menu.entryconfigure(2)
        cmds = self.menu._tclCommands[:]
        self.menu.delete('active', 'end')
        self.assertEqual(self.menu.entryconfigure(1), cfg1)
        self.assertEqual(self.menu.entryconfigure(2), cfg2)
        self.assertEqual(self.menu._tclCommands, cmds)

        self.menu.activate(1)
        self.menu.delete('active')
        self.assertIs(self.menu.index('active'), None)
        self.assertEqual(len(self.menu._tclCommands), 1)

        self.menu.delete(0, 'last')
        self.assertFalse(self.menu._tclCommands)

    def test_entrycget(self):
        self.assertRaises(Tkinter.TclError, self.menu.entrycget, -1, 'hi')
        self.menu.add_checkbutton(label='Test')
        self.menu.add_separator()
        self.assertTrue(isinstance(self.menu.entrycget(0, 'state'), str))
        self.assertEqual(self.menu.entrycget(0, 'underline'), -1)
        self.assertRaises(Tkinter.TclError, self.menu.entrycget, 1, 'state')

    def test_entryconfigure(self):
        self.assertEqual(self.menu.entryconfigure(0), {})
        self.menu.add_separator()
        cfg = self.menu.entryconfigure(0)
        self.assertTrue(isinstance(cfg, dict))
        key_test = cfg.keys()[0]
        self.assertEqual(self.menu.entryconfigure(0, key_test), cfg[key_test])

        self.assertRaises(Tkinter.TclError,
                self.menu.entryconfigure, 0, label='hi')

    def test_index(self):
        self.assertIs(self.menu.index(0), None)
        self.menu.add_command(label='test')
        self.assertEqual(self.menu.index(0), self.menu.index(1))
        self.assertEqual(self.menu.index(0), self.menu.index('last'))
        self.assertIs(self.menu.index('active'), None)
        self.assertIs(self.menu.index('none'), None)
        self.assertRaises(Tkinter.TclError, self.menu.index, -1)

    def test_invoke(self):
        self.assertIs(self.menu.invoke(0), '')

        def test():
            return (1, 2, 3)
        self.menu.add_command(command=test)
        self.assertEqual(self.menu.invoke(0), (1, 2, 3))

        self.assertRaises(Tkinter.TclError, self.menu.invoke, -1)

    def test_post_unpost(self):
        if sys.platform == 'win32':
            self.skipTest("Menu post blocks on Windows.")

        self.menu.add_radiobutton(label='R')
        self.menu.add_checkbutton(label='C')

        ypos = self.menu.yposition('last')
        self.assertTrue(isinstance(ypos, int))
        self.menu.post(10, 10)
        x, y = self.menu.winfo_rootx(), self.menu.winfo_rooty()
        self.assertIs(self.menu.index('active'), None)
        self.menu.event_generate('<Enter>', x=0, y=0)
        self.menu.event_generate('<Motion>', x=x, y=y)
        # If there is an active item, then we can assume that the menu was
        # posted and we moved the mouse over it correctly.
        self.assertIsNot(self.menu.index('active'), None)
        self.menu.activate(2)
        self.menu.unpost()

    # XXX Tkinter.Menu is missing xposition, postcascade and clone methods.


tests_gui = (MenuTest, )

if __name__ == "__main__":
    run_unittest(*tests_gui)
