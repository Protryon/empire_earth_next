// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <vector>
#include <map>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyCrossPromotion.h"
#include "ee/wonapi/EELobbySound.h"
#include "ee/wonapi/WONLobbyImageButton.h"
#include "ee/wonapi/WONLobbyEvents.h"
#include "ee/wonapi/WONComponentFlags.h"
#include "rust/won_gui/guiconf.h"
#include "ee/wonapi/WONLobbyGuard.h"
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
    if(event->mEv.kind==LobbyEvent_SetCrossPromotion) this->HandleSetCrossPromotion(event);
    else Component::HandleComponentEvent(event);
}
// Creates the cross-promotion button with an empty caption, listening to lobby broadcasts
CrossPromotionButton::CrossPromotionButton()
    : ImageTextButton(GUIString(""))
{
    this->SubscribeToBroadcast(true);
}

}
