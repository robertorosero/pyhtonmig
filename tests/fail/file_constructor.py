"""The constructor for 'file' should not work to open a file."""
try:
    _ = file('README', 'r')
except TypeError:
    pass
else:
    raise Exception
