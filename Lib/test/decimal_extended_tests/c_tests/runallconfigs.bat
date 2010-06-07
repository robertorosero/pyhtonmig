@ECHO OFF

copy /y Makefile.vc Makefile


call vcvars64.bat

echo.
echo # ========================================================================
echo #                                   x64
echo # ========================================================================
echo.
nmake clean
nmake MACHINE=x64 gmp
call runalltests.bat

echo.
echo # ========================================================================
echo #                               ansi64c32
echo # ========================================================================
echo.
nmake clean
nmake MACHINE=ansi64c32 gmp
call runalltests.bat


call vcvars32.bat

echo.
echo # ========================================================================
echo #                                  ppro
echo # ========================================================================
echo.
nmake clean
nmake MACHINE=ppro gmp
call runalltests.bat

echo.
echo # ========================================================================
echo #                                 ansi32
echo # ========================================================================
echo.
nmake clean
nmake MACHINE=ansi32 gmp
call runalltests.bat

echo.
echo # ========================================================================
echo #                              ansi-legacy
echo # ========================================================================
echo.
nmake clean
nmake MACHINE=ansi-legacy gmp
call runalltests.bat



