"""SystemExit should not be exposed as it will trigger Py_Finalize() if it
propagates to the top of the call chain."""
try:
    _ = SystemExit
except NameError:
    pass
else:
    raise Exception

try:
    import __builtin__
    __builtin__.SystemExit
except AttributeError:
    pass
else:
    raise Exception

try:
    __builtins__.SystemExit
except AttributeError:
    pass
else:
    raise Exception
