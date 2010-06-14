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
    CONFIGS="x64 ansi64 ansi64c32 ppro ansi32 ansi-legacy"
fi

GMAKE=`which gmake`
if [ X"$GMAKE" = X"" ]; then
    GMAKE=make
fi


cp -f Makefile.unix Makefile

for config in $CONFIGS; do
    printf "\n# ========================================================================\n"
    printf "#                                 %s\n" $config
    printf "# ========================================================================\n\n"
    $GMAKE clean
    $GMAKE MACHINE=$config gmp
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



