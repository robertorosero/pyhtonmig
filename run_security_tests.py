import subprocess
import os
import re

def exec_test(file_path):
    exec_ = './secure_python.exe ' + file_path
    proc = subprocess.Popen(exec_, stderr=subprocess.PIPE, shell=True,
                            universal_newlines=True)
    proc.wait()
    stderr_output = proc.stderr.read()
    return stderr_output


def run_tests(type_, test_verifier):
    failures = []
    print "Running '%s' tests ..." % type_
    for path_name in (x for x in os.listdir(os.path.join('tests', type_))
            if not x.startswith('_') and not x.startswith('.')):
        path = os.path.join('tests', type_, path_name)
        if os.path.isfile(path):
            if not path_name.endswith('.py'):
                continue
            test_name = path_name[:-3]
            print '\t%s ...' % test_name,
            stderr_output = exec_test(path)
            if not test_verifier(test_name, stderr_output):
                print 'failed'
                failures.append(test_name)
            else:
                print 'passed'
        elif os.path.isdir(path):
            print '\t%s ...' % path_name,
            module_name = 'tests.%s.%s.prep' % (type_, path_name)
            module = __import__(module_name, fromlist=['set_up', 'tear_down'],
                                level=0)
            module.set_up()
            try:
                stderr_output = exec_test(os.path.join(path, 'test.py'))
                if not test_verifier(test_name, stderr_output):
                    print 'failed'
                else:
                    print 'passed'
            finally:
                module.tear_down()


    return failures


debug_refs_regex = re.compile(r"^\[\d+ refs\]$")

def verify_no_output(test_name, stderr):
    """Should only have debug build output.

    Does not work for non-debug builds!

    """
    if not debug_refs_regex.match(stderr):
        return False
    return True


def verify_exception(test_name, stderr):
    """Should have an exception line with the proper exception raised."""
    exc_name = test_name.split('--')[1]
    if not re.search('^'+exc_name, stderr, re.MULTILINE):
        return False
    return True

for type_, verifier in (('succeed', verify_no_output),
        ('fail', verify_no_output)):
    failures = run_tests(type_, verifier)
    if failures:
        print '%s failures: %s' % (len(failures), ', '.join(failures))
    else:
        print 'All tests passed'
    print
