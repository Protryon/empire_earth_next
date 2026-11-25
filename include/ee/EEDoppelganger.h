// EEDoppelganger.h (RECONSTRUCTION; provisional names).
// Layout proven: EEDoppelganger sizeof 0x10 = its new-site size, ctor 0x201d85 exact in EEDoppelganger.cpp. Member
// names are provisional.
// What another player sees of a unit he no longer sees (RTTI EEDoppelganger > FSRestorable; vtable 0x446e7c; sizeof
// 0x10; Units/EEDoppelganger.cpp 0x201d85..0x201fd9). Its unit and player are saved as ids and repaired on load.
// New (units round 11): the bodies are inline here (the Rust ones: ee_sim units dm_dop.rs); the four that read the
// unit, its class or its owner are EEDoppelgangerManager.cpp's (EEDoppelganger.cpp is gone). Requires
// lle/FSRestorable.h.
#ifndef EE_EEDOPPELGANGER_H
#define EE_EEDOPPELGANGER_H

#include "rust/sim/units.h"             // the inline bodies' Rust calls

class EEComplexDoppelganger;
class EEGraphicObject;
typedef EEGraphicObject EEModel;   // ee/EEGraphicObject.h (as ee/EEUnit.h)
class EEPlayer;
class EEUnit;
class RTSTile;
class GETransformation;

class EEDoppelganger : public FSRestorable {
public:
    EEDoppelganger() { this->Reset(); }    // 0x201d85: no unit, viewing player or tile
    // FSRestorable (its GetChecksum is the engine's)
    // +0x04 0x201e9a: the unit id and viewing player id from the saved game; the second read's result
    virtual long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile) { return (long)ee_units_dm_dop(EE_UNITS_DM_DG_DO_LOAD, this, &inFile, NULL, NULL); }
    // +0x08 0x201ecd: the loaded ids back into unit and player, a simple one put on its tile; a simple one that lost
    // either is freed (0), a complex one returns 9
    virtual long RepairPointers(unsigned long inFirst, unsigned long inSecond) { return (long)ee_units_dm_dop(EE_UNITS_DM_DG_DO_REPAIR_POINTERS, this, NULL, NULL, NULL); }
    // +0x0c 0x201f65: the complex flag, unit id and viewing player id if the unit is valid; the first write error
    virtual long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile) { return (long)ee_units_dm_dop(EE_UNITS_DM_DG_DO_SAVE, this, &outFile, NULL, NULL); }
    // (overloaded virtuals take their slots in reverse declaration order)
    // +0x14 0x201e5d: another doppelganger's unit and tile for a new viewing player (the tile argument unused)
    virtual void Initialize(EEDoppelganger* inOther, EEPlayer* inPlayer, RTSTile* inTile) { ee_units_dm_dop(EE_UNITS_DM_DG_DO_INITIALIZE_COPY, this, inOther, inPlayer, inTile); }
    // +0x10 0x201e24: a unit on a tile for a viewing player, referenced (a simple one's media loaded)
    virtual void Initialize(EEUnit* inUnit, EEPlayer* inPlayer, RTSTile* inTile) { ee_units_dm_dop(EE_UNITS_DM_DG_DO_INITIALIZE, this, inUnit, inPlayer, inTile); }
    // the trivial slots (ICF-folded into other units' identical bodies): a simple doppelganger has no animation, frame
    // or complex part, counts as built, is neither a foundation nor complex, has no transformation and keeps its tile
    virtual long GetAnimation() { return 0; }   // +0x18
    virtual EEComplexDoppelganger* GetComplexDoppelganger() { return NULL; }  // +0x1c (EEComplexDoppelganger's: this)
    virtual bool IsBuilt() { return true; }     // +0x20
    virtual long GetFrame() { return 0; }       // +0x24
    virtual EEModel* GetModel();            // +0x28 0x201d97: the player's model of the unit's class
    virtual bool IsFoundation() { return false; }   // +0x2c
    virtual bool IsComplex() { return false; }  // +0x30 (true: no model to show)
    virtual void GetTransformation(GETransformation&) {}   // +0x34 (EEComplexDoppelganger's hands out its location)
    virtual float GetScale();               // +0x38 0x201db7: the unit class's +0x130
    virtual long GetHitPoints() { return -1; }   // +0x3c: a doppelganger shows no hit points
    virtual RTSTile* GetTile() { return this->mTile; }   // +0x40 (ICF-folded)
    virtual long GetTileX() { return -10000; }   // +0x44: off the map
    virtual long GetTileY() { return -10000; }   // +0x48 (folded with +0x44)
    virtual ~EEDoppelganger() { this->Clear(); }   // +0x4c 0x201dce (??_E 0x203433): its unit and model media released
    // +0x50 0x201de5: the remembered unit (and a simple doppelganger's model media) released, then reset
    virtual void Clear() { ee_units_dm_dop(EE_UNITS_DM_DG_DO_CLEAR, this, NULL, NULL, NULL); }

    // the unit, viewing player and tile forgotten without releasing anything
    void Reset() { ee_units_dm_dop(EE_UNITS_DM_DG_DO_RESET, this, NULL, NULL, NULL); }
    bool GetIsValid();                      // 0x201e16: the unit's mIsValid
    EEPlayer* GetPlayer();                  // 0x201e1d: the unit's owner

    RTSTile* mTile;                         // +0x04
    EEUnit* mUnit;                          // +0x08 (referenced; its id between Load and RepairPointers)
    EEPlayer* mPlayer;                      // +0x0c (the player who sees it; its index between Load and RepairPointers)
};

#endif
