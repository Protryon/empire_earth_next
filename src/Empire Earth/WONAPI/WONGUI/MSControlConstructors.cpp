// Constructors and destructors of the Windows-style controls, and helpers they inline (file name inferred)
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include <set>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyRoomServer.h"
#include "ee/wonapi/WONLobbySkinnedContainers.h"
#include "ee/wonapi/WONComponentFlags.h"
#include "ee/wonapi/WONSkinLayout.h"
namespace WONAPI {
// Creates an arrow button pointing the given way that repeats while held (emitted at 0x006ff0c0)
MSArrowButton::MSArrowButton(Direction direction)
{
    this->mDirection=direction;
    this->SetButtonFlags(2,true);
}
// Creates a tab button (a radio button that activates on press) with the given text, lower amount and fill mode
// (emitted at 0x006fee20)
MSTabButton::MSTabButton(const GUIString& text,bool fillWhole,int lower)
{
    this->mLowerAmount=lower;
    if(text.length()!=0) {
        this->mText->SetText(text);
        this->SetDesiredSize(0,0);
    }
    this->SetTransparentField(!fillWhole);
    this->SetButtonFlags(0x48,true);
    this->SetVertAlign(VertAlign_Center);
}
// Creates a 3D frame of the given type, raised or sunken, that ignores input
MS3DFrame::MS3DFrame(int type,bool up)
{
    this->mType=type;
    this->mIsUp=up;
    this->SetComponentFlags(ComponentFlag_NoInput,true);
}
// Creates a separator of the given height
MSSeperator::MSSeperator(int height)
{
    this->SetSize(this->Width(),height);
}
}

// Push button, header and list control setup
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include <set>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyMSExtraConfig.h"
#include "ee/wonapi/WONLobbyMSRemainingPainters.h"
namespace WONAPI {
// Sets a push button's padding (8 across, 4 down), makes it take focus and become the default button while focused
void MSButton::Init()
{
    this->SetHorzTextPad(8,8);
    this->SetVertTextPad(4,4);
    this->SetComponentFlags(ComponentFlag_WantFocus,true);
    this->SetButtonFlags(0x400,true);
    this->mUseFrame2=false;
}
// Creates a push button with no text (emitted at 0x006fec30)
MSButton::MSButton() { this->Init(); }
// Creates a push button sized to the given text (emitted at 0x006feca0)
MSButton::MSButton(const GUIString& text)
{
    this->Init();
    this->mText->SetText(text);
    this->SetDesiredSize(0,0);
}
// Creates a left-aligned list header button that takes no focus, sized to its text if any (emitted at 0x006fef40)
MSHeaderButton::MSHeaderButton(const GUIString& text)
{
    this->SetComponentFlags(ComponentFlag_WantFocus,false);
    this->SetUseFrame2(true);
    this->IncrementHorzTextPad(2,0);
    this->SetHorzAlign(HorzAlign_Left);
    if(text.length()!=0) {
        this->mText->SetText(text);
        this->SetDesiredSize(0,0);
    }
}
// Creates a list header for the given list area with no left text padding (emitted at 0x00700d10)
MSHeaderControl::MSHeaderControl(MultiListArea* area) : MultiListHeader(area)
{
    this->mTextLeftPad=0;
}
// Puts a new header button with the given title on a column
void MSHeaderControl::SetColumn(int column,const GUIString& title)
{
    SmartPtr<MSButton> button=new MSHeaderButton(title);
    this->SetColumnItem(column,button.get());
}
// Creates a list control (a new list area when none is given) with a 15-pixel column header (emitted at 0x00700e80)
MSListCtrl::MSListCtrl(MultiListArea* area)
    : MSScrollContainer(area ? area : new MultiListArea)
{
    this->mHeader=new MSHeaderControl(this->GetListArea());
    this->AddChild(this->mHeader.get());
    this->mHeaderHeight=15;
}
// Relays out the list control (the scroll container's layout), placing the header just above the scroll area
// (skinlay.rs)
void MSListCtrl::Virtual08()
{
    Container::Virtual08();
    WONSkinLayout(this,EeWonSkinLayKind::MsList);
}
}

#include "ee/wonapi/WONLobbyMSRemainingPainters.h"
#include "ee/wonapi/WONLobbyMSExtraConfig.h"
namespace WONAPI {

// Sets a check box's left-aligned text after the box, its focus rectangle around the text, and focus
void MSCheckbox::Init()
{
    this->MCall(EeWonMsBtnCall::BoxInit,(intptr_t)EeWonMsBox::Checkbox,MSCheckbox::mCheckboxWidth);
}
// Creates a check box
MSCheckbox::MSCheckbox() { this->Init(); }
inline void MSRadioButton::Init()
{
    this->MCall(EeWonMsBtnCall::BoxInit,(intptr_t)EeWonMsBox::Radio,MSRadioButton::mRadioWidth);
}
// Creates a radio button with left-aligned text after the circle
MSRadioButton::MSRadioButton() { this->Init(); }
// Creates an unpadded left-aligned label that never activates (emitted at 0x006ff6c0)
MSLabel::MSLabel()
{
    this->SetStandardTextColor(StandardColor_Text);
    this->SetHorzTextPad(0,0);
    this->SetVertTextPad(0,0);
    this->SetHorzAlignField(HorzAlign_Left);
    this->SetButtonFlags(0x200,true);
}
// Creates an unpadded, never-activating label sized to the given text; unlike the default constructor it keeps the
// centred alignment (emitted at 0x006ff740)
MSLabel::MSLabel(const GUIString& text)
{
    this->SetStandardTextColor(StandardColor_Text);
    this->SetHorzTextPad(0,0);
    this->SetVertTextPad(0,0);
    this->SetButtonFlags(0x200,true);
    this->SetText(text,true);
    this->SetDesiredSize(0,0);
}
// Creates a 16 x 14 close button (emitted at 0x006ff200)
MSCloseButton::MSCloseButton() { this->SetSize(16,14); }

// Creates an etched frame group with a centred title label (emitted at 0x007008d0)
MSEtchedFrameGroup::MSEtchedFrameGroup()
{
    this->mLabel=new Label;
    this->mLabel->SetHorzAlign(HorzAlign_Center);
}
}

#include "ee/wonapi/WONLobbyMSMessageBox.h"
#include "ee/wonapi/WONLobbyMSPainters.h"
namespace WONAPI {
// Creates a dialog with a dialog background that fills it (emitted at 0x00700550)
MSDialog::MSDialog()
{
    SmartPtr<MSDlgBack> back=new MSDlgBack;
    this->Fn2F6140(back.get(),3,this,0,0,0,0,0);
    this->AddChild(back.get());
}
}

#include "ee/wonapi/WONLobbyMSBorderPainters.h"
#include "ee/wonapi/WONLobbyMSRemainingPainters.h"
namespace WONAPI {
// Destroys the push button (emitted at 0x006fed10)
MSButton::~MSButton() {}
// Destroys the arrow button (emitted at 0x006ff140)
MSArrowButton::~MSArrowButton() {}
// Destroys the check box (emitted at 0x006ff420)
MSCheckbox::~MSCheckbox() {}
// Destroys the radio button (emitted at 0x006ff5b0)
MSRadioButton::~MSRadioButton() {}
// Destroys the scrollbar thumb (emitted at 0x006ff350)
MSScrollThumb::~MSScrollThumb() {}
// Creates a scrollbar thumb (emitted at 0x006ff310)
MSScrollThumb::MSScrollThumb() {}
// Creates an etched frame that ignores input
MSEtchedFrame::MSEtchedFrame() { this->SetComponentFlags(ComponentFlag_NoInput,true); }
}
