// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <vector>
#include <map>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyControlConfig.h"
#include "ee/wonapi/EELobbySound.h"
#include "ee/wonapi/WONLobbyImageButton.h"
#include "ee/wonapi/WONLobbyEvents.h"
#include "ee/wonapi/WONComponentFlags.h"
#include "rust/won_gui/guiconf.h"
#include "ee/wonapi/WONLobbyGuard.h"
#include "rust/won_lobby/lobbycfgrd.h"
#include "ee/wonapi/WONGUIShell.h"
// The instruction keys and the flag and value names this unit compares are ee_won's guiconf tables
// (rust/crates/ee_won_gui/src/guiconf.rs); this unit keeps the reads, the objects and what each entry does.
using namespace WONAPI;
namespace WONAPI {
// Creates an image button with a text caption in the default font
ImageTextButton::ImageTextButton(const GUIString& text) {
    this->SetComponentFlags(ComponentFlag_GrabBG,true);
    this->mFont=this->GetDefaultFont();
    this->mText=text;
}
// Draws the button image, then its caption in black, shifted one pixel while the button is held down
void ImageTextButton::Paint(Graphics& g) {
    ImageButton::Paint(g);
    int down=this->IsPressed()?1:0;
    g.SetFont(this->mFont.get());
    g.SetColor(0);
    g.DrawString(this->mText,30+down,(this->Height()-this->mFont->GetHeight()-4)/2+down);
}
// Opens the button's web link in the browser when it has one; otherwise lets the click go on as usual
bool ImageTextButton::HandleActivate() {
    if(this->mBrowse.empty()) return true;
    Browse(this->mBrowse);
    return false;
}
// Shows the cross-promotion image and link from the event, or the default image and link when it has none
void CrossPromotionButton::HandleSetCrossPromotion(ComponentEvent* event) {
    if(Image* anImage=(Image*)LobbyEvtGet(event,LobbyEvtField::Image)) {
        this->mImages[ImageButtonType_Normal]=anImage;
        this->mBrowse=GUIString(LobbyEvtStr<std::string>(event,LobbyEvtField::Link));
    } else {
        this->mImages[ImageButtonType_Normal]=this->mDefaultImage;
        this->mBrowse=this->mDefaultBrowse;
    }
    this->Invalidate();
}
// Sets the default promotion image and shows it if the button has no image yet
void CrossPromotionButton::SetDefaultImage(Image* image) {
    this->mDefaultImage=image;
    if(!this->mImages[ImageButtonType_Normal].get())
        this->mImages[ImageButtonType_Normal]=this->mDefaultImage;
}
// Handles set-cross-promotion broadcasts; passes every other event to the base component
void CrossPromotionButton::HandleComponentEvent(ComponentEvent* event) {
    if(event->mType==LobbyEvent_SetCrossPromotion) this->HandleSetCrossPromotion(event);
    else Component::HandleComponentEvent(event);
}
// Reads the DEFAULTIMAGE and DEFAULTBROWSE layout keys; passes other keys to the image button config
// the readers' statements (lobbycfgrd.rs composes them, R6 WON round 33)
// the config readers' reads (LobbyNetCtrlHost.cpp)
intptr_t LobbyCfgReadHost(ComponentConfig* c, EeWonLobbyCfgRdOp inOp, intptr_t a, intptr_t b, intptr_t cc);
static intptr_t CfgRdHost(void* inCfg, EeWonLobbyCfgRdOp inOp, intptr_t a, intptr_t b, intptr_t cc)
{
    typedef EeWonLobbyCfgRdOp Op;
    ComponentConfig* c = static_cast<ComponentConfig*>(inCfg);
    CrossPromotionButtonConfig* cp = static_cast<CrossPromotionButtonConfig*>(c);
    ImageTextButtonConfig* it = static_cast<ImageTextButtonConfig*>(c);
    switch (inOp) {
    case Op::CrossBase: return WON_GUARDED(cp->ComponentConfig::Virtual24(*(const std::string*)a));
    case Op::CrossDefaultImageRead: return WON_GUARDED_V(LobbyCfgComp<CrossPromotionButton>(c)->SetDefaultImage(c->ReadImage(false)));
    case Op::CrossDefaultBrowseRead: return WON_GUARDED_V(LobbyCfgComp<CrossPromotionButton>(c)->SetDefaultBrowse(c->ReadString(false)));
    case Op::ImageTextBase: return WON_GUARDED(it->ComponentConfig::Virtual24(*(const std::string*)a));
    case Op::ImageTextFontRead: return WON_GUARDED_V(LobbyCfgComp<ImageTextButton>(c)->mFont = c->ReadFont());
    case Op::ImageTextTextRead: return WON_GUARDED_V(LobbyCfgComp<ImageTextButton>(c)->mText = c->ReadString(false));
    case Op::ImageTextBrowseRead: return WON_GUARDED_V(LobbyCfgComp<ImageTextButton>(c)->mBrowse = c->ReadString(false));
    default: return LobbyCfgReadHost(c, inOp, a, b, cc);
    }
}
static intptr_t CfgRd(ComponentConfig* inCfg, EeWonLobbyCfgRdCall inCall, const std::string& inKey, intptr_t d = 0) { return WonRethrow(ee_won_lobbycfgrd_call(inCfg, CfgRdHost, inCall, (intptr_t)inKey.data(), (intptr_t)inKey.size(), (intptr_t)&inKey, d)); }

bool CrossPromotionButtonConfig::Virtual24(const std::string& key) { return CfgRd(this, EeWonLobbyCfgRdCall::CrossPromotion, key) != 0; }

// Reads the FONT, TEXT and BROWSELOCATION layout keys; passes other keys to the image button config
bool ImageTextButtonConfig::Virtual24(const std::string& key) { return CfgRd(this, EeWonLobbyCfgRdCall::ImageText, key) != 0; }

// Creates the cross-promotion button with an empty caption, listening to lobby broadcasts
CrossPromotionButton::CrossPromotionButton()
    : ImageTextButton(GUIString(new GUIStringData("")))
{
    this->SubscribeToBroadcast(true);
}

}
