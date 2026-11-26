// C:\EEX\Low-Level Engine\User Interface\UIStatusBar.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// The original unit: 0x1007db5b..0x1007e312 (the U2D key init chains, the ctor, the dtor, the out-of-line members);
// the header-inline members were emitted by UIControlsTable.cpp.
//
// UIStatusBar is whole in Rust (rust/crates/ee_ui/src/statusobj.rs, R6 ui round 10): the members below forward to
// the Rust object, which calls out through ee_ui_status_bar_host (UIControl's members, the two rhombi, the texts).
#include <windows.h>

#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GEScreen.h"
#include "lle/GERasterizer.h"
#include "lle/GERhombus.h"
#include "lle/UIStatusBar.h"

#define s static_cast<UIStatusBar*>(inObj)
#define theRhombus (a ? s->mCopiedStatusRhombus : s->mStatusRhombus)
EE_UI_VOID(status_rhombus_rect) { UIObjHost::PutRect(f, theRhombus.GetRectangle()); }
EE_UI_VOID(status_rhombus_set_rect) { theRhombus.SetRectangle(UIObjHost::Rect(f)); }
EE_UI_VOID(status_rhombus_copy) { s->mCopiedStatusRhombus = s->mStatusRhombus; }
EE_UI_VOID(status_rhombus_set_opacity) { theRhombus.SetOpacity(f[0]); }
EE_UI_VOID(status_rhombus_set_color) { theRhombus.SetColor(GEColor(f[0], f[1], f[2])); }
EE_UI_CASE(status_rhombus_draw) { return s->GetRasterizer()->Draw(&theRhombus, s->GetScreenIndex()); }
#undef theRhombus
#undef s

