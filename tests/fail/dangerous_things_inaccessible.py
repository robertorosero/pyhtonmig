"""None of the modules required for the interpreter to work should expose the
'sys' module, 'open', or 'execfile'.  This holds for the modules that they
import as well."""
# Stuff needed to look for sys.
import encodings
module_type = type(encodings)
examined = set()
# Needed to look for 'open' and 'execfile'.
builtin_fxn_type = type(any)
dangerous_builtins = ('open', 'execfile')
# Needed for SystemExit.
exc_type = type(Exception)
dangerous_exceptions = ('SystemExit',)

def check_imported_modules(module):
    """Recursively check that the module (and the modules it imports) do not
    expose the 'sys' module."""
    assert isinstance(module, module_type)
    if module.__name__ == 'sys':
        raise Exception
    for attr in module.__dict__.values():
        # If an object doesn't define __name__ then we don't care about it.
        try:
            attr_name = attr.__name__
        except AttributeError:
            continue
        if isinstance(attr, module_type) and attr.__name__ not in examined:
            examined.add(attr_name)
            check_imported_modules(attr)
        elif isinstance(attr, builtin_fxn_type):
            if attr_name in dangerous_builtins:
                raise Exception
        elif isinstance(attr, exc_type):
            if attr_name in dangerous_exceptions:
                raise Exception


import __builtin__
check_imported_modules(__builtin__)

import __main__
check_imported_modules(__main__)

import encodings
check_imported_modules(encodings)

import codecs
check_imported_modules(codecs)

import _codecs
check_imported_modules(_codecs)
