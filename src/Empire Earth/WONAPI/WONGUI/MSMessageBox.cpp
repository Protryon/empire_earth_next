// Windows-style message box: construction, layout and running it as a modal dialog
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
#include "ee/wonapi/WONLobbyMSMessageBox.h"
namespace WONAPI {
static RawBuffer gMessageBoxEmptyBuffer;
// FUNCTION: WON 0x007016f0
// Creates a message box: a centred wrapped label (the title when there is no message) and up to three
// same-size buttons ("Ok" by default, at least 80 wide) centred below it; Escape ends with the last button's id
MSMessageBox::MSMessageBox(const GUIString& title,const GUIString& message,
    const GUIString& button1,const GUIString& button2,const GUIString& button3)
{
    this->mTitle=title.mData.get();
    GUIString text=message;
    if(text.empty()) text=title;
    this->mLabel=new MSLabel(text);
    this->mLabel->SetHorzAlign(HorzAlign_Center);
    this->mLabel->SetAdjustHeightToFitText(true);
    this->mLabel->SetWrap(true);

    text=button1;
    if(text.empty()) text=GUIString("Ok");
    this->mButton1=new MSButton(text);
    this->mButton2=new MSButton(button2);
    this->mButton3=new MSButton(button3);
    bool doButton2=button2.length()!=0;
    bool doButton3=doButton2 && button3.length()!=0;
    SmartPtr<SameSizeLayout> sameSize=(new SameSizeLayout(this->mButton1.get(),true,true))->Add(this->mButton2.get())->Add(this->mButton3.get());
    sameSize->Execute(this);

    int width1=this->mLabel->mText->GetWidth();
    if(width1<140) width1=140;
    if(width1>400) width1=400;
    int buttonWidth=this->mButton1->Width();
    if(buttonWidth<80) {
        buttonWidth=80;
        this->mButton1->SetSize(buttonWidth,this->mButton1->Height());
    }
    int width2=buttonWidth;
    if(doButton2) width2+=20+buttonWidth;
    if(doButton3) width2+=20+buttonWidth;
    if(width2>width1) width1=width2;

    this->SetDefaultButton(this->mButton1.get());
    this->mButton1->SetButtonFlags(0x100,true);
    this->mButton2->SetButtonFlags(0x100,true);
    this->mButton3->SetButtonFlags(0x100,true);
    this->mButton1->mControlId=1;
    this->mButton2->mControlId=2;
    this->mButton3->mControlId=3;
    if(doButton3) this->SetEndOnEscape(3);
    else if(doButton2) this->SetEndOnEscape(2);
    else this->SetEndOnEscape(1);

    this->SetSize(width1+20,this->Height());
    this->mLabel->SetPos(10,10);
    this->Fn2F6140(this->mLabel.get(),1,this,0,0,-20,0,0);
    SmartPtr<CenterLayout> center=new CenterLayout(this,this->mButton1.get(),true,false);
    this->Fn2F6140(this->mButton1.get(),0x200,this->mLabel.get(),0,20,0,0,0);
    if(doButton2) {
        this->Fn2F6140(this->mButton2.get(),0x4403,this->mButton1.get(),10,0,0,0,0);
        center->Add(this->mButton2.get());
    }
    if(doButton3) {
        this->Fn2F6140(this->mButton3.get(),0x4403,this->mButton2.get(),10,0,0,0,0);
        center->Add(this->mButton3.get());
    }
    this->AddChildLayout(center.get());
    int aHeight=this->mButton1->Height()+this->mButton1->Top()+10;
    this->SetSize(this->Width(),aHeight);
    this->AddChild(this->mLabel.get());
    this->AddChild(this->mButton1.get());
    if(doButton2) this->AddChild(this->mButton2.get());
    if(doButton3) this->AddChild(this->mButton3.get());
}
}

// Showing the message box in its own window
namespace WONAPI {
// FUNCTION: WON 0x00701fd0
// Shows a modal message box over the parent window and returns the id (1-3) of the button pressed
int MSMessageBox::Msg(Window* parent,const GUIString& title,const GUIString& message,
    const GUIString& button1,const GUIString& button2,const GUIString& button3)
{
    SmartPtr<MSMessageBox> box=new MSMessageBox(title,message,button1,button2,button3);
    return box->DoDialog(parent);
}
// FUNCTION: WON 0x00702090
// Creates the dialog window (style 0x59) titled and sized for the box, puts the box in it and focuses
// the first button
SmartPtr<Window> MSMessageBox::PrepareWindow(Window* parent)
{
    SmartPtr<MSWindow> window=new MSWindow;
    window->SetParent(parent);
    window->SetTitle(this->mTitle);
    window->mStyle=0x59;
    window->Create(WONRectangle(0,0,this->Width(),this->Height()));
    window->AddComponent(this);
    this->mButton1->RequestFocus();
    return window.get();
}
// FUNCTION: WON 0x007021b0
// Runs the box as a modal dialog in its own window, closes the window and returns the result
int MSMessageBox::DoDialog(Window* parent)
{
    SmartPtr<Window> window=this->PrepareWindow(parent);
    int result=window->RunDialog(this);
    window->Close();
    return result;
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x00700630
// (public: virtual __thiscall WONAPI::MSDialog::~MSDialog(void)    [??1MSDialog@WONAPI@@UAE@XZ])
// TEMPLATE: WON 0x00701d20
// (public: __thiscall WONAPI::ChildLayoutInstructionBase::ChildLayoutInstructionBase(void)    [??0ChildLayoutInstructionBase@WONAPI@@QAE@XZ])
// TEMPLATE: WON 0x00701d60
// (public: virtual __thiscall WONAPI::CenterLayout::~CenterLayout(void)    [??1CenterLayout@WONAPI@@UAE@XZ])
// TEMPLATE: WON 0x00701ea0
// (public: virtual __thiscall WONAPI::MSMessageBox::~MSMessageBox(void)    [??1MSMessageBox@WONAPI@@UAE@XZ])
// TEMPLATE: WON 0x0071b310
// (public: void __thiscall std::list<class WONAPI::Component *,class std::allocator<class WONAPI::Component *> >::push_back(class WONAPI::Component * con)

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x00701d40
// (public: virtual void * __thiscall WONAPI::CenterLayout::`scalar deleting destructor'(unsigned int)    [??_GCenterLayout@WONAPI@@UAEPAXI@Z])
// TEMPLATE: WON 0x00701de0
// (public: virtual void * __thiscall WONAPI::SameSizeLayout::`scalar deleting destructor'(unsigned int)    [??_GSameSizeLayout@WONAPI@@UAEPAXI@Z])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x00701e80
// (public: virtual void * __thiscall WONAPI::MSMessageBox::`scalar deleting destructor'(unsigned int)    [??_GMSMessageBox@WONAPI@@UAEPAXI@Z])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x00701e00
// (public: virtual __thiscall WONAPI::SameSizeLayout::~SameSizeLayout(void))
// SYNTHETIC: WON 0x702240
// SYNTHETIC: WON 0x702250
// SYNTHETIC: WON 0x702270
// SYNTHETIC: WON 0x702280
