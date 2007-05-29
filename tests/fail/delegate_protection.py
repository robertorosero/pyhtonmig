"""The import delegate should not expose anything critical."""
if [x for x in dir(__import__)
        if not x.startswith('__') and not x.endswith('__')]:
    raise Exception
