#!/bin/sh

printf "\nRunning official tests ... \n\n"
./runtest --all official.decTest

printf "\nRunning additional tests ... \n\n"
./runtest --all additional.decTest


printf "\nRunning long tests ... \n\n"

./ppro_mulmod
if [ -f mpd_mpz_add ]; then
    ./mpd_mpz_add
fi
if [ -f mpd_mpz_sub ]; then
    ./mpd_mpz_sub
fi
if [ -f mpd_mpz_mul ]; then
    ./mpd_mpz_mul
fi
if [ -f mpd_mpz_divmod ]; then
    ./mpd_mpz_divmod
fi
./karatsuba_fnt
./karatsuba_fnt2
./test_transpose


