import subprocess
import os

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


def verify_succeed_test(test_name, stderr):
    if stderr.count('\n') > 1:
        return False
    else:
        return True


def verify_fail_test(test_name, stderr):
    if stderr.count('\n') <= 1:
        return False
    exc_name = test_name.split('--')[1]
    if exc_name not in stderr:
        return False
    return True

for type_, verifier in (('succeed', verify_succeed_test),
        ('fail', verify_fail_test)):
    failures = run_tests(type_, verifier)
    if failures:
        print '%s failures: %s' % (len(failures), ', '.join(failures))
    else:
        print 'All tests passed'
    print
