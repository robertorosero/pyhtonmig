

Test the libmpdec library directly without any intervention of the Python
interpreter. This is particularly useful when testing with a memory
debugger.

Some tests require a working gmp installation.


1. Tests that do not require gmp:

   # Use gmake on *BSD
   make MACHINE=[x64|ansi64|ansi64c32|ppro|ansi32|ansi-legacy] && ./runalltests.sh

   This runs:

      official.decTest:    IBM/Cowlishaw's test cases
      additional.decTest:  Additional tests (bignum, format, same-pointer arguments etc.)
      karatsuba_fnt:       Test Karatsuba multiplication against fnt multiplication.
      karatsuba_fnt2:      Test Karatsuba multiplication against karatsuba-with-fnt-basecase.
      ppro_mulmod:         Only relevant if PPRO is defined. Tests pentium-pro modular multiplication.
      test_transpose:      Test the transpose functions (used in the fnt).

2. Tests that require gmp:

   # Use gmake on *BSD
   make MACHINE=[x64|ansi64|ansi64c32|ppro|ansi32|ansi-legacy] gmp && ./runalltests.sh

   This runs all the above tests and:

      mpd_mpz_add:    Test addition against gmp.
      mpd_mpz_divmod: Test division with remainder against gmp.
      mpd_mpz_mul:    Test multiplication against gmp.
      mpd_mpz_sub:    Test subtraction against gmp.


3. Very extensive format tests:

   ../../../../python ../genrandlocale.py | ./runtest -
   ../../../../python ../genrandformat.py | ./runtest -
   ../../../../python ../genlocale.py | ./runtest -


