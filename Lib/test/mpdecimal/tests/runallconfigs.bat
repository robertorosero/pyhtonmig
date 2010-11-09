@ECHO OFF

cd ..\
copy /y Makefile.vc Makefile
cd tests
copy /y Makefile.vc Makefile


call vcvarsall x64

echo.
echo # ========================================================================
echo #                                   x64
echo # ========================================================================
echo.
cd ..\
nmake clean
nmake MACHINE=x64 extended_gmp
cd tests
call runalltests.bat


echo.
echo # ========================================================================
echo #                                 ansi64
echo # ========================================================================
echo.
cd ..\
nmake clean
nmake MACHINE=ansi64 extended_gmp
cd tests
call runalltests.bat


echo.
echo # ========================================================================
echo #                             full_coverage
echo # ========================================================================
echo.
cd ..\
nmake clean
patch < tests\fullcov_header.patch
nmake MACHINE=full_coverage extended_gmp
patch -R < tests\fullcov_header.patch
cd tests
call runalltests.bat


call vcvarsall x86

echo.
echo # ========================================================================
echo #                                  ppro
echo # ========================================================================
echo.
cd ..\
nmake clean
nmake MACHINE=ppro extended_gmp
cd tests
call runalltests.bat

echo.
echo # ========================================================================
echo #                                ansi32
echo # ========================================================================
echo.
cd ..\
nmake clean
nmake MACHINE=ansi32 extended_gmp
cd tests
call runalltests.bat

echo.
echo # ========================================================================
echo #                              ansi-legacy
echo # ========================================================================
echo.
cd ..\
nmake clean
nmake MACHINE=ansi-legacy extended_gmp
cd tests
call runalltests.bat



