// EEGhostUnit.h (RECONSTRUCTION from RTTI/vtables/ctors; one header per class; provisional names). Classes:
// EEGhostUnit.
// Names, argument/return types and member types inferred (signatures from `ret N` and the bodies, ctor
// arguments from their stores, members from layout.py); PLACEHOLDER members cover objects not decoded yet.
// Requires the engine declarations of: GETransformation.
#ifndef EE_EEGHOSTUNIT_H
#define EE_EEGHOSTUNIT_H

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
class EEGhostUnit {
public:
    // 0x20360e: loads the class's media (directly when gFreeCamera, else through the pre-loading thread unless
    // inNoMedia)
    EEGhostUnit(long inUnitClass, EEPlayer* inPlayer, bool inNoMedia);
    // 0x204278 (cdecl; news an EEGhostUnit(type, player, 0) under a lock; EESELoadThread Handle402 passes the editor's
    // mouse location, the class's +0x1bc, the player, a rotation and false; provisional name)
    static void Create(const GE2DPoint& inLocation, long inUnitType, EEPlayer* inPlayer, float inRotation, bool inIsMove);
    virtual ~EEGhostUnit();                 // +0x00 0x2036a7 (??_G 0x20368b): unloads the class's media
    virtual void AddChecksum(ESChecksum& ioChecksum);   // +0x04 0x2048f4
    // +0x08 0x2049c0 / +0x0c 0x204a48: the class's type and the player's id (ids between Load and the repair)
    virtual long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile);
    virtual long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile);
    void Reset();                           // 0x204208 (provisional name)
    void FillDockSquare();                 // original thiscall, ret0
    void FillRimSquare();                  // original thiscall, ret0
    void FillSquare();                     // original thiscall, ret0
    void FillVolcanoSquare();              // original thiscall, ret0
    void UpdateFootprint();                 // 0x203cb1: mCanPlace from the units on the tile, copied over the footprint (provisional name)
    float GetFootprintOffset();             // 0x20496b: 1 up to size 1, else half the class's +0x28c (rounded up when odd) (provisional name)
    // 0x2049fc (cdecl: a static member; the tile next to inGhost's in direction inDirection, 0x24cbd7) (provisional
    // name)
    static void SetFarmSlotTile(EEGhostUnit* inGhost, long inDirection);
    // 0x2044d4 / 0x204f02 (cdecl): the gate placement's ghost (dynamic_casts inClass to EEUCGate; inRotation applies
    // when the class's mWoodGatherTime is set) and its update at the mouse location (writes the direction back;
    // EEUIBuildGate:: Update 0x25627d) (provisional names)
    static void CreateGate(float inRotation, EEUnitClass* inClass, EEPlayer* inPlayer);
    static void PreviewGate(const GE2DPoint& inLocation, long& ioDirection);
    void SetAnimation(long inIndex);        // 0x204a97 (provisional name)
    // inline in the real header (out-of-line COMDAT 0x10cb62, first emitted elsewhere): the player's model of the
    // class (mPlayer->mU884[type]->mModel); declared out of line here (provisional name)
    EEModel* GetModel();
    float GetScale();                       // 0x10cb81 (the class upgrade's mF98; out-of-line COMDAT next to GetModel) (provisional name)

    GETransformation mTransformation;       // +0x004 (Reset zeroes its translation)
    float mHeights[144];                     // +0x044 (Reset: memset 0x240)
    long mTileX;                            // +0x284
    long mTileY;                            // +0x288
    long mFrame;                            // +0x28c
    long mAnimation;                        // +0x290 (= 7)
    EEPlayer* mPlayer;                      // +0x294
    EEUnitClass* mUnitClass;                // +0x298 (gUnitClassTable[inUnitClass])
    long mUnitSet;                          // +0x29c
    bool mFootprint[12][12];                // +0x2a0 (Reset: memset 0x90; 0x203cb1 fills it with mCanPlace) (provisional name)
    bool mCanPlace;                         // +0x330
    bool mNoMedia;                          // +0x331
    short mU332;                            // +0x332
};

// 0x563240: cleared by EEGhostUnit::Create once the new ghosts are in the world (provisional name)
extern bool gGhostUnitsChanged;
// 0x20ed25 (cdecl; EEUCBuilding.cpp's range): the wall piece kind for slot inIndex of a gate/wall ghost; sets *outFlag
// for the piece that gets frame 0xb (provisional name)
long EEGetWallPiecePart(long inIndex, bool* outFlag);

#endif
