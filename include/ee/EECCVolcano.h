// C:\EEX\Empire Earth\Calamities\EECCVolcano.h (RECONSTRUCTION; file name inferred). The volcano calamity class:
// Layout proven: EECCVolcano sizeof 0x54 = its new-site size, ctor 0xb4634 exact in EECCVolcano.cpp. Member names are
// provisional.
// after mGraphicEffect1's delay a volcano unit rises at the target location and the units in its way are moved off
// its tiles (vtable 0x43cefc; RTTI EECCVolcano > EELocationCalamityClass > EECalamityClass). EECCVolcano.cpp is
// 0xb4634..0xb5b7d (its std::sort COMDATs from 0xb538d). sizeof 0x54 (EECalamityManager's new). Field names
// are provisional (offsets proven by the ctor and Initialize 0xb4f72). Requires ee/EELocationCalamityClass.h,
// ee/EEAreaUtilities.h.
#ifndef EE_EECCVOLCANO_H
#define EE_EECCVOLCANO_H

class EEGraphicEffect;
class EESoundObject;
class EEUnitClass;

struct EECalamityUpdateHost;
struct EeCalamityCall;
struct EeCalamityReply;
class EECCVolcano : public EELocationCalamityClass {
public:
    long GetUnitClassIndex() const { return this->mUnitClassIndex; }  // inline (the main form's StartCalamity 0x26662d) (provisional name)
    // the class's own steps of its Rust Start and Stop (ee/EECalamityUpdate.h: EeCalamityOp::ClassStep)
    static bool StartStep(EECalamityUpdateHost& ioHost, const EeCalamityCall& inCall, EeCalamityReply& outReply);
    virtual void Place(EEUnit* inUnit, unsigned long inX, unsigned long inY);       // (the volcano reads inUnit->mPlayer) +0x30 0xb5137
    // the class's own world steps of its Rust update (ee/EECalamityUpdate.h)
    static bool UpdateStep(EECalamityUpdateHost& ioHost, const EeCalamityCall& inCall, EeCalamityReply& outReply);

    // 0xb4e74: a random location of a map tile between mMinimumRadius and mMaximumRadius around inCenter, at the
    // tile's height (another unit calls it: 0x20e879) (provisional name)
    void GetRandomLocation(const GE3DPoint& inCenter, GE3DPoint& outLocation);

protected:
    // Provisional names (by behaviour)
    // 0xb46bc (cdecl): moves the units on the inClass-sized square at (inX, inY) to the nearest free tiles around it
    static void ClearArea(EEUnitClass* inClass, long inX, long inY);
    float GetMaximumHeight(const GE3DPoint& inLocation);   // 0xb4b62: the highest terrain point around inLocation
    bool CreateVolcano(EECalamity* inCalamity);  // 0xb4d19
};

#endif
