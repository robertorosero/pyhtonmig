"""The built-in 'open' should not be accessible."""
try:
    _  = open
except NameError:
    pass

try:
    import __builtin__
    __builtin__.open
except AttributeError:
    pass

try:
    __builtins__.open
except AttributeError:
    pass
