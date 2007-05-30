from __future__ import with_statement

import os

file_name = os.path.join('Lib', 'sys.py')

def set_up():
    with open(file_name, 'w') as py_file:
        py_file.write('test_attr = 42')

def tear_down():
    os.unlink(file_name)
