// EEFOLocationScreenBase.h (RECONSTRUCTION; file name inferred). A tile screen passed to the random
// location search 0x249201 (RTTI EEFOLocationScreenBase, no base; vtable 0x43bf20: ??_G 0x61767, +0x04/+0x08 the shared
// `mov al, 1; ret 4` 0x220a18). EEFOLocationScreenEnsureContinentAndPathOverlay > base (vtable 0x43bf00: the same
// ??_G (ICF), +0x04 0x23dee8 (out of line, another unit: the map tile test when mB08, then the continent of the tile
// against mContinentID), +0x08 0x61712 (inline: floors the point and calls +0x04; COMDAT in EECPAttackSubGroup.cpp)).
// Overloaded virtuals take vtable slots in reverse declaration order. Names are provisional.
// Requires lle/LowLevelEngine.h (U2DPoint), lle/GE2DPoint.h.
#ifndef EE_EEFOLOCATIONSCREENBASE_H
#define EE_EEFOLOCATIONSCREENBASE_H

class EEFOLocationScreenBase {
public:
    virtual ~EEFOLocationScreenBase() {}
    virtual bool Screen(const GE2DPoint& inPoint) { return true; }          // +0x08
    virtual bool Screen(const U2DPoint<long>& inTile) { return true; }      // +0x04
};

class EEFOLocationScreenEnsureContinentAndPathOverlay : public EEFOLocationScreenBase {
public:
    // inline (EECPAttackSubGroup 0x5b075 stores the members after the vptr)
    EEFOLocationScreenEnsureContinentAndPathOverlay()
    {
        this->mContinentID = 0;
        this->mAreaID = 0;
        this->mB08 = true;
        this->mUseWaterAreas = true;
    }
    virtual bool Screen(const GE2DPoint& inPoint)                           // +0x08 0x61712
    {
        return this->Screen(U2DPoint<long>(UMFloor(inPoint.GetX()), UMFloor(inPoint.GetY())));
    }
    virtual bool Screen(const U2DPoint<long>& inTile);                      // +0x04 0x23dee8

    unsigned short mContinentID;            // +0x04 (0: any)
    unsigned short mAreaID;                 // +0x06
    bool mB08;                              // +0x08 (test the map tile first)
    bool mUseWaterAreas;                    // +0x09
};

#endif
