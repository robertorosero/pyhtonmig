echo "Build secure_python.o ..."
gcc -Wall -g -c -IInclude -I. secure_python.c
echo "Build executable ..."
gcc -L. -ldl -lpython2.6 -o secure_python.exe secure_python.o
