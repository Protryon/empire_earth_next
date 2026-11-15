// Dinkum C++ Library for VC++ 2.33, sxlrt233.lib static member: operator delete[] (reconstruction; see newaop.cpp).
// UNIT-COMPILER: msvc6.4
// UNIT-FLAGS: -Od

void operator delete(void *);

// FUNCTION: TCP 0x10004790
// FUNCTION: LLE 0x10084810
// FUNCTION: MIXER 0x100023a0
// FUNCTION: DEFAULT 0x10018b50
// FUNCTION: DX7HR 0x1000fcf0
// FUNCTION: DX7HRTNL 0x10010570
// Frees an array with plain operator delete
void operator delete[](void *ptr)
{
    operator delete(ptr);
}
