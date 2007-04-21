gcc -c -IInclude -I. secure_python.c
gcc -o secure_python.exe secure_python.o libpython2.6.a
