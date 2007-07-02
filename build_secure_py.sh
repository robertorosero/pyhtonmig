echo "Build secure_python.o ..."
gcc -Wall -g -c -IInclude -I. secure_python.c
echo "Build executable ..."
# If the command below fails, might need to add -lutil or -Wl,-E .
gcc -L. -lpthread -lm -ldl -o secure_python.exe secure_python.o libpython2.6.a
