// C:\EEX\Low-Level Engine\User Interface\UIBitmap.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// The original units: UIBitmap.cpp 0x1005d3a1..0x1005d72a and UIMovie.cpp 0x1007ba37..0x1007badf.
//
// UIBitmap and UIMovie are whole in Rust (rust/crates/ee_ui/src/mediaobj.rs, R6 ui round 21): the members below
// forward to the Rust records, which call out through UIMediaHost (UIControl's base members, the bitmaps, the
// movie). UIMovie.cpp's forwarders live here too.
#include <windows.h>

#include "rust/ee_ui.h"
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
#include "lle/GEMovie.h"
#include "lle/UIControl.h"
#include "lle/UIForm.h"
#include "lle/UIBitmap.h"
#include "lle/UIMovie.h"
#include "UIObjHost.h"

// The Rust records' calls out (EeUiOp): the object is the record, a bitmap control's or a movie control's base
struct UIMediaHost {
    static intptr_t BitmapOp(void* inObj, EeUiOp inOp, intptr_t a, intptr_t b, intptr_t, intptr_t, float* f)
    {
        UIBitmap* c = static_cast<UIBitmap*>(static_cast<EeUiBitmapFields*>(inObj));
        intptr_t r = 0;
        if (!UIObjHost::Base(c, inOp, a, f, &r))
            UIObjHost::Media(c, inOp, a, b, f, &r);
        return r;
    }
    static intptr_t MovieOp(void* inObj, EeUiOp inOp, intptr_t a, intptr_t, intptr_t, intptr_t, float* f)
    {
        UIMovie* c = static_cast<UIMovie*>(static_cast<EeUiMovieFields*>(inObj));
        GEMovie* m = reinterpret_cast<GEMovie*>(a);
        intptr_t r = 0;
        if (UIObjHost::Base(c, inOp, a, f, &r))
            return r;
        switch (inOp) {
        case EeUiOp::MovieReference: m->Reference(); break;
        case EeUiOp::MovieStop: m->Stop(); break;
        case EeUiOp::MovieRelease: m->Dereference(); break;
        case EeUiOp::MoviePlaying: return m->GetIsPlaying();
        case EeUiOp::MovieRender: return m->RenderFrame();
        default: break;
        }
        return 0;
    }
    static intptr_t Call(UIBitmap* c, EeUiBitmapCall inCall, intptr_t a = 0)
    {
        return ee_ui_bitmap_call(c, UIMediaHost::BitmapOp, inCall, a);
    }
    static intptr_t Call(UIMovie* c, EeUiMovieCall inCall, intptr_t a = 0)
    {
        return ee_ui_movie_call(c, UIMediaHost::MovieOp, inCall, a);
    }
};

// FUNCTION: LLE 0x1005d5ed
// Creates a bitmap control showing the given bitmap (referenced)
UIBitmap::UIBitmap(unsigned long inId, GEBitmap* inBitmap, const GERectangle& inRectangle, UIForm* inForm,
                   unsigned long inIndex)
    : UIControl(kControlTypeBitmap, inId, inRectangle, inForm, inIndex, 0)
{
    UIMediaHost::Call(this, EeUiBitmapCall::Init, (intptr_t)inBitmap);
}

UIBitmap::~UIBitmap() { UIMediaHost::Call(this, EeUiBitmapCall::Release); }
long UIBitmap::CopyData() { return (long)UIMediaHost::Call(this, EeUiBitmapCall::CopyData); }
long UIBitmap::DrawPolygons() { return (long)UIMediaHost::Call(this, EeUiBitmapCall::DrawPolygons); }
long UIBitmap::DrawTextA() { return (long)UIMediaHost::Call(this, EeUiBitmapCall::DrawText); }
void UIBitmap::SetBitmap(GEBitmap* inBitmap) { UIMediaHost::Call(this, EeUiBitmapCall::SetBitmap, (intptr_t)inBitmap); }

// FUNCTION: LLE 0x1007ba37
// Creates a movie control showing the given movie, keeping a reference to it
UIMovie::UIMovie(unsigned long inId, GEMovie* inMovie, const GERectangle& inRectangle, UIForm* inForm,
                 unsigned long inIndex)
    : UIControl(kControlTypeBitmap, inId, inRectangle, inForm, inIndex, 0)
{
    UIMediaHost::Call(this, EeUiMovieCall::Init, (intptr_t)inMovie);
}

UIMovie::~UIMovie() { UIMediaHost::Call(this, EeUiMovieCall::Release); }
long UIMovie::CopyData() { return (long)UIMediaHost::Call(this, EeUiMovieCall::CopyData); }
long UIMovie::DrawPolygons() { return (long)UIMediaHost::Call(this, EeUiMovieCall::DrawPolygons); }
long UIMovie::DrawTextA() { return (long)UIMediaHost::Call(this, EeUiMovieCall::DrawText); }
