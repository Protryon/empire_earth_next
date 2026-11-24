// C:\EEX\Empire Earth\User Interface Thread\EEUIBuildWall.h (RECONSTRUCTION; file name inferred). The wall placement
// mode: the first left click fixes the start, the mouse drags a line of wall ghosts,
// each further left click builds the line and starts the next one at its end; a right click cancels (RTTI
// EEUIBuildWall, no base; vtable 0x444f40; sizeof 0x20: by-value members of EEUIFMainGame +0x494 and of the scenario
// editor form +0x1c). EEUIBuildWall.cpp is 0x25643d..0x256845 (the members in alphabetical order; its Cancel is
// ICF-folded into EEUIBuildGate's 0x2561b6). Slot and member names are provisional. Requires lle/GE2DPoint.h.
#ifndef EE_EEUIBUILDWALL_H
#define EE_EEUIBUILDWALL_H
#include "ee/EETechTreeTechID.h"   // (kTechNone; files lean on it)
#include "rust/game/screens.h"   // ee_screens_wall_* (placement_ffi.rs)

// The class is the Rust record (UI lane round 16; ee_screens mouse_modes.rs EeScreensBuildWallFields under the
// members' names: mState 0 no start yet, 1 dragging; mStart, mEnd the tile centres; mTechnology, mPlayer; mEditor the
// scenario editor's, the original's EESEUIBuildWall). Its members are Rust's (placement_ffi.rs): ee_screens_wall_new
// (the ctor), _initialize (+0x08 0x256456), _terminate (+0x0c 0x2564a2), _cancel (+0x04), _build (+0x14 0x25643d);
// Update (+0x10 0x2564df) runs over the main game form's call record (EEUIFMainGame.cpp)
typedef EeScreensBuildWallFields EEUIBuildWall;
void EEUIBuildWallUpdate(EEUIBuildWall* inWall, const GE2DPoint& inLocation);

#endif
