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
    EECCVolcano();                          // 0xb4634
    virtual ~EECCVolcano();                 // 0xb4670 (??_G 0xb4654)
    virtual bool Start(EECalamity* inCalamity);                  // +0x04 0xb4af3
    virtual void Stop(EECalamity* inCalamity);                   // +0x08 0xb4e2d
    // the class's own steps of its Rust Start and Stop (ee/EECalamityUpdate.h: EeCalamityOp::ClassStep)
    static bool StartStep(EECalamityUpdateHost& ioHost, const EeCalamityCall& inCall, EeCalamityReply& outReply);
    virtual void Initialize(EECalamityData* inData);             // +0x20 0xb4f72
    virtual void Reset(EECalamity* inCalamity);                  // +0x24 (empty: ICF-folded 0x157269)
    virtual void LoadMedia(EEPlayer* inPlayer, bool inIsOnDemand);  // +0x28 0xb5023
    virtual void PlaySound(EECalamity* inCalamity);              // +0x2c 0xb508b: starts the sound
    virtual void Place(EEUnit* inUnit, unsigned long inX, unsigned long inY);       // (the volcano reads inUnit->mPlayer) +0x30 0xb5137
    virtual void UnloadMedia(EEPlayer* inPlayer);                // +0x34 0xb50ec
    virtual long Update(EECalamity* inCalamity, float inElapsedTime);   // +0x38 0xb516c
    // the class's own world steps of its Rust update (ee/EECalamityUpdate.h)
    static bool UpdateStep(EECalamityUpdateHost& ioHost, const EeCalamityCall& inCall, EeCalamityReply& outReply);
    virtual bool IsValidLocation(EEComplexUnit* inUnit, unsigned long inX, unsigned long inY);  // +0x4c 0xb523e

    // 0xb4e74: a random location of a map tile between mMinimumRadius and mMaximumRadius around inCenter, at the
    // tile's height (another unit calls it: 0x20e879) (provisional name)
    void GetRandomLocation(const GE3DPoint& inCenter, GE3DPoint& outLocation);

protected:
    // Provisional names (by behaviour)
    // 0xb46bc (cdecl): moves the units on the inClass-sized square at (inX, inY) to the nearest free tiles around it
    static void ClearArea(EEUnitClass* inClass, long inX, long inY);
    float GetMaximumHeight(const GE3DPoint& inLocation);   // 0xb4b62: the highest terrain point around inLocation
    bool CreateVolcano(EECalamity* inCalamity);  // 0xb4d19
    // empty, ICF-folded 0x163d6f (Update calls it directly)
    void UpdateVolcano(EECalamity* inCalamity, float inElapsedTime);

    EEGraphicEffect* mGraphicEffect1;       // +0x28 (record +0x94; placed at Start; its delay starts the volcano)
    EEGraphicEffect* mGraphicEffect2;       // +0x2c (record +0x98; on the volcano unit)
    EEGraphicEffect* mGraphicEffect3;       // +0x30 (record +0x9c; on the volcano unit)
    EEGraphicEffect* mGraphicEffect4;       // +0x34 (optional; not set by Initialize)
    EESoundObject* mSound;                  // +0x38 (record +0xd4)
    long mL3C;                              // +0x3c (record +0xb8)
    long mL40;                              // +0x40 (record +0xbc)
    long mUnitClassIndex;                   // +0x44 (record +0xb0: gUnitClassTable index, the volcano)
    long mL48;                              // +0x48 (record +0xc8)
    float mMaximumRadius;                   // +0x4c (record +0xc4)
    float mMinimumRadius;                   // +0x50 (record +0xc0)
};

#endif
