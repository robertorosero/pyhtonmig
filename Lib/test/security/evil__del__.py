evil_str = r"""
import __builtin__
import sys

class Evil(object):

    builtin = __builtin__.__dict__
    stdout = sys.stdout
    NameError = NameError
    BaseException = BaseException

    def __init__(self, num):
        self.num = num

    def __del__(self):
        if 'open' in self.builtin:
            self.stdout.write('(%s) First Evil!\n' % self.num)
        else:
            self.stdout.write("(%s) First Good!\n" % self.num)
        self.stdout.flush()

        try:
            temp = open
        except self.NameError:
            self.stdout.write("(%s) Second Good!\n" % self.num)
        except self.BaseException, exc:
            self.stdout.write("Unexpected exception: %r\n" % exc)
        else:
            self.stdout.write("(%s) Second Evil!\n" % self.num)
        finally:
            self.stdout.flush()

# Deletion in own scope.
Evil(0)

# Cleanup of interpreter.
__builtin__.__dict__['evil1'] = Evil(1)
# Explicitly deleted in calling interpreter.
__builtin__.__dict__['evil2'] = Evil(2)
# Implicit deletion during teardown of calling interpreter.
__builtin__.__dict__['evil3'] = Evil(3)
"""

import interpreter
import __builtin__


interp = interpreter.Interpreter()
print 'Same builtins?:', ('no' if id(__builtin__.__dict__) !=
                            id(interp.builtins) else 'yes')
del interp.builtins['open']
print 'Running interpreter ...'
interp.execute(evil_str)

evil2 = interp.builtins['evil2']
evil3 = interp.builtins['evil3']

print 'Deleting interpreter ...'
del interp

print 'Explicitly deleting locally ...'
del evil2

print 'Implicit deletion locally from interpreter teardown ...'
