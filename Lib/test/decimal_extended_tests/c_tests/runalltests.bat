@ECHO OFF


echo.
echo Running official tests ...
echo.
runtest.exe --all official.decTest

echo.
echo Running additional tests ...
echo.
runtest.exe --all additional.decTest

echo.
echo Running long tests ...
echo.
ppro_mulmod.exe
if exist mpd_mpz_add.exe mpd_mpz_add.exe
if exist mpd_mpz_sub.exe mpd_mpz_sub.exe
if exist mpd_mpz_mul.exe mpd_mpz_mul.exe
if exist mpd_mpz_divmod.exe mpd_mpz_divmod.exe
karatsuba_fnt.exe
karatsuba_fnt2.exe

echo.
echo Running transpose tests ...
echo.
test_transpose.exe

echo.
echo Running locale and format tests ...
echo.
..\..\..\..\python ..\genrandlocale.py | runtest.exe -
..\..\..\..\python ..\genrandformat.py | runtest.exe -
..\..\..\..\python ..\genlocale.py | runtest.exe -



