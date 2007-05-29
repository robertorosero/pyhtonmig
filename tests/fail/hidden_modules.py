"""Modules that have been hidden (and stored into .hidden) should not be
reachable unless they were whitelisted."""
try:
    import posix
except ImportError:
    pass
else:
    raise Exception
