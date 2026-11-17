// Skinned button (R6 WON round 35): the Rust WONButton (rust/crates/ee_won_gui/src/wonbtnobj.rs) holds the states, the
// wrapped text per font, the pads, colours, flags and alignments and runs every method; this is its host (its calls
// out: the base Button, the wrapped texts, the images, the fonts, the button's state, the Graphics), the text getter
// and the destructor.
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
#include "ee/wonapi/WONLobbyWONButton.h"
#include "ee/wonapi/WONComponentFlags.h"
#include "rust/won_gui/guidraw.h"
#include "rust/won_gui/guihost.h"
#include "rust/won_gui/wonbtnobj.h"
#include "ee/wonapi/WONGUIDraw.h"
using namespace WONAPI;

typedef EeWonGuiOp Op;

static WrappedText* Txt(intptr_t a) { return (WrappedText*)a; }
static Image* Img(intptr_t a) { return (Image*)a; }

// a wrapped text's method (EeWonWbText; `b` the text)
static intptr_t TextOp(intptr_t which, intptr_t b, intptr_t c)
{
    switch ((EeWonWbText)which) {
    case EeWonWbText::New: { WrappedText* t = new WrappedText; t->AddRef(); return (intptr_t)t; }
    case EeWonWbText::Hold: Txt(b)->AddRef(); return 0;
    case EeWonWbText::Release: Txt(b)->Release(); return 0;
    case EeWonWbText::SetText: Txt(b)->SetText(*(const GUIString*)c); return 0;
    case EeWonWbText::SetMaxWidth: Txt(b)->SetMaxWidth((int)c); return 0;
    case EeWonWbText::SetWrap: Txt(b)->SetWrap(c != 0); return 0;
    case EeWonWbText::SetCentered: Txt(b)->SetCentered(c != 0); return 0;
    case EeWonWbText::SetFont: Txt(b)->SetFont((Font*)c); return 0;
    case EeWonWbText::Width: return Txt(b)->GetWidth();
    case EeWonWbText::Height: return Txt(b)->GetHeight();
    case EeWonWbText::Font: return (intptr_t)Txt(b)->mFont.get();
    case EeWonWbText::Text: return (intptr_t)&Txt(b)->GetText();
    default: return 0;
    }
}

// an image's method (EeWonWbImage; `b` the image)
static intptr_t ImageOp(intptr_t which, intptr_t b)
{
    switch ((EeWonWbImage)which) {
    case EeWonWbImage::Hold: Img(b)->AddRef(); return 0;
    case EeWonWbImage::Release: Img(b)->Release(); return 0;
    case EeWonWbImage::Width: return Img(b)->mWidth;
    case EeWonWbImage::Height: return Img(b)->mHeight;
    case EeWonWbImage::Duplicate: { // the SmartPtr temporary's reference goes to the Rust
        SmartPtr<Image> aCopy = Img(b)->Duplicate();
        Image* p = aCopy.get();
        if (p) p->AddRef();
        return (intptr_t)p;
    }
    default: return 0;
    }
}

extern "C" intptr_t ee_won_guicpp_wonbtn_call(void* inComp, EeWonGuiOp op, intptr_t a, intptr_t b, intptr_t c, intptr_t d)
{
    WONButton* t = (WONButton*)inComp;
    switch (op) {
    case Op::Get:
        if ((EeWonGuiField)a == EeWonGuiField::Width) return t->Width();
        if ((EeWonGuiField)a == EeWonGuiField::Height) return t->Height();
        return 0;
    case Op::SetSize: t->SetSize((int)a,(int)b); return 0;
    case Op::BaseSizeChanged: t->Button::Virtual08(); return 0;
    case Op::BasePaint: t->Button::Paint(*(Graphics*)a); return 0;
    case Op::WbBaseCopy: t->Button::CopyAttributes((Button*)a); return 0;
    case Op::SkinBtn:
        switch ((EeWonSkinBtn)a) {
        case EeWonSkinBtn::IsDisabled: return t->IsDisabled() ? 1 : 0;
        case EeWonSkinBtn::HasFocus: return t->HasFocus() ? 1 : 0;
        case EeWonSkinBtn::Parent: return (intptr_t)t->mParent;
        case EeWonSkinBtn::UsedDialogKeys: return ((Container*)b)->HasUsedDialogKeys() ? 1 : 0;
        case EeWonSkinBtn::IsPressed: return t->IsPressed() ? 1 : 0;
        case EeWonSkinBtn::ButtonFlags: return (intptr_t)(uint32_t)t->GetButtonFlags();
        case EeWonSkinBtn::IsDefaultButton: return t->IsDefaultButton() ? 1 : 0;
        case EeWonSkinBtn::FlagSet: return ((Component*)b)->ComponentFlagSet((int)c) ? 1 : 0;
        case EeWonSkinBtn::SetFlag: t->SetComponentFlags((int)b,true); return 0;
        case EeWonSkinBtn::SetGrabBG: t->SetComponentFlags(ComponentFlag_GrabBG,true); return 0;
        default: return 0;
        }
    case Op::WbText: return TextOp(a,b,c);
    case Op::WbImage: return ImageOp(a,b);
    case Op::WbFont:
        switch ((EeWonWbFont)a) {
        case EeWonWbFont::Default: return (intptr_t)WONAPI::WindowManager::GetDefaultWindowManager()->GetDefaultFont();
        case EeWonWbFont::Hold: ((Font*)b)->AddRef(); return 0;
        case EeWonWbFont::Release: ((Font*)b)->Release(); return 0;
        default: return 0;
        }
    case Op::Draw: {
        WONDrawSlots aSlots;
        aSlots.mImages[0] = Img(c);
        aSlots.mText = Txt(d);
        WONDrawNoCall aNoCall;
        WONDrawOne(*(Graphics*)a, *(const EeWonDrawOp*)b, aSlots, aNoCall);
        return 0;
    }
    default: return 0;
    }
}

namespace WONAPI {
// Returns the button text, or an empty string when the button has no font yet
const GUIString& WONButton::GetText()
{
    const GUIString* aText = (const GUIString*)this->WCall(EeWonWbCall::GetText);
    return aText ? *aText : gEmptyGUIString;
}
// Destroys the button: the Rust button gives its texts and images back
WONButton::~WONButton() { ee_won_wonbtn_free(this->mWb); }
}
