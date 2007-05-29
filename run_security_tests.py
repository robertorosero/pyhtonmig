import subprocess
import os
import re

def run_tests(type_, test_verifier):
    failures = []
    print "Running '%s' tests ..." % type_
    for file_name in (x for x in os.listdir(os.path.join('tests', type_))
            if x.endswith('.py') and not x.startswith('_')):
        test_name = file_name[:-3]
        print '\t%s ...' % test_name,

        exec_ = './secure_python.exe ' + os.path.join('tests', type_, file_name)
        proc = subprocess.Popen(exec_, stderr=subprocess.PIPE, shell=True,
                                universal_newlines=True)
        proc.wait()
        stderr_output = proc.stderr.read()
        if not test_verifier(test_name, stderr_output):
            print 'failed'
            failures.append(test_name)
        else:
            print 'passed'
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
