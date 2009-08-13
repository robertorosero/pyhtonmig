import sys

def tb_print_list(extracted_list, file=sys.stderr):
    """An extended version of traceback.print_list which doesn't include
    "in <module>" when printing traceback."""
    for fname, lineno, name, line in extracted_list:
        if name == '<module>':
            file.write('  File "%s", line %d\n' % (fname, lineno))
        else:
            file.write('  File "%s", line %d, in %s\n' % (fname, lineno, name))
        if line:
            file.write('    %s\n' % line.strip())
