// C:\EEX\Low-Level Engine\User Interface\UIGraph.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// UIGraph (with UILineGraph and the post-game summary's EEUIPGSLineGraph, R6 game screens B round 3) is whole in
// Rust (rust/crates/ee_ui/src/graphobj.rs): the members below forward to the Rust object, which calls out through
// ee_ui_graph_host (UIControl's members, the texts, a line).
#include <windows.h>

#include "rust/ui/graphobj.h"
#include "rust/ui/obj.h"
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
#include "lle/GELine.h"
#include "lle/GEFont.h"
#include "lle/GEText.h"
#include "lle/TSCriticalSection.h"
#include "lle/UIEntity.h"
#include "lle/UIEntityGroup.h"
#include "lle/UIControl.h"
#include "lle/UIFormManager.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"
#include "lle/UIGraph.h"
#include "lle/UIObjHost.h"
#include "lle/UIShims.h"
#include "rust/ui/lle_hosts.h"

#define g static_cast<UIGraph*>(inObj)
#define theText reinterpret_cast<GEText*>(a)
#define theLine reinterpret_cast<GELine*>(a)
EE_UI_VOID(graph_text_set_bytes) { theText->SetText(UWideString(reinterpret_cast<const char*>(b))); }
EE_UI_VOID(graph_text_fg_color) { *reinterpret_cast<GEColor*>(f) = theText->GetFgColor(); }
EE_UI_VOID(graph_text_set_transparent) { theText->SetIsTransparent(b != 0); }
EE_UI_VOID(graph_set_bg_rectangle) { g->SetBgRectangle(UIObjHost::Rect(f)); }
EE_UI_CASE(graph_draw_border) { return g->DrawBorder(UIObjHost::Rect(f), (unsigned long)a); }
EE_UI_CASE(graph_line_create) { return (intptr_t) new GELine; }
EE_UI_VOID(graph_line_delete) { delete theLine; }
#undef theLine
#undef theText
#undef g

