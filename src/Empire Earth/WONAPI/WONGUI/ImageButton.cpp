// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <vector>
#include <map>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/EELobbySound.h"
#include "ee/wonapi/WONLobbyImageButton.h"
#include "ee/wonapi/WONComponentFlags.h"
#include "rust/won_gui/leafpaint.h"
using namespace WONAPI;
// Creates an image button that takes focus but not tab focus, with stretching off
ImageButton::ImageButton() {
    this->SetButtonFlags(1,true);
    this->SetComponentFlags(ComponentFlag_WantFocus,true);
    this->SetComponentFlags(ComponentFlag_WantTabFocus,false);
    this->mStretch=false;
}
// Destroys the image button (members only)
ImageButton::~ImageButton() {}
typedef EeWonLeafOp Lo;
// the button's calls for leafpaint.rs (R6 WON round 34: GetDesiredSize and Paint are Rust's), one statement each
extern "C" intptr_t ee_won_guicpp_imgbtn_call(void* inMe, EeWonLeafOp op, intptr_t a, intptr_t b)
{
    ImageButton* t = (ImageButton*)inMe;
    Graphics* g = (Graphics*)a;
    switch (op) {
    case Lo::BtnHasImage: return t->mImages[a].get() != NULL ? 1 : 0;
    case Lo::BtnImageWidth: return t->mImages[a]->GetWidth();
    case Lo::BtnImageHeight: return t->mImages[a]->GetHeight();
    case Lo::BtnBasePaint: t->Component::Paint(*g); return 0;
    case Lo::BtnDisabled: return t->IsDisabled() ? 1 : 0;
    case Lo::BtnChecked: return t->IsChecked() ? 1 : 0;
    case Lo::BtnPressed: return t->IsPressed() ? 1 : 0;
    case Lo::BtnMouseOver: return t->IsMouseOver() ? 1 : 0;
    case Lo::BtnStretch: return t->mStretch ? 1 : 0;
    case Lo::BtnWidth: return t->Width();
    case Lo::BtnHeight: return t->Height();
    case Lo::BtnImageFit: t->mImages[a]->SetSize(t->Width(), t->Height()); return 0;
    case Lo::BtnDrawImage: g->DrawImage(t->mImages[b].get(), 0, 0); return 0;
    default: return 0;
    }
}
// Returns the largest width and height among the button's five state images
void ImageButton::GetDesiredSize(int& outWidth,int& outHeight)
{
    ee_won_leaf_call(this, ee_won_guicpp_imgbtn_call, EeWonLeafCall::ImageButtonSize, (intptr_t)&outWidth, (intptr_t)&outHeight);
}
// Draws the image for the current state (disabled, checked, down, over), falling back to the normal image,
// resizing it to the button first when stretching
void ImageButton::Paint(Graphics& g) { ee_won_leaf_call(this, ee_won_guicpp_imgbtn_call, EeWonLeafCall::ImageButtonPaint, (intptr_t)&g, 0); }
