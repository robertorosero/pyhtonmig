@ECHO OFF

copy /y Makefile.vc Makefile


call vcvars64.bat

nmake clean
nmake machine=x64-asm gmp
call runalltests.bat

nmake clean
nmake machine=x64-ansi gmp
call runalltests.bat


call vcvars32.bat

nmake clean
nmake machine=ppro gmp
call runalltests.bat

nmake clean
nmake machine=ansi gmp
call runalltests.bat



