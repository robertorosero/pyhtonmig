"""The built-in execfile should not be reachable."""
try:
    _ = execfile
except NameError:
    pass

try:
    import __builtin__
    __builtin__.execfile
except AttributeError:
    pass

try:
    __builtins__.execfile
except AttributeError:
    pass
