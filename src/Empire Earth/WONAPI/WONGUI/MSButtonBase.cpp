// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
// MSButtonBase (R6 WON round 35): the Rust MSButtonBase (rust/crates/ee_won/src/msbtnobj.rs) holds the colours,
// alignments, focus style, pads and flags and runs every method; this is its host (its calls out: the base
// Component, the wrapped text, the button's state, the Graphics) and the constructor and destructor.
#include <windows.h>
#include <string>
#include <vector>
#include <list>
#include <map>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONComponentFlags.h"
#include "rust/ee_won.h"
#include "ee/wonapi/WONGUIDraw.h"
using namespace WONAPI;

typedef EeWonGuiOp Op;

// the button's and component's methods (EeWonSkinBtn)
static intptr_t SkinBtnOp(MSButtonBase* t, intptr_t which, intptr_t b, intptr_t c)
{
    switch ((EeWonSkinBtn)which) {
    case EeWonSkinBtn::IsDisabled: return t->IsDisabled() ? 1 : 0;
    case EeWonSkinBtn::HasFocus: return t->HasFocus() ? 1 : 0;
    case EeWonSkinBtn::Parent: return (intptr_t)t->mParent;
    case EeWonSkinBtn::UsedDialogKeys: return ((Container*)b)->HasUsedDialogKeys() ? 1 : 0;
    case EeWonSkinBtn::IsPressed: return t->IsPressed() ? 1 : 0;
    case EeWonSkinBtn::IsChecked: return t->IsChecked() ? 1 : 0;
    case EeWonSkinBtn::SetGrabBG: t->SetComponentFlags(ComponentFlag_GrabBG,true); return 0;
    case EeWonSkinBtn::SetWantFocus: t->SetComponentFlags(ComponentFlag_WantFocus,b!=0); return 0;
    case EeWonSkinBtn::SetButtonFlags: t->SetButtonFlags((unsigned long)b,c!=0); return 0;
    default: return 0;
    }
}

// the Rust's check box colour is StandardColor_Text (msbtnobj.rs SYS_TEXT)
static_assert(StandardColor_Text==14,"msbtnobj.rs SYS_TEXT");

extern "C" intptr_t ee_won_guicpp_msbtn_call(void* inComp, EeWonGuiOp op, intptr_t a, intptr_t b, intptr_t c, intptr_t d)
{
    (void)d;
    MSButtonBase* t = (MSButtonBase*)inComp;
    switch (op) {
    case Op::Get:
        if ((EeWonGuiField)a == EeWonGuiField::Width) return t->Width();
        if ((EeWonGuiField)a == EeWonGuiField::Height) return t->Height();
        return 0;
    case Op::SetSize: t->SetSize((int)a,(int)b); return 0;
    case Op::BaseSizeChanged: t->Component::Virtual08(); return 0;
    case Op::BasePaint: t->Component::Paint(*(Graphics*)a); return 0;
    case Op::WrapSetMaxWidth: t->mText->SetMaxWidth((int)a); return 0;
    case Op::WrapSetCentered: t->mText->SetCentered(a != 0); return 0;
    case Op::WrapWidth: return t->mText->GetWidth();
    case Op::WrapHeight: return t->mText->GetHeight();
    case Op::SkinBtn: return SkinBtnOp(t,a,b,c);
    case Op::SkinBoxPainter: // the subclass's replaceable painter function
        if ((EeWonMsBox)b == EeWonMsBox::Checkbox) MSCheckbox::CheckboxPainter(*(Graphics*)a,(MSCheckbox*)t);
        else if ((EeWonMsBox)b == EeWonMsBox::Radio) MSRadioButton::RadioPainter(*(Graphics*)a,(MSRadioButton*)t);
        return 0;
    case Op::Draw: {
        WONDrawSlots aSlots;
        aSlots.mText = t->mText.get();
        WONDrawNoCall aNoCall;
        WONDrawOne(*(Graphics*)a, *(const EeWonDrawOp*)b, aSlots, aNoCall);
        return 0;
    }
    default: return 0;
    }
}

// Creates a button base with unwrapped centred text in the MSButton font and 2-pixel padding (the Rust button holds
// the colours, alignments, focus style and pads)
MSButtonBase::MSButtonBase() {
    this->mMsb=ee_won_msbtn_new(this,ee_won_guicpp_msbtn_call);
    this->mText=new WrappedText;
    this->mText->SetWrap(false);
    this->mText->SetFont(this->GetNamedFont("MSButton"));
}
// Destroys the button base: the Rust button goes
MSButtonBase::~MSButtonBase() { ee_won_msbtn_free(this->mMsb); }
