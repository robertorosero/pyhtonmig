#!/bin/sh

echo ""
echo "Running official tests ..."
echo ""

valgrind --tool=memcheck --leak-check=full --leak-resolution=high \
         --db-attach=yes --show-reachable=yes \
         ./runtest --all official.decTest

echo ""
echo "Running additional tests ..."
echo ""

valgrind --tool=memcheck --leak-check=full --leak-resolution=high \
         --db-attach=yes --show-reachable=yes \
         ./runtest --all additional.decTest

echo ""
echo "Running long tests ..."
echo ""

for file in mpd_mpz_add mpd_mpz_sub mpd_mpz_mul mpd_mpz_divmod \
    karatsuba_fnt karatsuba_fnt2
do
    if [ -f $file ]; then
        valgrind --tool=memcheck --leak-check=full --leak-resolution=high \
                 --db-attach=yes --show-reachable=yes \
                 ./$file
        echo ""
    fi
done



