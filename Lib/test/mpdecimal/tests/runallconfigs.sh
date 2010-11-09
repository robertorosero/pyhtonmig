#!/bin/sh


VALGRIND=
if [ X"$1" = X"--valgrind" ]; then
    shift
    VALGRIND="valgrind --tool=memcheck --leak-check=full --leak-resolution=high --db-attach=yes --show-reachable=yes"
fi
export VALGRIND

if [ -n "$1" ]; then
    CONFIGS="$@"
else
    CONFIGS="x64 uint128 ansi64 full_coverage ppro ansi32 ansi-legacy"
fi

GMAKE=`which gmake`
if [ X"$GMAKE" = X"" ]; then
    GMAKE=make
fi


for config in $CONFIGS; do
    printf "\n# ========================================================================\n"
    printf "#                                 %s\n" $config
    printf "# ========================================================================\n\n"
    cd ..
    $GMAKE clean
    ./configure MACHINE=$config
    if [ X"$config" = X"full_coverage" ]; then
        patch < tests/fullcov_header.patch
        $GMAKE extended
        patch -R < tests/fullcov_header.patch
    else
        $GMAKE extended
    fi
    cd tests
    printf "\n"
    if [ X"$config" = X"ppro" ]; then
        # Valgrind has no support for 80 bit long double arithmetic.
        savevg=$VALGRIND
        VALGRIND=
        ./runalltests.sh
        VALGRIND=$savevg
    else
        ./runalltests.sh
    fi
done



