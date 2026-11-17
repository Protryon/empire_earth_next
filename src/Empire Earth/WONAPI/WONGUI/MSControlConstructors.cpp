// Constructors and destructors of the Windows-style controls, and helpers they inline (file name inferred; three of
// the original's units, their include lists joined) and the MS controls' paints (six more units, R6 WONGUI B round 1)
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
#include "ee/wonapi/WONLobbyMSExtraConfig.h"
#include "ee/wonapi/WONLobbyMSRemainingPainters.h"
#include "ee/wonapi/WONLobbyMSMessageBox.h"
#include "ee/wonapi/WONLobbyMSPainters.h"
#include "ee/wonapi/WONLobbyMSBorderPainters.h"
#include "ee/wonapi/WONGUIDraw.h"
#include "rust/won_gui/msbtnobj.h"
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

// Creates a dialog with a dialog background that fills it (emitted at 0x00700550)
MSDialog::MSDialog()
{
    SmartPtr<MSDlgBack> back=new MSDlgBack;
    this->Fn2F6140(back.get(),3,this,0,0,0,0,0);
    this->AddChild(back.get());
}

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
// ---- MS3DFrame (was MS3DFrame.cpp)
// Draws the frame raised or sunken in one of three styles (3D, flat lobby frame, three-line); other
// types draw nothing
void MS3DFrame::Paint(Graphics& g) {
    Component::Paint(g);
    WONMsPaint(g,WONMsPaintOf(EeWonMsPaintKind::Frame3DComponent,this->Width(),this->Height(),this->mIsUp?0:EE_WON_MSPAINT_PRESSED,this->mType));
}
// ---- MSArrowButton (was MSArrowButton.cpp)
// Draws the arrow button: face, frame and arrow, shifted one pixel while pressed and greyed when disabled
void MSArrowButton::Paint(Graphics& g) {
    Component::Paint(g);
    WONMsPaint(g,WONMsPaintOf(EeWonMsPaintKind::ArrowButton,this->Width(),this->Height(),
                              (this->IsPressed()?EE_WON_MSPAINT_PRESSED:0)|(this->IsDisabled()?EE_WON_MSPAINT_DISABLED:0),this->mDirection));
}
// ---- MSButton (was MSButton.cpp)
// Paints a Windows push button: face, a dark outline when it is the default button, a raised or pressed frame, and
// the text shifted one pixel while pressed (mspaint.rs Button)
void MSButton::Paint(Graphics& g) {
    Component::Paint(g);
    uint32_t f=(this->IsPressed()?EE_WON_MSPAINT_PRESSED:0)|(this->IsTransparent()?EE_WON_MSPAINT_TRANSPARENT:0)|
               (this->IsDefaultButton()?EE_WON_MSPAINT_DEFAULT:0)|(this->mUseFrame2?EE_WON_MSPAINT_FRAME2:0);
    WONMsPaint(g,WONMsPaintOf(EeWonMsPaintKind::Button,this->Width(),this->Height(),f),this);
}
// ---- MSTabButton (was MSTabButton.cpp)
// Returns the button's text size plus 16 pixels of width and 5 plus the lowered amount of height
void MSTabButton::GetDesiredSize(int& width,int& height) {
    MSButtonBase::GetDesiredSize(width,height);
    width+=16;
    height+=5+this->mLowerAmount;
}
// Paints a tab: the tab outline, then the button text (mspaint.rs TabButton)
void MSTabButton::Paint(Graphics& g) {
    Component::Paint(g);
    WONMsPaint(g,WONMsPaintOf(EeWonMsPaintKind::TabButton,this->Width(),this->Height()),this);
}
// ---- MSCheckbox (was MSCheckbox.cpp)
// Check box size and the replaceable function that paints the box
int MSCheckbox::mCheckboxWidth=13;
int MSCheckbox::mCheckboxHeight=13;
MSCheckbox::CheckboxPainterFunc MSCheckbox::CheckboxPainter=MSCheckbox::DefaultCheckboxPainterFunc;
// Paints the default check box square, pressed and checked as the button's flags say
void MSCheckbox::DefaultCheckboxPainterFunc(Graphics& g,MSCheckbox* button) {
    MSGraphicsUtil::DrawCheck(g,0,0,MSCheckbox::mCheckboxWidth,MSCheckbox::mCheckboxHeight,button->IsPressed(),button->IsChecked());
}
// Paints the check box: face, the box aligned vertically and drawn by the painter function, then the text (the Rust
// button's box paint, msbtnobj.rs)
void MSCheckbox::Paint(Graphics& g) {
    this->MCall(EeWonMsBtnCall::BoxPaint,(intptr_t)EeWonMsBox::Checkbox,(intptr_t)&g,MSCheckbox::mCheckboxHeight);
}
// ---- MSRadioButton (was MSRadioButton.cpp)
// Radio circle size (13 x 13) and the replaceable radio painter
int MSRadioButton::mRadioWidth=13;
int MSRadioButton::mRadioHeight=13;
MSRadioButton::RadioPainterFunc MSRadioButton::RadioPainter=MSRadioButton::DefaultRadioPainterFunc;
// Draws the standard radio circle, pressed (flag 0x20) and checked (flag 4) as the button's flags say
void MSRadioButton::DefaultRadioPainterFunc(Graphics& g,MSRadioButton* button) {
    MSGraphicsUtil::DrawRadio(g,0,0,MSRadioButton::mRadioWidth,MSRadioButton::mRadioHeight,button->IsPressed(),button->IsChecked());
}
// Paints the radio button: face background unless transparent, the radio circle aligned vertically between the
// text pads and drawn by the painter function, then the button text (the Rust button's box paint, msbtnobj.rs)
void MSRadioButton::Paint(Graphics& g) {
    this->MCall(EeWonMsBtnCall::BoxPaint,(intptr_t)EeWonMsBox::Radio,(intptr_t)&g,MSRadioButton::mRadioHeight);
}
}
