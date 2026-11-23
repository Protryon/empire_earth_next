// C:\EEX\Low-Level Engine\User Interface\UIScrollBar.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// The original unit: 0x1007c901..0x1007d013 (the U2D key init chains, the ctor, the out-of-line members); the
// header-inline members were emitted by UICheckBox.cpp (0x1005f3b0..0x1005f538).
//
// UIScrollBar is whole in Rust (rust/crates/ee_ui/src/scrollbarobj.rs, R6 ui round 10): the members below forward to
// the Rust object, which calls out through UIScrollBarRust::Op (UIControl's members, the parts' virtuals, their new
// and delete, the manager's arrow bitmaps).
#include <windows.h>

#include "rust/ui/obj.h"
#include "rust/ui/rect.h"
#include "rust/ui/scrollbarobj.h"

#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GESurface.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GETextureManager.h"
#include "lle/GETexture.h"
#include "GEWindow.h"
#include "lle/GEScreen.h"
#include "lle/GEViewport.h"
#include "lle/GEVertexArray.h"
#include "lle/U2DRectangle.h"
#include "lle/GERasterizer.h"
#include "lle/GEBitmap.h"
#include "lle/GERhombus.h"
#include "lle/TMPoll.h"
#include "lle/TSCriticalSection.h"
#include "lle/UIEntity.h"
#include "lle/UIEntityGroup.h"
#include "lle/UIControl.h"
#include "lle/UIButton.h"
#include "lle/UIFormManager.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"
#include "lle/UISlider.h"
#include "lle/UIButton.h"
#include "lle/UIScrollBar.h"
#include "lle/UIObjHost.h"
#include "lle/UIShims.h"
#include "rust/ui/lle_hosts.h"

#define s static_cast<UIScrollBar*>(inObj)
#define theSlider reinterpret_cast<UISlider*>(a)
EE_UI_CASE(bar_create_slider) { return (intptr_t) new UISlider(s->GetId(), UIObjHost::Rect(f), (unsigned long)a, s->GetForm(), s->GetIndex(), (unsigned long)b); }
// a 19 x 19 arrow button at f (its slider and direction set by Rust: scrollbarobj.rs scroll_init)
EE_UI_CASE(bar_create_button) { return (intptr_t) new UIButton(s->GetId(), reinterpret_cast<GEBitmap*>(a), NULL, reinterpret_cast<GEBitmap*>(b), GERectangle(GE2DPoint(f[0], f[1]), GE2DPoint(f[0] + 19.0f, f[1] + 19.0f)), s->GetForm(), s->GetIndex(), 0); }
EE_UI_VOID(bar_part_button_down) { reinterpret_cast<UIButton*>(a)->ButtonDown(); }
EE_UI_CASE(bar_slider_style) { return theSlider->GetSliderStyle(); }
EE_UI_CASE(bar_slider_value) { return theSlider->GetCurrentRangeValue(); }
EE_UI_VOID(bar_slider_set_value) { theSlider->SetCurrentRangeValue((long)b); }
EE_UI_VOID(bar_slider_decrement_page) { theSlider->DecrementByPageSize(); }
EE_UI_VOID(bar_slider_increment_page) { theSlider->IncrementByPageSize(); }
EE_UI_VOID(bar_slider_set_page_size) { theSlider->SetPageSize((unsigned long)b); }
EE_UI_VOID(bar_slider_set_range) { theSlider->SetRange((unsigned long)b, (unsigned long)c); }
#undef theSlider
#undef s

