// C:\EEX\Empire Earth\User Interface Thread\EEUIBuildGate.h (RECONSTRUCTION; file name inferred). The gate
// placement mode: follows the mouse with a gate ghost on the wall under it, builds on a left
// click, cancels on a right click (RTTI EEUIBuildGate, no base; vtable 0x444f5c; sizeof 0x20: by-value members of
// EEUIFMainGame +0x4b4 and of the scenario editor form +0x3c). EEUIBuildGate.cpp is 0x25619b..0x25643d (the members in
// alphabetical order). Slot and member names are provisional. Requires lle/GE2DPoint.h.
#ifndef EE_EEUIBUILDGATE_H
#define EE_EEUIBUILDGATE_H
#include "rust/game/screens.h"   // ee_screens_gate_* (placement_ffi.rs)

// The class is the Rust record (UI lane round 16; ee_screens mouse_modes.rs EeScreensBuildGateFields under the
// members' names: mUnitClass the gate's class (an EEUnitClass*), mTileX, mTileY the tile of the last build, mTechnology,
// mWallType the highlighted walls' unit type, mDirection, mPlayer; mEditor the scenario editor's, the original's
// EESEUIBuildGate). Its members are Rust's (placement_ffi.rs): ee_screens_gate_new (the ctor), _initialize (+0x08
// 0x2561ff), _terminate (+0x0c 0x256245), _cancel (+0x04 0x2561b6), _build (+0x14 0x25619b), _clear_ghosts
// (0x2561c9); Update (+0x10 0x25627d) runs over the main game form's call record (EEUIFMainGame.cpp)
typedef EeScreensBuildGateFields EEUIBuildGate;
void EEUIBuildGateUpdate(EEUIBuildGate* inGate, const GE2DPoint& inLocation);

#endif
