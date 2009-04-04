f = open('Lib/test/formatfloat_testcases.txt')

for line in f:
    if line.startswith('--'):
        continue
    line = line.strip()
    if not line:
        continue

    lhs, rhs = map(str.strip, line.split('->'))
    fmt, arg = lhs.split()
    print(line)
    assert fmt % float(arg) == rhs
    assert fmt % -float(arg) == '-'+rhs
