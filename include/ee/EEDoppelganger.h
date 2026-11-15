// EEDoppelganger.h (RECONSTRUCTION; provisional names).
// Layout proven: EEDoppelganger sizeof 0x10 = its new-site size, ctor 0x201d85 exact in EEDoppelganger.cpp. Member
// names are provisional.
// What another player sees of a unit he no longer sees (RTTI EEDoppelganger > FSRestorable; vtable 0x446e7c; sizeof
// 0x10; Units/EEDoppelganger.cpp 0x201d85..0x201fd9). Its unit and player are saved as ids and repaired on load.
// Requires lle/FSRestorable.h.
#ifndef EE_EEDOPPELGANGER_H
#define EE_EEDOPPELGANGER_H

class EEComplexDoppelganger;
class EEGraphicObject;
typedef EEGraphicObject EEModel;   // ee/EEGraphicObject.h (as ee/EEUnit.h)
class EEPlayer;
class EEUnit;
class RTSTile;
class GETransformation;

class EEDoppelganger : public FSRestorable {
public:
    EEDoppelganger();                       // 0x201d85
    // FSRestorable (its GetChecksum is the engine's)
    virtual long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile);   // +0x04 0x201e9a
    virtual long RepairPointers(unsigned long inFirst, unsigned long inSecond);   // +0x08 0x201ecd
    virtual long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile);  // +0x0c 0x201f65
    // (overloaded virtuals take their slots in reverse declaration order)
    virtual void Initialize(EEDoppelganger* inOther, EEPlayer* inPlayer, RTSTile* inTile);   // +0x14 0x201e5d
    virtual void Initialize(EEUnit* inUnit, EEPlayer* inPlayer, RTSTile* inTile);   // +0x10 0x201e24
    virtual long GetAnimation();            // +0x18 (0)
    virtual EEComplexDoppelganger* GetComplexDoppelganger();  // +0x1c (0; EEComplexDoppelganger's: this)
    virtual bool IsBuilt();                 // +0x20 (true)
    virtual long GetFrame();                // +0x24 (0)
    virtual EEModel* GetModel();            // +0x28 0x201d97: the player's model of the unit's class
    virtual bool IsFoundation();            // +0x2c (false)
    virtual bool IsComplex();               // +0x30 (false; true: no model to show)
    virtual void GetTransformation(GETransformation& outTransformation);  // +0x34 (empty; EEComplexDoppelganger's hands out its location)
    virtual float GetScale();               // +0x38 0x201db7: the unit class's +0x130
    virtual long GetHitPoints();            // +0x3c (-1)
    virtual RTSTile* GetTile();             // +0x40 (mTile; ICF-folded)
    virtual long GetTileX();                // +0x44 (-10000)
    virtual long GetTileY();                // +0x48 (-10000; folded with +0x44)
    virtual ~EEDoppelganger();              // +0x4c 0x201dce (??_E 0x203433)
    virtual void Clear();                   // +0x50 0x201de5

    void Reset();                           // 0x201dd9
    bool GetIsValid();                      // 0x201e16: the unit's mIsValid
    EEPlayer* GetPlayer();                  // 0x201e1d: the unit's owner

    RTSTile* mTile;                         // +0x04
    EEUnit* mUnit;                          // +0x08 (referenced; its id between Load and RepairPointers)
    EEPlayer* mPlayer;                      // +0x0c (the player who sees it; its index between Load and RepairPointers)
};

#endif
