"""The constructor for the 'code' type should not work."""
def fxn():
    pass

code = type(fxn.func_code)
try:
    code(0, 0, 0, 0, '', (), (), (), 'test', '', 0, '')
except TypeError:
    pass
else:
    raise Exception
