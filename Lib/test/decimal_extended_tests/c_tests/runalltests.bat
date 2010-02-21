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


if exist ..\..\..\..\PCbuild\python.exe (
  set PYTHON=..\..\..\..\PCbuild\python.exe
) else (
  if exist ..\..\..\..\PCbuild\amd64\python.exe (
    set PYTHON=..\..\..\..\PCbuild\amd64\python.exe
  ) else (
    goto :eof
  )
)

echo.
echo Running locale and format tests ...
echo.
%PYTHON% ..\genrandlocale.py | runtest.exe -
%PYTHON% ..\genrandformat.py | runtest.exe -
%PYTHON% ..\genlocale.py | runtest.exe -



