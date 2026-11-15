// EEComplexDoppelganger.h (RECONSTRUCTION from RTTI/vtables/ctors; slot signatures as EEDoppelganger's). Names are
// provisional.
// A doppelganger of a complex unit (RTTI EEComplexDoppelganger > EEDoppelganger > FSRestorable; vtable 0x446d5c;
// sizeof 0x70: EEDoppelgangerManager's pool; ctor 0x1f1f19; Units/EEComplexDoppelganger.cpp 0x1f1f19..0x1f57d3).
// +0x10 is a GETransformation (the ctor's member ctor).
#ifndef EE_EECOMPLEXDOPPELGANGER_H
#define EE_EECOMPLEXDOPPELGANGER_H

#include "ee/EEDoppelganger.h"

class EEComplexDoppelganger : public EEDoppelganger {
public:
    EEComplexDoppelganger();                // 0x1f1f19
    virtual long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile);   // +0x04 0x1f21a3
    virtual long RepairPointers(unsigned long inFirst, unsigned long inSecond);   // +0x08 0x1f230c
    virtual long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile);  // +0x0c 0x1f2392
    virtual void Initialize(EEDoppelganger* inOther, EEPlayer* inPlayer, RTSTile* inTile);   // +0x14 0x1f212e
    virtual void Initialize(EEUnit* inUnit, EEPlayer* inPlayer, RTSTile* inTile);   // +0x10 0x1f1fdf
    virtual long GetAnimation();            // +0x18 0x1f1f3f (mL54)
    virtual EEComplexDoppelganger* GetComplexDoppelganger();  // +0x1c 0x1f1f43 (this)
    virtual bool IsBuilt();                 // +0x20 0x1f1f46
    virtual long GetFrame();                // +0x24 0x1f1f57 (mL68)
    virtual EEModel* GetModel();            // +0x28 0x1f1f5b (mModel)
    virtual bool IsFoundation();            // +0x2c 0x1f1f5f (mB6C)
    virtual bool IsComplex();               // +0x30 (ICF-folded 0x18aa9: true)
    virtual void GetTransformation(GETransformation& outTransformation);  // +0x34 0x1f1f63
    virtual float GetScale();               // +0x38 0x1f1f74 (mF58)
    virtual long GetHitPoints();            // +0x3c 0x1f1f78 (mL5C & 0x7fffffff)
    virtual long GetTileX();                // +0x44 0x1f1f81 (mL60)
    virtual long GetTileY();                // +0x48 0x1f1f85 (mL64)
    virtual ~EEComplexDoppelganger();       // +0x4c (??_E 0x20348e, EEDoppelgangerManager.cpp)
    virtual void Clear();                   // +0x50 0x1f1fc2

    void Reset();                           // 0x1f1fa0 (the ctor and Clear)

    GETransformation mTransformation;       // +0x10 (the unit's location; Initialize, Virtual34)
    EEModel* mModel;                        // +0x50
    unsigned long mAnimation;               // +0x54 (the unit's +0x264)
    float mScale;                           // +0x58 (the unit's model scale)
    long mPackedHitPoints;                  // +0x5c
    long mTileX;                            // +0x60 (a tile x: EEDoppelgangerManager 0x202962)
    long mTileY;                            // +0x64 (its tile y)
    unsigned long mFrame;                   // +0x68 (the unit's model frame)
    bool mIsFoundation;                     // +0x6c
};

#endif
