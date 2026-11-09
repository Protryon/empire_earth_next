// C:\EEX\Empire Earth\Graphics Thread\EEE3DEnergyWall.h (RECONSTRUCTION; file name inferred).
// The energy wall (gate) effect: ground strips from a unit (or from both posts of a gate) towards the neighbouring
// friendly wall pieces (vtable 0x4406dc; RTTI EEE3DEnergyWall > EEEffect3D; EEE3DEnergyWall.cpp 0xed606..0xee1b4, key
// chain "EEE3DEnergyWall-Data"; the effect manager creates it with kind 0x34). sizeof 0x120 (operator new 0x107045).
// Names are provisional. Requires ee/EEEffect3D.h, lle/GEColor.h, lle/GETextureCoordinate.h, <vector>.
#ifndef EE_EEE3DENERGYWALL_H
#define EE_EEE3DENERGYWALL_H

#include <vector>

class EEGraphicObject;

// 0x24c3df (cdecl; also declared locally by EECPWallManager.cpp): the units on tile (inX, inY) (provisional name)
long EEGetUnitsOnTile(long inX, long inY, std::vector<EEUnit*>& outUnits);  // returns their count (eax from 0x24c433)
// 0x103ac8 (cdecl, in EEEffect.cpp's range; MISSING there): a textured ground strip from inFrom to inTo of width inWidth
// (the ground batch). Declared here because only this unit calls it so far; types from the pushes (provisional name)
void AddGroundStrip(const GE3DPoint& inFrom, const GE3DPoint& inTo, float inWidth, const GEColor& inColor,
              const GETextureCoordinate& inOffset, const GETextureCoordinate& inSize, float inAlpha, float inBrightness,
              float inAlphaReference);

class EEE3DEnergyWall : public EEEffect3D {
public:
    EEE3DEnergyWall(EEUnit* inUnit, EEGraphicEffect* inGraphicEffect, long inKind);   // 0xed649
    EEE3DEnergyWall(long inKind);           // 0xed6eb
    virtual ~EEE3DEnergyWall();             // +0x00 0xed745 (??_G 0xed6cf)
    virtual long DrawPass2();               // +0x08 (`return 0`: ICF-folded into 0xfca98)
    virtual long DrawPass4();               // +0x10 (likewise)
    virtual long DrawPass5();               // +0x14 (likewise)
    virtual long DrawPass6();               // +0x18 0xedb6c (the draw: AddGroundStrip per segment)
    virtual long Queue(EEEffectQueue* inQueues);       // +0x24 0xed9d0 (alpha, colour, texture frame; queue 4)
    virtual void PrepareQueue();            // +0x2c (empty: ICF-folded into 0xe123d)
    virtual long SetUp();                   // +0x30 0xedb58 (loads the graphic)
    virtual long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile);    // +0x38 0xedce0
    virtual long RepairPointers();          // +0x3c (EEEffect::RepairPointers: ICF-folded into EEE3DBlastWave's 0xe93e0)
    virtual long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile);   // +0x40 0xede8b
    virtual long Step();                    // +0x4c 0xedf19 (places once; finds the segments every 500 ms)
    virtual bool IsTileVisible(EEMapTile* inTile);  // +0x54 0xed98d (also true where a doppelganger is seen)
    // +0x58 0xedd9c: Load for files before version 0.250 (two points, a float, 18 points: read and dropped; then
    // placed again)
    virtual long LoadOldVersion(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile);

    void Place();                           // 0xed784: the posts and the segment ends (provisional name)
    void FindSegments();                    // 0xedf5a: the segments from both posts (provisional name)
    // 0xee00c: segments inFrom..inTo-1 towards the friendly wall pieces around tile (inX, inY) (provisional name)
    void FindSegments(long inX, long inY, long inFrom, long inTo);

    static const UString sData;             // "EEE3DEnergyWall-Data" (0x4e8b58; provisional name)

    EEGraphicObject* mGraphicObject;        // +0x74
    long mL78;                              // +0x78 (not used here)
    float mAlpha;                           // +0x7c (the effect's mF30; 0 for a cloaked enemy)
    float mGateAlpha;                       // +0x80 (the segment between a gate's posts: by the gate's mOpenFraction)
    GETextureCoordinate mTextureOffset;     // +0x84 (the animation frame: (frame & 7, frame >> 2 & 7) * 0.14)
    bool mPlaced;                           // +0x8c (saved)
    bool mSeen;                             // +0x8d (Virtual54: a doppelganger; resets the frame once)
    GEColor mColor;                         // +0x90 (the owner's colour)
    bool mGate;                             // +0x9c (two posts; saved)
    GE3DPoint mPoints[10];                  // +0xa0 [0]/[9] the posts, [1..4]/[5..8] the segment ends (saved)
    unsigned long mSegments;                // +0x118 bit i: segment i is drawn (saved)
    long mTimer;                            // +0x11c ms to the next FindSegments (saved)
};

#endif
