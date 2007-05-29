"""None of the modules required for the interpreter to work should expose the
'sys' module (nor the modules that they import)."""
import encodings
module_type = type(encodings)
examined = set()
def check_imported_modules(module):
    """Recursively check that the module (and the modules it imports) do not
    expose the 'sys' module."""
    assert isinstance(module, module_type)
    if module.__name__ == 'sys':
        raise Exception
    for attr in module.__dict__.values():
        if isinstance(attr, module_type) and attr.__name__ not in examined:
            examined.add(attr.__name__)
            check_imported_modules(attr)


try:
    import __builtin__
    __builtin__.sys
    check_imported_modules(__builtin__)
except AttributeError:
    pass
else:
    raise Exception

try:
    import __main__
    __main__.sys
    check_imported_modules(__main__)
except AttributeError:
    pass
else:
    raise Exception

try:
    import exceptions
    exceptions.sys
    check_imported_modules(exceptions)
except AttributeError:
    pass
else:
    raise Exception

try:
    import encodings
    encodings.sys
    check_imported_modules(encodings)
except AttributeError:
    pass
else:
    raise Exception

try:
    import codecs
    codecs.sys
    check_imported_modules(codecs)
except AttributeError:
    pass
else:
    raise Exception

try:
    import _codecs
    _codecs.sys
    check_imported_modules(_codecs)
except AttributeError:
    pass
else:
    raise Exception
