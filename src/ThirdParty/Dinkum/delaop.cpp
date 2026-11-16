// Dinkum C++ Library for VC++ 2.33, sxlrt233.lib static member: operator delete[] (reconstruction; see newaop.cpp).
// UNIT-COMPILER: msvc6.4
// UNIT-FLAGS: -Od

void operator delete(void *);

// Frees an array with plain operator delete
void operator delete[](void *ptr)
{
    operator delete(ptr);
}
