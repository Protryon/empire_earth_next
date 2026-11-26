// The buttons' hosts (R6 WON GUI round 37: Button.cpp with ImageButton.cpp, MSButtonBase.cpp and WONButton.cpp).
// Button (R6 WON round 30): the Rust Button's calls out (rust/crates/ee_won_gui/src/btnobj.rs, EeWonGuiOp's button ops and
// the component ops it shares with the container), its constructor and destructor, the static functions' forwarders,
// the two virtuals subclasses override (HandleActivate, SetTextHook) and the click sound. The original's Button.cpp
// (the flags, the press and release, the keys, the repeat, the radio group, the default buttons) is the Rust object;
// each op here is a run of its C++ statements with no decision between them.
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <vector>
#include <list>
#include <map>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/EELobbySound.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONComponentFlags.h"
#include "rust/won_gui/btnobj.h"
#include "rust/won_gui/guihost.h"
#include "ee/wonapi/WONLobbyImageButton.h"
#include "rust/won_gui/leafpaint.h"
#include "rust/won_gui/guidraw.h"
#include "rust/won_gui/msbtnobj.h"
#include "ee/wonapi/WONGUIDraw.h"
#include "ee/wonapi/WONGUIShell.h"
#include "rust/won_gui/wonbtnobj.h"
using namespace WONAPI;

typedef EeWonGuiOp Op;

// t is NULL for the static functions' ops (those on a)
extern "C" intptr_t ee_won_guicpp_btn_call(void* inComp, EeWonGuiOp op, intptr_t a, intptr_t b, intptr_t c, intptr_t d)
{
    (void)c; (void)d;
    Button* t = static_cast<Button*>((Component*)inComp);
    switch (op) {
    // (Component's own virtuals, the references, the siblings, the flags, the repaints, the focus, the timer and
    // BtnOf are gamewin.rs common_op's in the game: R6 WON GUI round 38)
    case Op::BtnHandleActivate: return t->HandleActivate() ? 1 : 0;
    case Op::BtnSetTextHook: t->SetTextHook(*(const GUIString*)a); return 0;
    case Op::BtnHasClickSound: return t->mClickSound.get() != NULL ? 1 : 0;
    case Op::BtnPlayClick: t->mClickSound->Play(); return 0;
    default: return 0;
    }
}

namespace WONAPI {
// Creates a button that takes focus (the Rust button: no flags, not pressed)
Button::Button() : mBtn(ee_won_btn_make(static_cast<Component*>(this),ee_won_guicpp_btn_call)) {}
// Destroys the button (the Rust button; the click sound is a member)
Button::~Button() { ee_won_btn_free(this->mBtn); }

// Lets an activation fire its event (always true here)
bool Button::HandleActivate() { return true; }

void Button::SetTextHook(const GUIString&) {}
// Copies another button's flags and click sound
void Button::CopyAttributes(Button* from)
{
    this->BCall(EeWonBtnCall::CopyAttributes,(intptr_t)from->mBtn);
    this->mClickSound=from->mClickSound;
}
// Sets the sound played when the button is pressed
void Button::SetClickSound(Sound* sound) { this->mClickSound=sound; }
}

// ---- ImageButton.cpp until R6 WON GUI round 37
using namespace WONAPI;
// Creates an image button that takes focus but not tab focus, with stretching off
ImageButton::ImageButton() : mStretch(false) { ee_won_btn_image_init(this->mBtn,static_cast<Component*>(this)); }
// Destroys the image button (members only)
ImageButton::~ImageButton() {}
typedef EeWonLeafOp Lo;
// the button's calls for leafpaint.rs (R6 WON round 34: GetDesiredSize and Paint are Rust's), one statement each
extern "C" intptr_t ee_won_guicpp_imgbtn_call(void* inMe, EeWonLeafOp op, intptr_t a, intptr_t b)
{
    ImageButton* t = (ImageButton*)inMe;
    switch (op) {
    // (the images, their sizes and draw and the stretch flag are leafpaint.rs's, read in place: R6 WON GUI round 39)
    case Lo::BtnBasePaint: t->Component::Paint(*(Graphics*)a); return 0;
    default: return 0;
    }
}
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
static_assert(sizeof(SmartPtr<Image>) == sizeof(void*) && offsetof(ImageButton, mStretch) == offsetof(ImageButton, mImages) + 5 * sizeof(void*),
              "leafpaint.rs reads ImageButton's images and stretch flag");
[[maybe_unused]] static const bool sImgBtnAt = (ee_won_leaf_imgbtn_register(offsetof(ImageButton, mImages)), true);
#pragma clang diagnostic pop
// Returns the largest width and height among the button's five state images
void ImageButton::GetDesiredSize(int& outWidth,int& outHeight)
{
    ee_won_leaf_call(this, ee_won_guicpp_imgbtn_call, EeWonLeafCall::ImageButtonSize, (intptr_t)&outWidth, (intptr_t)&outHeight);
}
// Draws the image for the current state (disabled, checked, down, over), falling back to the normal image,
// resizing it to the button first when stretching
void ImageButton::Paint(Graphics& g) { ee_won_leaf_call(this, ee_won_guicpp_imgbtn_call, EeWonLeafCall::ImageButtonPaint, (intptr_t)&g, 0); }

// ---- MSButtonBase.cpp until R6 WON GUI round 37
// MSButtonBase (R6 WON round 35): the Rust MSButtonBase (rust/crates/ee_won_gui/src/msbtnobj.rs) holds the colours,
// alignments, focus style, pads and flags and runs every method; this is its host (its calls out: the base
// Component, the wrapped text, the button's state, the Graphics) and the constructor and destructor.

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
    default: return 0; // (the size and the button's state: gamewin.rs common_op's)
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

// ---- WONButton.cpp until R6 WON GUI round 37
// Skinned button (R6 WON round 35): the Rust WONButton (rust/crates/ee_won_gui/src/wonbtnobj.rs) holds the states, the
// wrapped text per font, the pads, colours, flags and alignments and runs every method; this is its host (its calls
// out: the base Button, the wrapped texts, the images, the fonts, the button's state, the Graphics). Since R6
// WONLobby round 24 the button is the shell kind WonButton (WONGUIShell.h, shell_won.rs).

static WrappedText* Txt(intptr_t a) { return (WrappedText*)a; }
static Image* Img(intptr_t a) { return (Image*)a; }

// a wrapped text's method (EeWonWbText; `b` the text)
static intptr_t TextOp(intptr_t which, intptr_t b, intptr_t c)
{
    switch ((EeWonWbText)which) {
    case EeWonWbText::New: { WrappedText* t = new WrappedText; t->AddRef(); return (intptr_t)t; }
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

extern "C" intptr_t ee_won_guicpp_wonbtn_call(void* inComp, EeWonGuiOp op, intptr_t a, intptr_t b, intptr_t c, intptr_t d)
{
    WONButton* t = (WONButton*)inComp;
    switch (op) {
    case Op::WbBaseCopy: t->Button::CopyAttributes((Button*)a); return 0;
    case Op::WbText: return TextOp(a,b,c);
    // (WbImage, WbFont and the texts' references are gamewin.rs wonbtn_op's in the game: R6 WON GUI round 38)
    case Op::Draw: WONDrawOne(*(Graphics*)a, *(const EeWonDrawOp*)b, WONDrawSlots(NULL, NULL, Img(c), Txt(d)), WONDrawNoCall()); return 0;
    default: return 0; // (the size and the button's state: gamewin.rs common_op's)
    }
}
