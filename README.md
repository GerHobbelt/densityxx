densityxx
=========
densityxx is the c++ version of density, which is a super fast compress library.

It is BSD licensed open-source, just like density.

Where it come from?
-------------------

density is a superfast compression library composed by Centaurean.

This is a C++ version of density, spookyhash. The original code is migrate from:
* https://github.com/centaurean/density
* https://github.com/centaurean/spookyhash
* https://github.com/centaurean/sharc

Changes
-------

To use C++ features to simplify the code, the following changes are applied:
* name convention is changed to follow these rules:
  * Remove the prefix "density_" or "DENSITY_" because the enum, function, struct, class, variables are put into namespace "density".
  * The macro names are not changed because they can not be put into namespace.
  * Always use xxx_xxx name scheme, but not XxxXxx.
  * Always use lowercase letters.
  * Use suffix "_t" for name of types, classes & structures.
* Every member and functions are inlined to speed it up, just like density itself.
* Use template to combine algorithm with block framework, so virtual functions, functions pointers are not required to speed it up.
* Use context_t to do the framework task, so not all code has to be templated. This feature removes encode_t/decode_t & stream_encode_t/stream_decode_t.
* The size of generated binary decreased greatly.
* Combine the three packages, spookyhash, density, sharc into one package.
