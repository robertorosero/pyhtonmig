:mod:`cdecimal` --- Decimal fixed point and floating point arithmetic
=====================================================================

.. module:: cdecimal
   :synopsis: C-Implementation of the General Decimal Arithmetic Specification.

.. moduleauthor:: Stefan Krah <skrah@bytereef.org>

.. import modules for testing inline doctests with the Sphinx doctest builder
.. testsetup:: *

   import cdecimal
   import math
   from cdecimal import *
   # make sure each group gets a fresh context
   setcontext(Context())


The :mod:`cdecimal` module is a C implementation of :mod:`decimal`. Since it is
almost fully compatible with :mod:`decimal`, this documentation will only list
the differences between the two modules.


Setting Context Values
----------------------

* *prec*, *Emin*, *Emax*, *rounding*, *capitals* and *_clamp* are implemented as
  getters/setters.

* An additional field *_allcr* toggles correct rounding for :meth:`exp`,
  :meth:`ln` and :meth:`log10`.

* *traps* and *flags* have the custom type :class:`SignalDict`,
  which behaves like a dictionary for most purposes. This is the familiar
  interface from :mod:`decimal`.

* Internally, *traps* and *flags* are just C unsigned integers. :mod:`cdecimal`
  provides the option to access the integers directly using the getters/setters
  *_traps* and *_flags*.

* Use of the two interfaces can be mixed freely. The following table shows
  how the :class:`SignalDict` items and the C-flags are related:

  +---------------------------+------------------------------------+--------------------------------+
  |         SignalDict        |              C signals             |          C conditions          |
  +===========================+====================================+================================+
  | :const:`InvalidOperation` |  :const:`DecIEEEInvalidOperation`  |              n/a               |
  +---------------------------+------------------------------------+--------------------------------+
  |            n/a            |                n/a                 | :const:`DecConversionSyntax`   |
  +---------------------------+------------------------------------+--------------------------------+
  |            n/a            |                n/a                 | :const:`DecDivisionImpossible` |
  +---------------------------+------------------------------------+--------------------------------+
  |            n/a            |                n/a                 | :const:`DecDivisionUndefined`  |
  +---------------------------+------------------------------------+--------------------------------+
  |            n/a            |                n/a                 | :const:`DecInvalidContext`     |
  +---------------------------+------------------------------------+--------------------------------+
  |            n/a            |                n/a                 | :const:`DecInvalidOperation`   |
  +---------------------------+------------------------------------+--------------------------------+
  |            n/a            |                n/a                 | :const:`DecMallocError`        |
  +---------------------------+------------------------------------+--------------------------------+
  | :const:`Clamped`          | :const:`DecClamped`                | :const:`DecClamped`            |
  +---------------------------+------------------------------------+--------------------------------+
  | :const:`DivisionByZero`   | :const:`DecDivisionByZero`         | :const:`DecDivisionByZero`     |
  +---------------------------+------------------------------------+--------------------------------+
  | :const:`Inexact`          | :const:`DecInexact`                | :const:`DecInexact`            |
  +---------------------------+------------------------------------+--------------------------------+
  | :const:`Rounded`          | :const:`DecRounded`                | :const:`DecRounded`            |
  +---------------------------+------------------------------------+--------------------------------+
  | :const:`Subnormal`        | :const:`DecSubnormal`              | :const:`DecSubnormal`          |
  +---------------------------+------------------------------------+--------------------------------+
  | :const:`Overflow`         | :const:`DecOverflow`               | :const:`DecOverflow`           |
  +---------------------------+------------------------------------+--------------------------------+
  | :const:`Underflow`        | :const:`DecUnderflow`              | :const:`DecUnderflow`          |
  +---------------------------+------------------------------------+--------------------------------+


Context Limits
--------------


+------------+-----------------+---------------------+------------------------------+
|            |     decimal     |  cdecimal, 32-bit   |      cdecimal, 64-bit        |
+============+=================+=====================+==============================+
| max *Emax* |    unlimited    | :const:`425000000`  | :const:`999999999999999999`  |
+------------+-----------------+---------------------+------------------------------+
| min *Emin* |    unlimited    | :const:`-425000000` | :const:`-999999999999999999` |
+------------+-----------------+---------------------+------------------------------+
| max *prec* |    unlimited    | :const:`425000000`  | :const:`999999999999999999`  |
+------------+-----------------+---------------------+------------------------------+
| min *Etop* | may be negative | :const:`0`          | :const:`0`                   |
+------------+-----------------+---------------------+------------------------------+


*Etop* is only relevant if *_clamp* is set to 1. In this case, the maximum exponent
is defined as *Etop* = *Emax* - (*prec*-1).


Thread local default contexts
-----------------------------

When no context is given, all operations use the default context. In :mod:`decimal`,
this default context is thread local. For :mod:`cdecimal`, thread local default contexts
can be enabled at compile time. However, the performance penalty is so huge that
this is not the default.

The consequences for threaded programs are:

* One new context has to be created for each thread.

* Only context methods or methods that take a context argument can be used.


Unlimited reading of decimals
-----------------------------

The :class:`Decimal` constructor is supposed to read input as if there were no limits.
Since the context of cdecimal has limits, the following approach is used:

If the :const:`Inexact` or :const:`Rounded` conditions occur during conversion,
:const:`InvalidOperation` is raised and the result is :const:`NaN`. In this case,
the :meth:`create_decimal` context method has to be used.


Correct rounding in the power method
------------------------------------

The :meth:`power()` method in :mod:`decimal` is correctly rounded. :mod:`cdecimal`
currently only guarantees an error less than one ULP (which is standard conforming).



