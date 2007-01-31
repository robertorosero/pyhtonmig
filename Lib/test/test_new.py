import unittest
from test import test_support
import sys, new

class NewTest(unittest.TestCase):
    def test_spam(self):
        class Eggs:
            def get_yolks(self):
                return self.yolks

        m = new.module('Spam')
        m.Eggs = Eggs
        sys.modules['Spam'] = m
        import Spam

        def get_more_yolks(self):
            return self.yolks + 3

        # new.classobj()
        C = new.classobj('Spam', (Spam.Eggs,), {'get_more_yolks': get_more_yolks})

        # new.instance()
        c = new.instance(C, {'yolks': 3})

        o = new.instance(C)
        self.assertEqual(o.__dict__, {}, "new __dict__ should be empty")
        del o
        o = new.instance(C, None)
        self.assertEqual(o.__dict__, {}, "new __dict__ should be empty")
        del o

        def break_yolks(self):
            self.yolks = self.yolks - 2

        # new.instancemethod()
        im = new.instancemethod(break_yolks, c, C)

        self.assertEqual(c.get_yolks(), 3,
            'Broken call of hand-crafted class instance')
        self.assertEqual(c.get_more_yolks(), 6,
            'Broken call of hand-crafted class instance')

        im()
        self.assertEqual(c.get_yolks(), 1,
            'Broken call of hand-crafted instance method')
        self.assertEqual(c.get_more_yolks(), 4,
            'Broken call of hand-crafted instance method')

        im = new.instancemethod(break_yolks, c)
        im()
        self.assertEqual(c.get_yolks(), -1)

        # Verify that dangerous instance method creation is forbidden
        self.assertRaises(TypeError, new.instancemethod, break_yolks, None)
