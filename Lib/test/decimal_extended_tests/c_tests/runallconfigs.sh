#/bin/sh

cp -f Makefile.unix Makefile

GMAKE=`which gmake`
if [ "$GMAKE" == "" ]; then
    GMAKE=make
fi


$GMAKE clean
$GMAKE machine=x64-asm gmp
./runalltests.sh

$GMAKE clean
$GMAKE machine=x64-ansi gmp
./runalltests.sh

$GMAKE clean
$GMAKE machine=ppro gmp
./runalltests.sh

$GMAKE clean
$GMAKE machine=ansi gmp
./runalltests.sh



