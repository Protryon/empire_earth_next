// C:\EEX\Low-Level Engine\User Interface\UIMovie.h (RECONSTRUCTION). A control that shows a GEMovie. Vtable
// 0x100997b0. The original's sizeof was 0xa4.
//
// Whole in Rust (rust/crates/ee_ui/src/mediaobj.rs, R6 ui round 21): the member is EeUiMovieFields, the class's
// private base; every method forwards there (UIBitmap.cpp holds the forwarders and the host). Requires
// lle/UIControl.h.
#ifndef LLE_UIMOVIE_H
#define LLE_UIMOVIE_H

#include "lle/UIControl.h"

class GEMovie;

class LLE_API UIMovie : public UIControl, private EeUiMovieFields {
    friend struct UIMediaHost;              // UIBitmap.cpp: the Rust record's calls out
public:
    UIMovie(unsigned long inId, GEMovie* inMovie, const GERectangle& inRectangle, UIForm* inForm,
            unsigned long inIndex);
    UIMovie(const UIMovie&) = delete;
    UIMovie& operator=(const UIMovie&) = delete;
    virtual ~UIMovie();
    virtual long CopyData();
    virtual long DrawPolygons();
    virtual long DrawTextA();
    virtual long UpdateKeyboard() { return kErrorOK; }
protected:
    // protected in the original (exported ?MouseLButtonDown@UIMovie@@MAEJXZ etc.); UpdateKeyboard is public
    virtual long MouseLButtonDown() { return kErrorOK; }
    virtual long MouseLButtonUp() { return kErrorOK; }
    virtual long MouseMove(const GE2DPoint& inLocation) { return kErrorOK; }
    virtual long MouseRButtonDown() { return kErrorOK; }
    virtual long MouseRButtonUp() { return kErrorOK; }
    virtual void TabbedTo() {}
    virtual void TabbedFrom() {}
};

#endif
