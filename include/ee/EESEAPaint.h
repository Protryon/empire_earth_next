// C:\EEX\Empire Earth\Scenario Editor Actions\EESEAPaint.h (RECONSTRUCTION; file name inferred).
// The base of the editor's painting actions (RTTI EESEAPaint > EEAction; vtable 0x445928: slot 0 the shared ??_G
// 0x1e1b69 (an implicit destructor, ICF-folded with EEAction's), slots 4/8/0x0c pure). Derived: EESEAPaintElevation
// (ee/EEUIFSEPaintElevation.h), EESEAPaintMoutains and EESEAPaintTerrain (ee/EESEAPaintTerrain.h). The inline ctor is
// emitted expanded except where EEAction's ctor is refused (EESEACreateBlankMap 0x1a59bd: the EEAction COMDAT
// 0x1a6695, then this vptr). Requires ee/EEAction.h.
#ifndef EE_EESEAPAINT_H
#define EE_EESEAPAINT_H

#include <stdint.h>

// The paint actions' queued areas are a Rust record each (ee_screens editor_actions.rs; the original's deques of
// EESEAPaintRect / EESEAPaintRegion): created by the action's ctor, freed by its dtor (rust/ee_screens.h declares
// these too; here so the actions' inline ctors need no Rust header)
struct EeScreensSeaPaint;
extern "C" EeScreensSeaPaint* ee_screens_sea_paint_new(uint32_t kind, int32_t a, int32_t b);
extern "C" void ee_screens_sea_paint_free(EeScreensSeaPaint* paint);
enum { kSEAPaintTerrain = 0, kSEAPaintElevation = 1, kSEAPaintBulldozer = 2 };   // EE_SCREENS_SEA_PAINT_*

class EESEAPaint : public EEAction {
public:
    EESEAPaint() : EEAction(false) {}
    // +0x0c: the area to paint, in tiles (left, top, right, bottom); true when anything changed (provisional name)
    virtual bool SetArea(long inLeft, long inTop, long inRight, long inBottom) = 0;
};

#endif
