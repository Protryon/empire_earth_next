// C:\EEX\Low-Level Engine\User Interface\UISlider.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// The original unit: 0x1007d139..0x1007db5b (the U2D key init chains, the ctor, the out-of-line members); the
// header-inline members were emitted by UICheckBox.cpp (0x1005ef4b..0x1005f272).
//
// UISlider is whole in Rust (rust/crates/ee_ui/src/sliderobj.rs, R6 ui round 10): the members below forward to the
// Rust object, which calls out through ee_ui_slider_host (UIControl's members, the virtuals ChangeRange, ButtonDown,
// ButtonUp and SetDirty, the thumb bitmap, the manager's drag bar).
#include <windows.h>

#include "rust/ui/obj.h"
#include "rust/ui/sliderobj.h"
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
#include "lle/TSCriticalSection.h"
#include "lle/UIEntity.h"
#include "lle/UIEntityGroup.h"
#include "lle/UIControl.h"
#include "lle/UIFormManager.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"
#include "lle/UISlider.h"
#include "lle/UIObjHost.h"
#include "lle/UIShims.h"
#include "rust/ui/lle_hosts.h"

#define s static_cast<UISlider*>(inObj)
#define theManager (s->GetForm()->GetManager())
EE_UI_VOID(slider_change_range) { s->ChangeRange((unsigned long)a); }
EE_UI_VOID(slider_button_down) { s->ButtonDown(); }
EE_UI_VOID(slider_button_up) { s->ButtonUp(); }
EE_UI_CASE(slider_thumb_create) { return (intptr_t)theManager->CreateBitmapForScaledControl(theManager->GetDragBarBitmap(), UIObjHost::Rect(f)); }
EE_UI_VOID(slider_drag_bar_scale) { *reinterpret_cast<GE2DPoint*>(f) = GE2DPoint(theManager->GetDragBarBitmap()->GetXScale(), theManager->GetDragBarBitmap()->GetYScale()); }
#undef theManager
#undef s

