#!/bin/sh


printf "\nRunning official tests ... \n\n"
$VALGRIND ./runtest --all official.decTest

printf "\nRunning additional tests ... \n\n"
$VALGRIND ./runtest --all additional.decTest


printf "\nRunning long tests ... \n\n"
./ppro_mulmod
if [ -f mpd_mpz_add ]; then
    $VALGRIND ./mpd_mpz_add
fi
if [ -f mpd_mpz_sub ]; then
    $VALGRIND ./mpd_mpz_sub
fi
if [ -f mpd_mpz_mul ]; then
    $VALGRIND ./mpd_mpz_mul
fi
if [ -f mpd_mpz_divmod ]; then
    $VALGRIND ./mpd_mpz_divmod
fi
$VALGRIND ./karatsuba_fnt
$VALGRIND ./karatsuba_fnt2


printf "\nRunning transpose tests ... \n\n"
$VALGRIND ./test_transpose


printf "\nRunning locale and format tests ... \n\n"

../../../../python ../genrandlocale.py | $VALGRIND ./runtest -
../../../../python ../genrandformat.py | $VALGRIND ./runtest -
../../../../python ../genlocale.py | $VALGRIND ./runtest -



