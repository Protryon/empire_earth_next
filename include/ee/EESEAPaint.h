// C:\EEX\Empire Earth\Scenario Editor Actions\EESEAPaint.h (RECONSTRUCTION; file name inferred).
// The base of the editor's painting actions (RTTI EESEAPaint > EEAction; vtable 0x445928: slot 0 the shared ??_G
// 0x1e1b69 (an implicit destructor, ICF-folded with EEAction's), slots 4/8/0x0c pure). The original's three derived
// actions (EESEAPaintTerrain, vtable 0x445914; EESEAPaintElevation, 0x445604; EESEAPaintMoutains, 0x4459dc) are one
// class here, EESEAPaintRust: their runs and areas were Rust's already (ee_screens editor_actions.rs), their classes
// only held the record. Requires ee/EEAction.h.
#ifndef EE_EESEAPAINT_H
#define EE_EESEAPAINT_H

#include <stdint.h>

// The paint actions' queued areas are a Rust record each (ee_screens editor_actions.rs; the original's deques of
// EESEAPaintRect / EESEAPaintRegion): created by the action's ctor, freed by its dtor (rust/ee_screens.h declares
// these too; here so the actions' inline ctors need no Rust header)
struct EeScreensSeaPaint;
extern "C" EeScreensSeaPaint* ee_screens_sea_paint_new(uint32_t kind, int32_t a, int32_t b);
extern "C" void ee_screens_sea_paint_free(EeScreensSeaPaint* paint);
extern "C" uint8_t ee_screens_sea_paint_set_area(EeScreensSeaPaint* paint, int32_t left, int32_t top, int32_t right,
                                                 int32_t bottom);
extern "C" void ee_screens_sea_paint_run(EeScreensSeaPaint* paint);
enum { kSEAPaintTerrain = 0, kSEAPaintElevation = 1, kSEAPaintBulldozer = 2 };   // EE_SCREENS_SEA_PAINT_*

class EESEAPaint : public EEAction {
public:
    EESEAPaint() : EEAction(false) {}
    // +0x0c: the area to paint, in tiles (left, top, right, bottom); true when anything changed (provisional name)
    virtual bool SetArea(long inLeft, long inTop, long inRight, long inBottom) = 0;
};

// The paint actions (kSEAPaintTerrain: a the EETerrainTable index, b the texture (the table entry's mL68);
// kSEAPaintElevation: a the level (a char); kSEAPaintBulldozer): Virtual4 paints each queued area, SetArea queues one
// when the paint changes anything there (the terrain's: any tile on the map; the elevation's: any point off the
// level's height; the bulldozer's: always). The original's EESEAPaintTerrain::Virtual4 0x1a8e6c, SetArea 0x1a8f45,
// PlantTerrainUnits 0x1a8d3e; EESEAPaintElevation::Virtual4 0x1a830a, SetArea 0x1a8507; EESEAPaintMoutains::Virtual4
// 0x1a86ff, SetArea 0x1a88ea (ee_screens editor_actions.rs)
class EESEAPaintRust : public EESEAPaint {
public:
    EESEAPaintRust(uint32_t inKind, int32_t inA = 0, int32_t inB = 0) : mPlan(ee_screens_sea_paint_new(inKind, inA, inB)) {}
    virtual ~EESEAPaintRust() { ee_screens_sea_paint_free(this->mPlan); }
    virtual void Virtual4() { ee_screens_sea_paint_run(this->mPlan); }
    virtual void Virtual8() {}
    virtual bool SetArea(long inLeft, long inTop, long inRight, long inBottom)
    {
        return ee_screens_sea_paint_set_area(this->mPlan, inLeft, inTop, inRight, inBottom) != 0;
    }

    EeScreensSeaPaint* mPlan;               // the queued areas (the original's deques; editor_actions.rs)
};

#endif
