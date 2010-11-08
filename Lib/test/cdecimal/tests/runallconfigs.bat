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
copy /y mpdecimal64vc.h mpdecimal.h
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
copy /y mpdecimal64vc.h mpdecimal.h
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
copy /y mpdecimal32vc.h mpdecimal.h
nmake MACHINE=full_coverage extended_gmp
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
copy /y mpdecimal32vc.h mpdecimal.h
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
copy /y mpdecimal32vc.h mpdecimal.h
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
copy /y mpdecimal32vc.h mpdecimal.h
nmake MACHINE=ansi-legacy extended_gmp
cd tests
call runalltests.bat



