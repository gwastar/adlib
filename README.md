**ADLIB**

Generic, portable, and efficient algorithms, data structures, and other useful things to complement the C standard library.

Supplements the C standard library with functionality that is commonly found in other standard libraries
(e.g. C++), which makes developing user-space applications in C much more convenient.

Highlights:

- Type-safe dynamic arrays (~ “vector” in C++), which can (mostly) be used like normal pointers
- Dynamic strings (~ “string” in C++), which can (mostly) be used like normal C strings
- Generic red/black and AVL trees with an API similar to those in the Linux kernel
- Type-safe and fast hash tables (implemented as "macro template")
- Custom test framework and a large number of extensive tests