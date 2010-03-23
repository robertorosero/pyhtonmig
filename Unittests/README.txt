### README.txt for the Unittests/ directory.

This directory contains C and C++ unittests implemented using Google's
open-source C++ testing framework, googletest. googletest is bundled with
Python via Subversion's svn:externals property.

To run the C unittests, run `make ctest`. This target is run as part of
`make test` and other testing targets. The C unittests will not be run if no
C++ compiler is available.


googletest site: http://code.google.com/p/googletest/

Documentation:
- Basic tutorial: http://code.google.com/p/googletest/wiki/GoogleTestPrimer
- Advanced: http://code.google.com/p/googletest/wiki/GoogleTestAdvancedGuide
