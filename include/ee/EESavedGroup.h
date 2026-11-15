// EESavedGroup.h (RECONSTRUCTION; file name inferred). A hot-key group (RTTI EESavedGroup > EEGroup >
// UReferenceCounter; vtable 0x446f6c; sizeof 0x40; EESavedGroup.cpp 0x208e6b..0x209008). EEPlayer +0x954 holds 20 of
// them (mGroups, typed EEGroup*); the slots forward to EEGroup's and tell the user interface (thread message 0x435
// with mIndex) when the group's first unit changes. Names are provisional.
// Requires ee/EEPlayer.h (EEGroup, EESelectedGroup).
#ifndef EE_EESAVEDGROUP_H
#define EE_EESAVEDGROUP_H

#include "ee/EEPlayer.h"

class EEComplexUnit;
class FSFile;

class EESavedGroup : public EEGroup {
public:
    EESavedGroup(EEPlayer* inPlayer, long inIndex);   // 0x208e6b
    virtual ~EESavedGroup();                // +0x00 0x208ea6 (??_G 0x208e8a)
    virtual void Add(EEComplexUnit* inUnit);   // +0x04 0x208ebd (living units only)
    virtual long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile);   // +0x1c 0x208f24
    virtual void RemoveUnit(EEComplexUnit* inUnit);  // +0x28 0x208f38
    virtual long RepairPointers(unsigned long inMajorVersion, unsigned long inMinorVersion);  // +0x2c 0x208f67
    virtual long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile);   // +0x30 0x208f77
    virtual void Validate();                // +0x34 0x208f8b (drops dead, invalid and given-away units)
    // 0x208ef3: takes inGroup's units (sorted first) and becomes its saved group (names.txt also carries the
    // EEGroup::Assign spelling EEMessage.cpp calls) (provisional name)
    void Assign(EESelectedGroup* inGroup);

    long mIndex;                            // +0x3c the hot-key index (the ctor argument; posted to the UI)
};

#endif
