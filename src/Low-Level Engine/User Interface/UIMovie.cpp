// C:\EEX\Low-Level Engine\User Interface\UIMovie.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// 0x1007ba37..0x1007badf: ctor, dtor, CopyData, DrawPolygons, DrawTextA. No key chains (the unit does not include the
// geometry key headers); UIRadioButton.cpp's key chains start at 0x1007badf.
#include <windows.h>

#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GERhombus.h"
#include "lle/GEMovie.h"
#include "lle/UIControl.h"
#include "lle/UIMovie.h"

// Creates a movie control showing the given movie, keeping a reference to it
UIMovie::UIMovie(unsigned long inId, GEMovie* inMovie, const GERectangle& inRectangle, UIForm* inForm,
                 unsigned long inIndex)
    : UIControl(kControlTypeBitmap, inId, inRectangle, inForm, inIndex, 0)
{
    this->mMovie = inMovie;
    this->mMovie->Reference();
}

// Destroys the movie control, stopping the movie and releasing it
UIMovie::~UIMovie()
{
    this->mMovie->Stop();
    this->mMovie->Dereference();
    this->mMovie = NULL;
}

// Snapshots the control for drawing; keeps it dirty while the movie plays
long UIMovie::CopyData()
{
    long theResult = UIControl::CopyData();
    if (this->mMovie->GetIsPlaying())
        this->SetDirty(true);
    return theResult;
}

// Draws the control frame
long UIMovie::DrawPolygons()
{
    return UIControl::DrawPolygons();
}

// Renders the current movie frame in the text pass, lowering the pending draw count; returns the render result
long UIMovie::DrawTextA()
{
    UIControl::DrawTextA();
    long theResult = kErrorOK;
    if (this->mMovie)
        theResult = this->mMovie->RenderFrame();
    return theResult;
}
