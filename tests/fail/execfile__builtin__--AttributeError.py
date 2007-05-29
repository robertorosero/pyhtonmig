"""'execfile' should not be accessible from __builtin__."""
import __builtin__
__builtin__.execfile
