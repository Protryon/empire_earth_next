// C:\EEX\Low-Level Engine\User Interface\UIBitmap.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// 0x1005d3a1..0x1005d72a: UIBitmap's header-inline members (SetBitmap, the shared `return 0` overrides, the
// implicit copy members, ??_E), the U2D key init chains, the ctor and the out-of-line members.
// UIBitmapBackground.cpp's inline block starts at 0x1005d72a.
#include <windows.h>

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
#include "lle/UIControl.h"
#include "lle/UIForm.h"
#include "lle/UIBitmap.h"

// Creates a bitmap control showing the given bitmap (referenced)
UIBitmap::UIBitmap(unsigned long inId, GEBitmap* inBitmap, const GERectangle& inRectangle, UIForm* inForm,
                   unsigned long inIndex)
    : UIControl(kControlTypeBitmap, inId, inRectangle, inForm, inIndex, 0)
{
    this->mBitmap = inBitmap;
    if (this->mBitmap)
        this->mBitmap->Reference();
    this->mCopiedBitmap = NULL;
}

// Destroys the bitmap control, releasing its bitmap and the copy for drawing
UIBitmap::~UIBitmap()
{
    if (this->mBitmap)
        this->mBitmap->Dereference();
    if (this->mCopiedBitmap)
        this->mCopiedBitmap->Dereference();
}

// Copies the bitmap and its top-left location for the draw pass
long UIBitmap::CopyData()
{
    long theResult = UIControl::CopyData();
    if (this->mCopiedBitmap)
        this->mCopiedBitmap->Dereference();
    if (this->mBitmap) {
        this->mCopiedBitmap = this->mBitmap;
        this->mLocation = this->GetRectangle().GetTopLeft();
    } else
        this->mCopiedBitmap = NULL;
    if (this->mCopiedBitmap)
        this->mCopiedBitmap->Reference();
    return theResult;
}

// Draws the control, then the copied bitmap at its location
long UIBitmap::DrawPolygons()
{
    long theResult = UIControl::DrawPolygons();
    if (this->mCopiedBitmap) {
        this->mCopiedBitmap->SetLocation(this->mLocation);
        theResult = this->GetRasterizer()->Draw(this->mCopiedBitmap, this->GetScreenIndex());
    }
    return theResult;
}

// Draws the control's text (the base control's)
long UIBitmap::DrawTextA()
{
    return UIControl::DrawTextA();
}
