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
namespace WONAPI {
// FUNCTION: WON 0x006f5560
// Sets or clears the given component flags
void Component::SetComponentFlags(int flags,bool on)
{
    if(on) this->mComponentBits|=flags;
    else this->mComponentBits&=~flags;
}
// FUNCTION: WON 0x006f7910
// Creates a focusable, draggable dialog that grabs its background, stays inside the root window and ignores Escape
Dialog::Dialog()
{
    this->mDragX=this->mDragY=0;
    this->SetComponentFlags(0x118,true);
    this->mAllowDrag=true;
    this->mKeepInRoot=true;
    this->mEndOnEscape=false;
    this->mEscapeId=-1;
}
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
    this->mIsTransparent=!fillWhole;
    this->SetButtonFlags(0x48,true);
    this->SetVertAlign(VertAlign_Center);
}
// FUNCTION: WON 0x00700780
// Creates a 3D frame of the given type, raised or sunken, that ignores input
MS3DFrame::MS3DFrame(int type,bool up)
{
    this->mType=type;
    this->mIsUp=up;
    this->SetComponentFlags(ComponentFlag_NoInput,true);
}
// FUNCTION: WON 0x007006a0
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
// FUNCTION: WON 0x006febf0
// Sets a push button's padding (8 across, 4 down), makes it take focus and become the default button while focused
void MSButton::Init()
{
    this->mLeftTextPad=this->mRightTextPad=8;
    this->mTopTextPad=this->mBottomTextPad=4;
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
// FUNCTION: WON 0x00700dd0
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
// FUNCTION: WON 0x007010b0
// Relays out the list control, placing the header just above the scroll area
void MSListCtrl::Virtual08()
{
    MSScrollContainer::Virtual08();
    this->mHeader->SetPosSize(this->mScrollArea->Left(),this->mScrollArea->Top()-this->mHeaderHeight,
                     this->mScrollArea->Width(),this->mHeaderHeight);
}
}

#include "ee/wonapi/WONLobbyMSRemainingPainters.h"
#include "ee/wonapi/WONLobbyMSExtraConfig.h"
namespace WONAPI {

// FUNCTION: WON 0x006ff3b0
// Sets a check box's left-aligned text after the box, its focus rectangle around the text, and focus
void MSCheckbox::Init()
{
    this->mStandardTextColor=StandardColor_Text;
    this->mTextColor=-1;
    this->mHAlign=HorzAlign_Left;
    this->mFocusStyle=FocusStyle_AroundText;
    this->mLeftTextPad=MSCheckbox::mCheckboxWidth+4;
    this->SetComponentFlags(ComponentFlag_WantFocus,true);
}
// FUNCTION: WON 0x006ff3e0
// Creates a check box
MSCheckbox::MSCheckbox() { this->Init(); }
inline void MSRadioButton::Init()
{
    this->SetButtonFlags(8,true);
    this->mHAlign=HorzAlign_Left;
    this->mFocusStyle=FocusStyle_AroundText;
    this->mLeftTextPad=MSRadioButton::mRadioWidth+4;
}
// FUNCTION: WON 0x006ff530
// Creates a radio button with left-aligned text after the circle
MSRadioButton::MSRadioButton() { this->Init(); }
// Creates an unpadded left-aligned label that never activates (emitted at 0x006ff6c0)
MSLabel::MSLabel()
{
    this->mStandardTextColor=StandardColor_Text;
    this->mTextColor=-1;
    this->mTopTextPad=this->mRightTextPad=this->mLeftTextPad=this->mBottomTextPad=0;
    this->mHAlign=HorzAlign_Left;
    this->SetButtonFlags(0x200,true);
}
// Creates an unpadded, never-activating label sized to the given text; unlike the default constructor it keeps the
// centred alignment (emitted at 0x006ff740)
MSLabel::MSLabel(const GUIString& text)
{
    this->mStandardTextColor=StandardColor_Text;
    this->mTextColor=-1;
    this->mTopTextPad=this->mRightTextPad=this->mLeftTextPad=this->mBottomTextPad=0;
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
// FUNCTION: WON 0x00700840
// Creates an etched frame that ignores input
MSEtchedFrame::MSEtchedFrame() { this->SetComponentFlags(ComponentFlag_NoInput,true); }
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006fec30
// (public: __thiscall WONAPI::MSButton::MSButton(void)    [??0MSButton@WONAPI@@QAE@XZ])
// TEMPLATE: WON 0x006fec80
// (public: virtual void * __thiscall WONAPI::MSButton::`scalar deleting destructor'(unsigned int)    [??_GMSButton@WONAPI@@UAEPAXI@Z])
// TEMPLATE: WON 0x006feca0
// (public: __thiscall WONAPI::MSButton::MSButton(class WONAPI::GUIString const &)    [??0MSButton@WONAPI@@QAE@ABVGUIString@1@@Z])
// TEMPLATE: WON 0x006fed10
// (public: virtual __thiscall WONAPI::MSButton::~MSButton(void)    [??1MSButton@WONAPI@@UAE@XZ])
// TEMPLATE: WON 0x006fee20
// (public: __thiscall WONAPI::MSTabButton::MSTabButton(class WONAPI::GUIString const &,bool,int)    [??0MSTabButton@WONAPI@@QAE@ABVGUIString@1@_NH@Z])
// TEMPLATE: WON 0x006fef40
// (public: __thiscall WONAPI::MSHeaderButton::MSHeaderButton(class WONAPI::GUIString const &)    [??0MSHeaderButton@WONAPI@@QAE@ABVGUIString@1@@Z])
// TEMPLATE: WON 0x006feff0
// (public: virtual __thiscall WONAPI::MSHeaderButton::~MSHeaderButton(void)    [??1MSHeaderButton@WONAPI@@UAE@XZ])
// TEMPLATE: WON 0x006ff0c0
// (public: __thiscall WONAPI::MSArrowButton::MSArrowButton(enum WONAPI::Direction)    [??0MSArrowButton@WONAPI@@QAE@W4Direction@1@@Z])
// TEMPLATE: WON 0x006ff120
// (public: virtual void * __thiscall WONAPI::MSArrowButton::`scalar deleting destructor'(unsigned int)    [??_GMSArrowButton@WONAPI@@UAEPAXI@Z])
// TEMPLATE: WON 0x006ff140
// (public: virtual __thiscall WONAPI::MSArrowButton::~MSArrowButton(void)    [??1MSArrowButton@WONAPI@@UAE@XZ])
// TEMPLATE: WON 0x006ff200
// (public: __thiscall WONAPI::MSCloseButton::MSCloseButton(void)    [??0MSCloseButton@WONAPI@@QAE@XZ])
// TEMPLATE: WON 0x006ff290
// (public: virtual __thiscall WONAPI::MSCloseButton::~MSCloseButton(void)    [??1MSCloseButton@WONAPI@@UAE@XZ])
// TEMPLATE: WON 0x006ff310
// (public: __thiscall WONAPI::MSScrollThumb::MSScrollThumb(void)    [??0MSScrollThumb@WONAPI@@QAE@XZ])
// TEMPLATE: WON 0x006ff330
// (public: virtual void * __thiscall WONAPI::MSScrollThumb::`scalar deleting destructor'(unsigned int)    [??_GMSScrollThumb@WONAPI@@UAEPAXI@Z])
// TEMPLATE: WON 0x006ff350
// (public: virtual __thiscall WONAPI::MSScrollThumb::~MSScrollThumb(void)    [??1MSScrollThumb@WONAPI@@UAE@XZ])
// TEMPLATE: WON 0x006ff400
// (public: virtual void * __thiscall WONAPI::MSCheckbox::`scalar deleting destructor'(unsigned int)    [??_GMSCheckbox@WONAPI@@UAEPAXI@Z])
// TEMPLATE: WON 0x006ff420
// (public: virtual __thiscall WONAPI::MSCheckbox::~MSCheckbox(void)    [??1MSCheckbox@WONAPI@@UAE@XZ])
// TEMPLATE: WON 0x006ff590
// (public: virtual void * __thiscall WONAPI::MSRadioButton::`scalar deleting destructor'(unsigned int)    [??_GMSRadioButton@WONAPI@@UAEPAXI@Z])
// TEMPLATE: WON 0x006ff5b0
// (public: virtual __thiscall WONAPI::MSRadioButton::~MSRadioButton(void)    [??1MSRadioButton@WONAPI@@UAE@XZ])
// TEMPLATE: WON 0x006ff6c0
// (public: __thiscall WONAPI::MSLabel::MSLabel(void)    [??0MSLabel@WONAPI@@QAE@XZ])
// TEMPLATE: WON 0x006ff730
// (public: virtual __thiscall WONAPI::MSTabButton::~MSTabButton(void)    [??1MSTabButton@WONAPI@@UAE@XZ])
// TEMPLATE: WON 0x006ff740
// (public: __thiscall WONAPI::MSLabel::MSLabel(class WONAPI::GUIString const &)    [??0MSLabel@WONAPI@@QAE@ABVGUIString@1@@Z])
// TEMPLATE: WON 0x00700550
// (public: __thiscall WONAPI::MSDialog::MSDialog(void)    [??0MSDialog@WONAPI@@QAE@XZ])
// TEMPLATE: WON 0x00700610
// (public: virtual void * __thiscall WONAPI::MSDialog::`scalar deleting destructor'(unsigned int)    [??_GMSDialog@WONAPI@@UAEPAXI@Z])
// TEMPLATE: WON 0x007008d0
// (public: __thiscall WONAPI::MSEtchedFrameGroup::MSEtchedFrameGroup(void)    [??0MSEtchedFrameGroup@WONAPI@@QAE@XZ])
// TEMPLATE: WON 0x00700d10
// (public: __thiscall WONAPI::MSHeaderControl::MSHeaderControl(class WONAPI::MultiListArea *)    [??0MSHeaderControl@WONAPI@@QAE@PAVMultiListArea@1@@Z])
// TEMPLATE: WON 0x00700e80
// (public: __thiscall WONAPI::MSListCtrl::MSListCtrl(class WONAPI::MultiListArea *)    [??0MSListCtrl@WONAPI@@QAE@PAVMultiListArea@1@@Z])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006feeb0
// (public: virtual void * __thiscall WONAPI::MSTabButton::`scalar deleting destructor'(unsigned int)    [??_GMSTabButton@WONAPI@@UAEPAXI@Z])
// TEMPLATE: WON 0x006fefd0
// (public: virtual void * __thiscall WONAPI::MSHeaderButton::`scalar deleting destructor'(unsigned int)    [??_GMSHeaderButton@WONAPI@@UAEPAXI@Z])
// TEMPLATE: WON 0x006ff270
// (public: virtual void * __thiscall WONAPI::MSCloseButton::`scalar deleting destructor'(unsigned int)    [??_GMSCloseButton@WONAPI@@UAEPAXI@Z])
// TEMPLATE: WON 0x007009a0
// (public: virtual void * __thiscall WONAPI::MSEtchedFrameGroup::`scalar deleting destructor'(unsigned int)    [??_GMSEtchedFrameGroup@WONAPI@@UAEPAXI@Z])
// TEMPLATE: WON 0x00700d40
// (public: virtual void * __thiscall WONAPI::MSHeaderControl::`scalar deleting destructor'(unsigned int)    [??_GMSHeaderControl@WONAPI@@UAEPAXI@Z])
// TEMPLATE: WON 0x00700fa0
// (public: virtual void * __thiscall WONAPI::MSListCtrl::`scalar deleting destructor'(unsigned int)    [??_GMSListCtrl@WONAPI@@UAEPAXI@Z])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x006ff250
// (public: virtual void __thiscall WONAPI::MSCloseButton::Virtual0C(int &,int &))
// TEMPLATE: WON 0x00700700
// (public: virtual void __thiscall WONAPI::MSSeperator::Virtual0C(int &,int &))
// TEMPLATE: WON 0x007009c0
// (public: virtual __thiscall WONAPI::MSEtchedFrameGroup::~MSEtchedFrameGroup(void))
// TEMPLATE: WON 0x00700fc0
// (public: virtual __thiscall WONAPI::MSListCtrl::~MSListCtrl(void))
// TEMPLATE: WON 0x00700d60
// (public: virtual __thiscall WONAPI::MSHeaderControl::~MSHeaderControl(void))
// TEMPLATE: WON 0x006f7970
// (public: virtual void * __thiscall WONAPI::Dialog::`scalar deleting destructor'(unsigned int)    [??_GDialog@WONAPI@@UAEPAXI@Z])
