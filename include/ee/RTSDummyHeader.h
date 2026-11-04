// C:\EEX\RTS Engine\RTS Engine\RTSDummyHeader.h (RECONSTRUCTION; file name inferred).
// A polymorphic list head (RTTI RTSDummyHeader<RTSUnit>, vtable 0x4421f8; RTSDummyUnit, vtable 0x4421f0). sizeof 8.
// All inline: RTSWorld's ctor 0x147dfd stores +4 = 0 and then RTSDummyUnit's vptr (the base vptr store is dead),
// its dtor 0x147eb6 re-stores the base vptr; both classes share the ??_G 0x148274 (emitted after RTSWorld's dtor,
// its first user). Member name is provisional.
#ifndef EE_RTSDUMMYHEADER_H
#define EE_RTSDUMMYHEADER_H

#include <typeinfo>

template <class T>
class RTSDummyHeader {
public:
    RTSDummyHeader() : mFirst(NULL) {}
    virtual ~RTSDummyHeader() {}

    T* mFirst;                              // +0x04 (provisional name)
};

class RTSUnit;

class RTSDummyUnit : public RTSDummyHeader<RTSUnit> {
};

// The tile unit chains link their dummy head through RTSUnit pointers although an RTSDummyUnit is no RTSUnit. The
// original tested a link with dynamic_cast<RTSDummyUnit*>(link): a cast between unrelated hierarchies that VC6's
// runtime resolved by finding the target type in the object's own base list. Newer runtimes reject it for RTTI that
// records class descriptors (clang's), so the test asks the object's own type instead: the same answer, as nothing
// derives from RTSDummyUnit. Null for a null link, as dynamic_cast.
template <class T>
inline RTSDummyUnit* RTSDummyHead(T* inLink)
{
    return inLink != NULL && typeid(*inLink) == typeid(RTSDummyUnit) ? reinterpret_cast<RTSDummyUnit*>(inLink) : NULL;
}

#endif
