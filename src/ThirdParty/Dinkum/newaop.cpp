// Dinkum C++ Library for VC++ 2.33, sxlrt233.lib static member: operator new[] (reconstruction; Dinkum 2.33's sxl\
// sources are not available). VC6 had no operator new[]: Dinkum's <new> declares it and its import library supplies
// this static member, so every module that calls new[] links it (Rich Utc12_CPP 8799: built with VC6 SP4, no
// optimisation, like delaop.cpp).
// UNIT-COMPILER: msvc6.4
// UNIT-FLAGS: -Od
#include <stddef.h>

void *operator new(size_t);

// Allocates an array with plain operator new
void *operator new[](size_t count)
{
    return operator new(count);
}
