evil_str = r"""
import __builtin__
import sys

class Evil(object):

    stdout = sys.stdout
    NameError = NameError
    BaseException = BaseException
    ImportError = ImportError
    KeyError = KeyError
    TypeError = TypeError

    def __init__(self, num):
        self.num = num

    def __del__(self):
        # Uses context of where deletion occurs, or where object defined?
        # __import__() might be gone and thus raise a
        # TypeError when trying to call it when it has been set to None.
        try:
            import __builtin__
            if 'open' in __builtin__.__dict__:
                self.stdout.write("Evil 2!\n")
                self.stdout.flush()
        except self.TypeError:
            pass
        try:
            temp = open
        except self.NameError:
            pass
        else:
            self.stdout.write("Evil 3!\n")
            self.stdout.flush()
            

# Deletion in own scope.
print "Creation in sub-interpreter's global scope, deletion from interpreter cleanup ..."
temp = Evil(0)

# Cleanup of interpreter.
__builtin__.__dict__['evil1'] = Evil(1)
# Explicitly deleted in calling interpreter.
__builtin__.__dict__['evil2'] = Evil(2)
# Implicit deletion during teardown of calling interpreter.
__builtin__.__dict__['evil3'] = Evil(3)
"""

import interpreter
import __builtin__
import gc

interp = interpreter.Interpreter()
del interp.builtins()['open']
gc.collect()
if 'open' not in __builtin__.__dict__:
    print "'open()' missing!"
interp.execute(evil_str)

evil2 = interp.builtins()['evil2']
evil3 = interp.builtins()['evil3']

del interp
gc.collect()

print 'Explicitly deleting locally ...'
del evil2
gc.collect()

print 'Implicit deletion locally from interpreter teardown ...'
