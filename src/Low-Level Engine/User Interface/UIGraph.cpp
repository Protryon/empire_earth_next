// C:\EEX\Low-Level Engine\User Interface\UIGraph.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// UIGraph (with UILineGraph and the post-game summary's EEUIPGSLineGraph, R6 game screens B round 3) is whole in
// Rust (rust/crates/ee_ui/src/graphobj.rs): the members below forward to the Rust object, which calls out through
// UIGraphHost::Op (UIControl's members, the texts, a line).
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
#include "UIObjHost.h"

struct UIGraphHost {
    static intptr_t Op(void* inGraph, EeUiOp inOp, intptr_t a, intptr_t b, intptr_t, intptr_t, float* f)
    {
        UIGraph* g = static_cast<UIGraph*>(inGraph);
        GEText* theText = reinterpret_cast<GEText*>(a);
        GELine* theLine = reinterpret_cast<GELine*>(a);
        intptr_t r = 0;
        if (UIObjHost::Base(g, inOp, a, f, &r) || UIObjHost::Media(g, inOp, a, b, f, &r))
            return r;
        switch (inOp) {
        case EeUiOp::TextSetBytes: theText->SetText(UWideString(reinterpret_cast<const char*>(b))); break;
        case EeUiOp::TextFgColor:
            f[0] = theText->GetFgColor().GetRed();
            f[1] = theText->GetFgColor().GetGreen();
            f[2] = theText->GetFgColor().GetBlue();
            break;
        case EeUiOp::TextSetTransparent: theText->SetIsTransparent(b != 0); break;
        case EeUiOp::SetBgRectangle: g->SetBgRectangle(UIObjHost::Rect(f)); break;
        case EeUiOp::DrawBorder: return g->DrawBorder(UIObjHost::Rect(f), (unsigned long)a);
        case EeUiOp::LineCreate: return (intptr_t) new GELine;
        case EeUiOp::LineDelete: delete theLine; break;
        default: break;
        }
        return 0;
    }
    static intptr_t Call(const UIGraph* g, EeUiGraphCall c, intptr_t a = 0, intptr_t b = 0, intptr_t cc = 0,
                         float* f = NULL)
    {
        return ee_ui_graph_call(g->mRust, c, a, b, cc, f);
    }
};
typedef EeUiGraphCall C;

// Creates a graph control with a 15 x 10 grid, every label on, zoom 1 and no fonts yet
UIGraph::UIGraph(unsigned long inId, const GERectangle& inRectangle, UIForm* inForm, unsigned long inIndex,
                 unsigned long inBorderThickness, EeUiGraphKind inKind, EeUiGraphFormatFn inFormat)
    : UIControl(kControlTypeGraph, inId, inRectangle, inForm, inIndex, inBorderThickness)
{
    this->mRust = ee_ui_graph_new(this, UIGraphHost::Op, inKind, inFormat);
}

UIGraph::~UIGraph() { ee_ui_graph_free(this->mRust); }
long UIGraph::CopyData() { return (long)UIGraphHost::Call(this, C::CopyData); }
long UIGraph::DrawPolygons() { return (long)UIGraphHost::Call(this, C::DrawPolygons); }
long UIGraph::DrawTextA() { return (long)UIGraphHost::Call(this, C::DrawText); }
void UIGraph::SetFont(GEFont* inFont) { UIGraphHost::Call(this, C::SetFont, (intptr_t)inFont); }
void UIGraph::SetFontColor(const GEColor& inColor)
{
    float f[4] = {inColor.GetRed(), inColor.GetGreen(), inColor.GetBlue(), 0.0f};
    UIGraphHost::Call(this, C::SetFontColor, 0, 0, 0, f);
}
void UIGraph::AddGraphSeries(const long* inPoints, unsigned long inCount, unsigned long inGraph, const GEColor& inColor)
{
    float f[4] = {inColor.GetRed(), inColor.GetGreen(), inColor.GetBlue(), 0.0f};
    UIGraphHost::Call(this, C::AddGraphSeries, inGraph, (intptr_t)inPoints, inCount, f);
}
void UIGraph::TabbedTo() { UIGraphHost::Call(this, C::TabbedTo); }
void UIGraph::TabbedFrom() { UIGraphHost::Call(this, C::TabbedFrom); }
