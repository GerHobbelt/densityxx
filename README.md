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
* All enum, function, struct, class, variables are put into namespace "density". For these names:
   * Remove the prefix "density_".
   * Use xxx_xxx name scheme.
   * Use suffix "_t" for name of types.
   * Always use lowercase letters.
* The name of all macros are not changed because they are not support namespace.
* For the name that put into e name of enum, function, struct, class, varables 
* All compress algorithmes are implemented in subclass of kernel_encode_t & kernel_decode_t. No more function pointer are required.
* Each member functions in spookyhash.hpp, memory.hpp, format.hpp, kernel.hpp, block.hpp are inlined.
* The size of generated binary decreased greatly.
* Combine the three packages, spookyhash, density, sharc into one package.
