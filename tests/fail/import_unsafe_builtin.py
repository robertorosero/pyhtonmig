"""You should not be able to import non-whitelisted modules, especially sys."""
try:
    import sys
except ImportError:
    pass
else:
    raise Exception
