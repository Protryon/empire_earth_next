// C:\EEX\Empire Earth\Graphics Thread\EEE2DMarchingIcons.h (RECONSTRUCTION; file name inferred). Icons circling a
// unit while an area effect of a kind is on it (RTTI EEE2DMarchingIcons > EEEffect2D; vtable 0x43fdc4;
// EEE2DMarchingIcons.cpp 0xe3954..0xe4529; sizeof 0xa0). The circular and square variants lay the
// icons out (pure WriteDetailsHook). Layout from the two ctors 0xe3aa3/0xe3b1d, the dtor 0xe3bf6 and Load/Save; names
// are provisional (the members after the save keys). Requires ee/EEEffect2D.h, lle/GE3DPoint.h.
#ifndef EE_EEE2DMARCHINGICONS_H
#define EE_EEE2DMARCHINGICONS_H

#include "ee/EEEffect2D.h"

class EEGraphicObject;

class EEE2DMarchingIcons : public EEEffect2D {
public:
    // the save keys (the unit's `$E` chains 0xe3954..0xe3aa3, guard 0x4e8908; not used by Load/Save)
    static const UString sUnitIDStringID;           // "MarchingIconsUnitIDStringID" (0x4e8910, bit 1)
    static const UString sAngularOffsetStringID;    // "MarchingIconsAngularOffsetStringID" (0x4e88f8, bit 2)
    static const UString sRotationSpeedStringID;    // "MarchingIconsRotationSpeedStringID" (0x4e88e8, bit 4)
    static const UString sIconCountStringID;        // "MarchingIconsIconCountStringID" (0x4e88d8, bit 8)
    static const UString sAreaEffectTypeStringID;   // "MarchingIconsAreaEffectTypeStringID" (0x4e88c8, bit 0x10)

    EEE2DMarchingIcons(long inKind);        // 0xe3aa3 (the load-time ctor)
    EEE2DMarchingIcons(EEUnit* inUnit, EEGraphicEffect* inGraphicEffect, long inKind);   // 0xe3b1d
    virtual ~EEE2DMarchingIcons();          // +0x00 0xe3bf6 (??_G: the ICF-kept copy 0xdf590)
    virtual long DrawPass6();               // +0x18 0xe3e92 (draws the icons)
    virtual long Queue(EEEffectQueue* inQueues);       // +0x24 0xe3c32
    virtual void PrepareQueue();            // +0x2c (empty: ICF-folded `ret` 0xe123d)
    virtual long SetUp();                   // +0x30 0xe3df8 (loads the graphic object)
    virtual long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile);   // +0x38 0xe3e0f
    virtual long RepairPointers();          // +0x3c 0xe4132
    virtual long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile);   // +0x40 0xe4182
    virtual long Step();                    // +0x4c 0xe41ef
    // +0x54: the icons' offsets around the unit for an angular offset (the circular/square variants)
    virtual void ComputeOffsets(float inAngle, std::vector<GE3DPoint>& outPoints) = 0;
    virtual bool IsIconShown(const GE3DPoint& inOffset);  // +0x58 0xe3cdf (whether an icon at inOffset shows)
    virtual bool Virtual5C();               // +0x5c 0xe3d8e
    virtual bool GetPlayerID(unsigned long& outPlayerID);  // +0x60 0xe3dd0
    virtual void LayOutIcons();             // +0x64 0xe42db (turns the icons)

    unsigned long GetIconCount();           // 0xe3dc9 (the variants' WriteDetailsHook) (provisional name)
    float GetHeightOffset();                // 0xe3df1 (0.05) (provisional name)

    std::vector<GE3DPoint> mOffsets;        // +0x54 (WriteDetailsHook's output)
    std::vector<GE3DPoint> mShown;          // +0x64 (the offsets drawn this frame)
    long mAreaEffectType;                   // +0x74
    long mAreaEffectIconCount;              // +0x78 (= 1, or the matching area effect's template +0x34)
    GE3DPoint mLocation;                    // +0x7c (the unit's position)
    EEGraphicObject* mGraphicObject;        // +0x88 (UnloadMedia in the dtor)
    float mAngularOffset;                   // +0x8c
    float mRotationSpeed;                   // +0x90 (radians per second)
    unsigned long mIconCount;               // +0x94
    long mL98;                              // +0x98 (0: Virtual64 calls Virtual2C)
    bool mVisible;                          // +0x9c (Virtual4C)
};

#endif
