/*
   Proof-of-concept application that embeds Python with security features
   turned on to prevent unmitigated access to resources.

   XXX See BRANCH_NOTES for what needs to be done.
*/
#include "Python.h"

int
main(int argc, char *argv[])
{
    Py_Initialize();
    Py_Main(argc, argv);
    Py_Finalize();

    return 0;
}
