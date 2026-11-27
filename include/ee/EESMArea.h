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
#include "ee/EEAreaUtilities.h"
#include "rust/sim/scenario.h"

class EEUnit;

// the numbers (the anchor, the rectangle, the cached centre tile, the select mode, the tile time, the id, the two
// flags) are the Rust record's (ee_scenario area_fields.rs), with GERectangle's and U2DPoint<long>'s layouts
class EESMArea : public EeScenarioAreaFields {
public:
    // Creates an unnamed rectangle-mode area with no rectangle or anchor and no cached centre tile
    EESMArea() : EeScenarioAreaFields(ee_scenario_area_defaults()) {  }
    // Destroys the area (members only)
    ~EESMArea() {  }
    // Reads the area's name, text, anchor (as floats), rectangle, flags, select mode, tile time and ID; returns the first
    // error (ee_scenario save.rs area_load)
    // +0x00 0x1ba670
    virtual long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile) { return ee_scenario_en_area_io(this, &inFile, false); }
    // Writes the area's name, text, anchor, rectangle, flags, select mode, tile time and ID; returns the first error
    // +0x04 0x1ba78e
    virtual long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile) { return ee_scenario_en_area_io(this, &outFile, true); }
    // Returns whether a tile is in the area: inside the rectangle, on the anchor's continent, or inside the hull of a
    // non-Gaia town that contains the anchor
    bool ContainsTile(const U2DPoint<long>& inTile) { return ee_scenario_en_area_contains_tile(this, inTile.mX, inTile.mY); }
    // Copies another area's name, text, rectangle, anchor, select mode and ID (not its tile cache)
    // 0x1ba2b2 (EEScenarioManager 0x1b12ff; provisional name)
    void Copy(const EESMArea& inArea) { ee_scenario_en_area_copy(this, (void*)&inArea); }
    // Copies the area's tiles out, rebuilding the cache when empty or unused for 3000 ticks; every cached read
    // restarts the timer
    // 0x1ba495: real typed cache/output family
    void GetTiles(std::vector<U2DPoint<long> >& outTiles) { ee_scenario_en_area_tiles(this, &outTiles); }
    const UWideString& GetName() const { return this->mName; }  // inline (DuplicateArea 0x5b120d)
    // inline (COMDAT 0x17d381, first emitted by the editor's area screen; Reset 0x1afbb1 calls it) (provisional name)
    void SetAnchor(const U2DPoint<long>& inAnchor) { ee_scenario_en_area_set_anchor(this, inAnchor.mX, inAnchor.mY); }
    // inline (COMDAT 0x17d34c, before SetAnchor's: the editor's area screen 0x17d272 calls both) (provisional name)
    void SetRectangle(const GERectangle& inRectangle) { ee_scenario_en_area_set_rectangle(this, reinterpret_cast<const EeScenarioAreaRect*>(&inRectangle)); }

    const UWideString& GetText() const { return this->mText; }
    const GERectangle& GetRect() const { return *reinterpret_cast<const GERectangle*>(&this->mRect); }
    const U2DPoint<long>& GetAnchor() const { return *reinterpret_cast<const U2DPoint<long>*>(&this->mAnchor); }
    unsigned long GetSelectMode() const { return this->mSelectMode; }
    bool GetHasRectangle() const { return this->mHasRectangle; }
    bool GetHasAnchor() const { return this->mHasAnchor; }
    UWideString mName;                     // (ctor: empty)
    UWideString mText;
    std::vector<U2DPoint<long> > mTiles;   // the tile cache (`_Buy` 0x14823b; `_Tidy` 0xa9c2c in the dtor)
};

#endif
