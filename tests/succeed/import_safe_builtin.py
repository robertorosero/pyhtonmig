"""Make sure that all whitelisted built-in modules can be imported."""
import _ast
import _codecs
import _sre
# Also tests that modules moved to .hidden can be imported again.
import _types
import errno
import exceptions
