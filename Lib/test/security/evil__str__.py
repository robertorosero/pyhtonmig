"""How evil can we be with an exception that defines a __str__() method?"""

evil_source = '''
import __builtin__

class EvilException(Exception):
    
    """Try to be evil!"""
    
    def __str__(self):
        # Does this get it where the exception is used, or where the class was
        #defined?
        import __builtin__
        if 'open' in __builtin__.__dict__:
            print "Evil 2!"
        # Just try to get it directly from the built-in namespace.
        try:
            temp = open
        except NameError:
            pass
        else:
            print "Evil 3!"
            
        return "Evil exception!!!"
        
__builtins__.__dict__['evil_cls'] = EvilException
__builtins__.__dict__['evil_inst'] = EvilException()
'''

import interpreter

interp = interpreter.Interpreter()
del interp.builtins()['open']
interp.execute(evil_source)

slave_cls = interp.builtins()['evil_cls']
slave_inst = interp.builtins()['evil_inst']
master_inst = slave_cls()

print "Raising slave class ..."
try:
    raise slave_cls
except Exception, exc:
    print exc
    
print "Raising slave instance ..."
try:
    raise slave_inst
except Exception, exc:
    print exc
    
print "Raising master instance ..."
try:
    raise master_inst
except Exception, exc:
    print exc
    
print "Just printing from the class ..."
print slave_cls

print "Just printing from the slave instance ..."
print slave_inst

print "Just printing from the master instance ..."
print master_inst