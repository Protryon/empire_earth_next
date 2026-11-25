// Component backgrounds: fill colour, border colour, tiled or stretched image, or a painted component
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <vector>
#include <map>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyBackground.h"
#include "ee/wonapi/WONGUIDraw.h"
#include "rust/won_gui/leafpaint.h"



using namespace WONAPI;

// The constructors over WONLobbyBackground.h's defaults (Init, 0x335cb0, is its member initialisers since R6 WON GUI
// round 36): the standard colour 13, a background that tiles an image, a fill and border colour, one drawn by a
// component (the out-of-line bodies keep the SmartPtr members' types incomplete in the header)
Background::Background() : mColor((int)0x8000000d) {}
Background::Background(Image* theImage) : mImage(theImage) {}
Background::Background(int theColor, int theBorderColor) : mColor(theColor), mBorderColor(theBorderColor) {}
Background::Background(Component* theComponent) : mComponent(theComponent) {}
// Sets the fill colour (-1 for none)
void Background::SetColor(int theColor)
{
	this->mColor = theColor;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// the background's reads and calls for leafpaint.rs (R6 WON round 34: the paint is Rust's), one statement each
extern "C" intptr_t ee_won_guicpp_background_call(void* inMe, EeWonLeafOp op, intptr_t a, intptr_t b)
{
	Background* t = (Background*)inMe;
	Graphics* g = (Graphics*)a;
	switch (op) {
	case EeWonLeafOp::BgHasImage: return t->mImage.get() != NULL ? 1 : 0;
	case EeWonLeafOp::BgStretch: return t->mStretchImage ? 1 : 0;
	case EeWonLeafOp::BgUseOffsets: return t->mUseOffsets ? 1 : 0;
	case EeWonLeafOp::BgHasComponent: return t->mComponent.get() != NULL ? 1 : 0;
	case EeWonLeafOp::BgImageWidth: return t->mImage->GetWidth();
	case EeWonLeafOp::BgImageHeight: return t->mImage->GetHeight();
	case EeWonLeafOp::BgColor: return t->mColor;
	case EeWonLeafOp::BgBorderColor: return t->mBorderColor;
	case EeWonLeafOp::BgDraw: WONDrawOne(*g, *(const EeWonDrawOp*)b, WONDrawSlots(NULL, NULL, t->mImage.get()), WONDrawNoCall()); return 0;
	case EeWonLeafOp::BgCompSetSize: t->mComponent->SetSize((int)a, (int)b); return 0;
	case EeWonLeafOp::BgCompInvalidate: t->mComponent->Invalidate(); return 0;
	case EeWonLeafOp::BgCompPaint: t->mComponent->Paint(*g); return 0;
	default: return 0;
	}
}

// Paints a rectangle: stretched image alone, or fill, border, tiled image (scrolled by the offsets) and component
void Background::Paint(Graphics &g, int xpos, int ypos, int width, int height, int xoff, int yoff)
{
	ee_won_leaf_background(this, ee_won_guicpp_background_call, (intptr_t)&g, xpos, ypos, width, height, xoff, yoff);
}
