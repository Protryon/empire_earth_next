// EEGhostUnit.h (RECONSTRUCTION from RTTI/vtables/ctors; one header per class; provisional names). Classes:
// EEGhostUnit.
// Names, argument/return types and member types inferred (signatures from `ret N` and the bodies, ctor
// arguments from their stores, members from layout.py); PLACEHOLDER members cover objects not decoded yet.
// Requires the engine declarations of: GETransformation.
#ifndef EE_EEGHOSTUNIT_H
#define EE_EEGHOSTUNIT_H
#include "rust/sim/units.h"             // the inline bodies' Rust calls

// vtable 0x446ef4; bases EEGhostUnit; sizeof 0x334; ctor 0x20360e; ??_G 0x20368b; dtor 0x2036a7
class EEGraphicObject;
typedef EEGraphicObject EEModel;   // ee/EEGraphicObject.h (as ee/EEUnit.h)
class EEPlayer;
class EEUnitClass;
class ESChecksum;
class FSFile;
class GE2DPoint;

// A unit placed but not yet built (the editor's preview; EEGhostUnit.cpp 0x20360e..0x20560c). Member names and types
// from the ctor, Reset 0x204208 and the slots (provisional name)
class EEGhostUnit : public EeUnitsGhostFields {
public:
    // 0x20360e: loads the class's media (directly when gFreeCamera, else through the pre-loading thread unless
    // inNoMedia)
    EEGhostUnit(long inUnitClass, EEPlayer* inPlayer, bool inNoMedia) { ee_units_ghost_more(EE_UNITS_GHOST_M_DO_CONSTRUCT, this, inPlayer, inUnitClass, inNoMedia); }
    // 0x204278 (cdecl; news an EEGhostUnit(type, player, 0) under a lock; EESELoadThread Handle402 passes the editor's
    // mouse location, the class's +0x1bc, the player, a rotation and false; provisional name)
    static void Create(const GE2DPoint& inLocation, long inUnitType, EEPlayer* inPlayer, float inRotation,
                         bool inIsMove) { ee_units_ghost_create(inUnitType, inPlayer, inRotation, inIsMove); }
    virtual ~EEGhostUnit() { ee_units_ghost_more(EE_UNITS_GHOST_M_DO_DESTROY, this, NULL, 0, 0); }                 // +0x00 0x2036a7 (??_G 0x20368b): unloads the class's media
    virtual void AddChecksum(ESChecksum& ioChecksum) { ee_units_ghost_more(EE_UNITS_GHOST_M_DO_ADD_CHECKSUM, this, &ioChecksum, 0, 0); }   // +0x04 0x2048f4
    // +0x08 0x2049c0 / +0x0c 0x204a48: the class's type and the player's id (ids between Load and the repair)
    virtual long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile) { return (long)ee_units_ghost_more(EE_UNITS_GHOST_M_DO_LOAD, this, &inFile, 0, 0); }
    virtual long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile) { return (long)ee_units_ghost_more(EE_UNITS_GHOST_M_DO_SAVE, this, &outFile, 0, 0); }
    // 0x2049fc (cdecl: a static member; the tile next to inGhost's in direction inDirection, 0x24cbd7) (provisional
    // name)
    static void SetFarmSlotTile(EEGhostUnit* inGhost, long inDirection) { ee_units_ghost_more(EE_UNITS_GHOST_M_DO_SET_FARM_SLOT_TILE, inGhost, NULL, inDirection, 0); }
    static void PreviewGate(const GE2DPoint& inLocation, long& ioDirection) { ee_units_ghost_body(EE_UNITS_GHOST_DO_PREVIEW_GATE, NULL, &inLocation, &ioDirection); }
    void SetAnimation(long inIndex) { ee_units_ghost_more(EE_UNITS_GHOST_M_DO_SET_ANIMATION, this, NULL, inIndex, 0); }
    // inline in the real header (out-of-line COMDAT 0x10cb62, first emitted elsewhere): the player's model of the
    // class (mPlayer->mU884[type]->mModel); declared out of line here (provisional name)
    EEModel* GetModel();
    float GetScale();                       // 0x10cb81 (the class upgrade's mF98; out-of-line COMDAT next to GetModel) (provisional name)

    GETransformation mTransformation;       // +0x004 (Reset zeroes its translation)
    // +0x044 mHeights .. +0x332 mU332: the record EeUnitsGhostFields (ee_sim units obj_fields.rs)
};

// 0x563240: cleared by EEGhostUnit::Create once the new ghosts are in the world (provisional name)
extern bool gGhostUnitsChanged;
long EEGetWallPiecePart(long inIndex, bool* outFlag);

#endif
