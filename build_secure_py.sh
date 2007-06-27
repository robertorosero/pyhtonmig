echo "Build secure_python.o ..."
gcc -Wall -g -c -IInclude -I. secure_python.c
echo "Build executable ..."
gcc -L. -lpthread -lm -ldl -o secure_python.exe secure_python.o libpython2.6.a
