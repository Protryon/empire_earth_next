// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
// MSButtonBase (R6 WON round 35): the Rust MSButtonBase (rust/crates/ee_won_gui/src/msbtnobj.rs) holds the colours,
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
#include "rust/won_gui/guidraw.h"
#include "rust/won_gui/guihost.h"
#include "rust/won_gui/msbtnobj.h"
#include "ee/wonapi/WONGUIDraw.h"
using namespace WONAPI;

typedef EeWonGuiOp Op;

// the Rust's check box colour is StandardColor_Text (msbtnobj.rs SYS_TEXT)
static_assert(StandardColor_Text==14,"msbtnobj.rs SYS_TEXT");

extern "C" intptr_t ee_won_guicpp_msbtn_call(void* inComp, EeWonGuiOp op, intptr_t a, intptr_t b, intptr_t c, intptr_t d)
{
    (void)d;
    MSButtonBase* t = (MSButtonBase*)inComp;
    switch (op) {
    case Op::WrapSetMaxWidth: t->mText->SetMaxWidth((int)a); return 0;
    case Op::WrapSetCentered: t->mText->SetCentered(a != 0); return 0;
    case Op::WrapWidth: return t->mText->GetWidth();
    case Op::WrapHeight: return t->mText->GetHeight();
    case Op::SkinBoxPainter: // the check box's or radio button's default painter function (never replaced), 13 x 13
        WONMsPaint(*(Graphics*)a,WONMsPaintOf((EeWonMsBox)b == EeWonMsBox::Checkbox ? EeWonMsPaintKind::Check : EeWonMsPaintKind::Radio,13,13,(t->IsPressed()?EE_WON_MSPAINT_PRESSED:0)|(t->IsChecked()?EE_WON_MSPAINT_CHECKED:0)));
        return 0;
    case Op::Draw: WONDrawOne(*(Graphics*)a, *(const EeWonDrawOp*)b, WONDrawSlots(NULL, NULL, NULL, t->mText.get()), WONDrawNoCall()); return 0;
    default: return ButtonHostCall(t, op, a, b, c); // the size, the button's state
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
