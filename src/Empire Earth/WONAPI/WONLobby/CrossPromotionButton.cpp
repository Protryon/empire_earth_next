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
bool CrossPromotionButtonConfig::Virtual24(const std::string& key) {
    const int aKeyIdx=ee_won_gui_cfg_key(EE_WON_CFG_KEYS_CROSSPROMOTIONBUTTONCONFIG_VIRTUAL24_KEY,key.data(),key.size());
    if(aKeyIdx==0)
        this->mCrossPromotionButton->SetDefaultImage(this->ReadImage(false));
    else if(aKeyIdx==1)
        this->mCrossPromotionButton->SetDefaultBrowse(this->ReadString(false));
    else
        return ImageButtonConfig::Virtual24(key);
    return true;
}
// Attaches the config to the cross-promotion button it configures
void CrossPromotionButtonConfig::SetComponent(Component* component) {
    ImageButtonConfig::SetComponent(component);
    this->mCrossPromotionButton=static_cast<CrossPromotionButton*>(component);
}

// Reads the FONT, TEXT and BROWSELOCATION layout keys; passes other keys to the image button config
bool ImageTextButtonConfig::Virtual24(const std::string& key) {
    const int aKeyIdx=ee_won_gui_cfg_key(EE_WON_CFG_KEYS_IMAGETEXTBUTTONCONFIG_VIRTUAL24_KEY,key.data(),key.size());
    if(aKeyIdx==0)
        this->mImageTextButton->mFont=this->ReadFont();
    else if(aKeyIdx==1)
        this->mImageTextButton->mText=this->ReadString(false);
    else if(aKeyIdx==2)
        this->mImageTextButton->mBrowse=this->ReadString(false);
    else
        return ImageButtonConfig::Virtual24(key);
    return true;
}

void ImageTextButtonConfig::SetComponent(Component* component) {
    ImageButtonConfig::SetComponent(component);
    this->mImageTextButton=static_cast<ImageTextButton*>(component);
}
// Creates the cross-promotion button with an empty caption, listening to lobby broadcasts
CrossPromotionButton::CrossPromotionButton()
    : ImageTextButton(GUIString(new GUIStringData("")))
{
    this->SubscribeToBroadcast(true);
}

}

namespace WONAPI {
// Creates a config for an image text button in the layout file
ComponentConfig* ImageTextButtonConfig::CfgFactory()
{
    return new ImageTextButtonConfig;
}
}

namespace WONAPI {
// Creates a config for a maximize-splitter button in the layout file
ComponentConfig* MaxSplitterButtonConfig::CfgFactory()
{
    return new MaxSplitterButtonConfig;
}
}

namespace WONAPI {
// Creates a config for a cross-promotion button in the layout file
ComponentConfig* CrossPromotionButtonConfig::CfgFactory()
{
    return new CrossPromotionButtonConfig;
}
}


namespace WONAPI {
// Creates a 15x15 arrow button that maximizes a splitter pane in the given direction
MaxSplitterButton::MaxSplitterButton(Direction direction) : MSArrowButton(direction)
{
    this->SetSize(15,15);
    this->SetButtonFlags(2,false);
}

// Draws the arrow button and, while it is checked, a small bar in the button text colour
void MaxSplitterButton::Paint(Graphics& g)
{
    MSArrowButton::Paint(g);
    if(!this->IsChecked())
        return;
    int x=this->Width()/2-3,y=this->Height()/2-2;
    if(this->IsPressed()) {
        ++x;
        ++y;
    }
    g.SetSysColor(StandardColor_ButtonText);
    g.FillRect(x,y,7,4);
}

// Passes every layout key to the arrow button config
bool MaxSplitterButtonConfig::Virtual24(const std::string& instruction)
{
    if(MSArrowButtonConfig::Virtual24(instruction))
        return true;
    return false;
}

// Attaches the config to the maximize-splitter button it configures
void MaxSplitterButtonConfig::SetComponent(Component* component)
{
    MSArrowButtonConfig::SetComponent(component);
    this->mButton=static_cast<MaxSplitterButton*>(component);
}

// Creates a maximize-splitter button for the layout file
Component* MaxSplitterButtonConfig::CompFactory()
{
    return new MaxSplitterButton;
}
}
