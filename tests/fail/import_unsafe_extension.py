"""Importing non-whitelisted extension modules should fail."""
try:
    import thread
except ImportError:
    pass
