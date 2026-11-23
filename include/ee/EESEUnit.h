// C:\EEX\Empire Earth\Scenario Editor\EESEUnit.h (RECONSTRUCTION; file name inferred).
// The editor's unit tab group (once RTTI EESEUnit > EEUIFSEMainGroup; vtable 0x444e9c, sizeof 0x140, ctor 0x19a93e); the
// editor's tab group 3. EESEUnit.cpp is 0x199afd..0x19cc39 (its helper controls first, then the ctor and the members).
// Layout from the ctor and the dtor 0x19bb38; member names are provisional. Requires ee/EEUIFSEMainGroup.h,
// lle/GERectangle.h (includes the build-mode headers itself).
#ifndef EE_EESEUNIT_H
#define EE_EESEUNIT_H

#include "lle/GERhombus.h"
#include "ee/EEUIBuildWall.h"
#include "ee/EEUIBuildGate.h"

class UIForm;

// The unit tab's C++ parts (the tab whole in Rust: ee_screens editor_unit.rs, its controls, the palettes, sorts and
// owners and the class list they fill, the cargo, the placement state, the mouse and keys; its group
// editor_groups_ffi.rs owns these, its host reads them as the tab's shell): the build tools by value (their Build and
// Cancel call the tab's PlaceWall, EndWall, PlaceGate, EndGate), the rhombus, the panel's rectangle (Enter: (0, 0)-(224,
// 605)) and the editor form (the panel's background is drawn on it).
struct EESEUnit {
    explicit EESEUnit(UIForm* inForm) : mForm(inForm) { this->mRhombus.SetColor(GEColor(0.0f, 0.0f, 0.0f)); }
    GERhombus mRhombus;                     // +0x04
    EEUIBuildWall mBuildWall = ee_screens_wall_new(true);   // +0x1c (the original's EESEUIBuildWall)
    EEUIBuildGate mBuildGate = ee_screens_gate_new(true);   // +0x3c (EESEUIBuildGate)
    GERectangle mPanelRectangle;            // +0x5c
    UIForm* mForm;
};


#endif
