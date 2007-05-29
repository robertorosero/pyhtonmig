"""The built-in execfile should not be reachable."""
try:
    _ = execfile
except NameError:
    pass
else:
    raise Exception

try:
    import __builtin__
    __builtin__.execfile
except AttributeError:
    pass
else:
    raise Exception

try:
    __builtins__.execfile
except AttributeError:
    pass
else:
    raise Exception
