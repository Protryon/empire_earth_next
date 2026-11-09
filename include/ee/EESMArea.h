// C:\EEX\Empire Earth\Scenario Manager\EESMArea.h (RECONSTRUCTION; file name inferred). One scenario area (RTTI
// EESMArea, a root class; vtable 0x446038 with two slots: +0x00 Load 0x1ba670 and +0x04 Save 0x1ba78e, both `ret
// 0xc`). Layout from the ctor 0x1ba02f (the dtor 0x1ba0ac is not virtual and stores the vptr): vptr, two
// UWideStrings, the anchor tile (+0x2c, a U2DPoint<long>: 0x1ba154 passes its address to U2DConvexHull<long>'s
// 0xd16e), a GERectangle (+0x34: the ctor's top/left/bottom/right fldz stores and the copy 0x1ba2b2's are its
// inline default ctor and operator=; 0x1ba315 calls GetCenter on it), a
// vector<U2DPoint<long>>, the two -10000 "no area" longs. Load/Save (0x1ba670/0x1ba78e) type the rest
// Member names by offset (provisional names).
// Requires lle/UWideString.h, lle/GE2DPoint.h, lle/GERectangle.h, lle/FSFile.h.
#ifndef EE_EESMAREA_H
#define EE_EESMAREA_H

#include <vector>
#include "lle/UWideString.h"
#include "lle/U2DPoint.h"
#include "ee/EEAreaUtilities.h"

class EEUnit;

class EESMArea {
public:
    EESMArea();                             // 0x1ba02f
    ~EESMArea();                            // 0x1ba0ac
    virtual long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile);   // +0x00 0x1ba670
    virtual long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile);  // +0x04 0x1ba78e
    // 0x1ba0d1: whether inUnit is in this area (EESMObject's 0x1c6458 asks; provisional name)
    bool ContainsUnit(EEUnit* inUnit);
    bool ContainsTile(const U2DPoint<long>& inTile);   // 0x1ba154 (provisional name)
    void Copy(const EESMArea& inArea);      // 0x1ba2b2 (EEScenarioManager 0x1b12ff; provisional name)
    U2DPoint<long> GetCenterTile();         // 0x1ba315: hidden-result ABI; by-value spelling inferred
    void GetTiles(std::vector<U2DPoint<long> >& outTiles);   // 0x1ba495: real typed cache/output family
    const UWideString& GetName() const { return this->mName; }  // inline (DuplicateArea 0x5b120d)
    bool IsUnset() const;                   // 0x1ba45e
    bool IsOffMap() const;                  // 0x1ba473: the anchor has a negative coordinate (provisional name)
    // inline (COMDAT 0x17d381, first emitted by the editor's area screen; Reset 0x1afbb1 calls it) (provisional name)
    void SetAnchor(const U2DPoint<long>& inAnchor)
    {
        this->mHasAnchor = true;
        this->mAnchor = inAnchor;
        this->mTiles.clear();
        this->mCenterTile.mX = -10000;
    }
    // inline (COMDAT 0x17d34c, before SetAnchor's: the editor's area screen 0x17d272 calls both) (provisional name)
    void SetRectangle(const GERectangle& inRectangle)
    {
        this->mHasRectangle = true;
        this->mRect = inRectangle;
        this->mTiles.clear();
        this->mCenterTile.mX = -10000;
    }

    const UWideString& GetText() const { return this->mText; }
    const GERectangle& GetRect() const { return this->mRect; }
    const U2DPoint<long>& GetAnchor() const { return this->mAnchor; }
    unsigned long GetSelectMode() const { return this->mSelectMode; }
    bool GetHasRectangle() const { return this->mHasRectangle; }
    bool GetHasAnchor() const { return this->mHasAnchor; }
    UWideString mName;                     // +0x04 (ctor: assigned "")
    UWideString mText;                      // +0x18
    U2DPoint<long> mAnchor;                 // +0x2c
    GERectangle mRect;                      // +0x34
    std::vector<U2DPoint<long> > mTiles;   // +0x44 (`_Buy` 0x14823b; `_Tidy` 0xa9c2c in the dtor)
    U2DPoint<long> mCenterTile;             // +0x54 (ctor: zeroed by its ctor, then (-10000, -10000); 0x1ba315 returns it)
    unsigned long mSelectMode;              // +0x5c (Load/Save: unsigned long; 0x1ba154 switches on it: 0, 1, 2)
    unsigned long mTilesTime;               // +0x60
    unsigned long mID;                      // +0x64
    bool mHasRectangle;                     // +0x68
    bool mHasAnchor;                        // +0x69
};

#endif
