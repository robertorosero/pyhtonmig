"""The built-in 'open' should not be accessible."""
try:
    _  = open
except NameError:
    pass
else:
    raise Exception

try:
    import __builtin__
    __builtin__.open
except AttributeError:
    pass
else:
    raise Exception

try:
    __builtins__.open
except AttributeError:
    pass
else:
    raise Exception
