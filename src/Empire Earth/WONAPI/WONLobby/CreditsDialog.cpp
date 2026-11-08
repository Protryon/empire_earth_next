// The lobby's credits dialog: credit lines from the layout file scroll up and fade in and out at the edges
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyDialogs.h"
#include "ee/wonapi/WONComponentFlags.h"
namespace WONAPI {
// FUNCTION: WON 0x7f7780
// Reads an ADDLINE layout key (text, optional colour) into the credits; passes other keys to the dialog config
bool CreditsDialogConfig::Virtual24(const std::string& inInstruction)
{
    if(inInstruction=="ADDLINE") {
    GUIString aLine=this->ReadString(false);
    DWORD aColor=-1;
    if(!this->EndOfString()) {
        this->EnsureComma();
        aColor=this->ReadColor();
    }
    this->mCredits->AddLine(aLine,this->Fn386860(std::string("DialogComponent")),aColor);
    return true;
    }
    return DialogConfig::Virtual24(inInstruction);
}

void CreditsDialogConfig::SetComponent(Component* inComponent)
{
    DialogConfig::SetComponent(inComponent);
    this->mCredits=static_cast<CreditsDialog*>(inComponent);
}
// FUNCTION: WON 0x7f7660
// Adds a credit line in the given colour, widening the given component to fit the text
void CreditsDialog::AddLine(const GUIString& inText,Component* inComponent,unsigned long inColor)
{
    Font* aFont=this->GetDefaultFont();
    int aWidth=aFont->GetStringWidth(inText,0,inText.length())+20;
    if(aWidth>inComponent->Width()) inComponent->SetSize(aWidth,inComponent->Height());
    this->mScrollingText->AddLine(inText,inColor);
}
// FUNCTION: WON 0x7f76b0
// Finds the credits and close buttons and puts the scrolling credits in place of the hidden credits button
void CreditsDialog::InitComponent(ComponentInit& inInit)
{
    if(inInit.mType!=2) return;
    ComponentConfig* aConfig=static_cast<ComponentConfigInit&>(inInit).mConfig;
    SafeGetComponent(aConfig,this->mCreditsButton,"CreditsButton",__FILE__,271);
    SafeGetComponent(aConfig,this->mCloseButton,"CancelButton",__FILE__,272);
    Component* aParent=this->mCreditsButton->mParent;
    this->Fn2F6140(this->mScrollingText.get(),0x5003,this->mCreditsButton.get(),aParent->Left(),aParent->Top(),0,0,0);
    this->AddChild(this->mScrollingText.get());
    this->mCreditsButton->SetVisible(false);
    this->UpdateCredits();
}
}

// FUNCTION: WON 0x007f7100
// Fades a colour towards white: 0 percent gives white, 100 percent the colour itself
DWORD DimColor(DWORD color,int percent)
{
    if(percent<=0) return 0xffffff;
    if(percent>=100) return color;
    int red=(color>>16)&255,green=(color>>8)&255,blue=color&255;
    red=255-((255-red)*percent/100);
    green=255-((255-green)*percent/100);
    blue=255-((255-blue)*percent/100);
    return (red<<16)|(green<<8)|blue;
}
namespace WONAPI {
// FUNCTION: WON 0x007f6f90
// Appends a line in the given colour, or in the previous line's colour when given -1
void ScrollingTextComponent::AddLine(const GUIString& text,unsigned long color)
{
    if(color!=static_cast<DWORD>(-1)) this->mLastColor=color;
    this->mLines.push_back(CreditLineInfo(text,this->mLastColor));
}
// FUNCTION: WON 0x007f70b0
// Scrolls the text up one pixel per scroll period, starting again from the bottom once it has scrolled out
bool ScrollingTextComponent::TimerEvent(int delta)
{
    this->mTimeSinceLastScroll+=delta;
    if(!(this->mTimeSinceLastScroll>=this->mScrollSpeed))
        return true;
    if(--this->mScrollTop < -this->mTotalTextHeight) this->mScrollTop=this->Height();
    this->Invalidate();
    this->mTimeSinceLastScroll=0;
    return true;
}
// FUNCTION: WON 0x007f71b0
// Draws the framed white box and the visible lines centred, fading lines in the top and bottom thirds
void ScrollingTextComponent::Paint(Graphics& g)
{
    Component::Paint(g);
    g.SetFont(this->GetDefaultFont());
    g.SetColor(0xffffff);
    g.FillRect(2,2,this->Width()-4,this->Height()-4);
    g.SetSysColor(StandardColor_3DHilight);
    g.DrawRect(0,0,this->Width(),this->Height());
    g.DrawRect(1,1,this->Width()-1,this->Height()-1);
    g.SetSysColor(StandardColor_3DShadow);
    g.DrawRect(0,0,this->Width()-1,this->Height()-1);
    int top=this->mScrollTop;
    for(CreditsList::iterator i=this->mLines.begin();i!=this->mLines.end();++i) {
        CreditLineInfo& line=*i;
        int height=g.mFont->GetHeight();
        if(top<this->Height()/3) g.SetColor(DimColor(line.GetColor(),top*100/(this->Height()/3)));
        else if(this->Height()-top<=height) g.SetColor(0xffffff);
        else if(top>(this->Height()*2/3)-height)
            g.SetColor(DimColor(line.GetColor(),(this->Height()-(top+height))*100/(this->Height()/3)));
        else g.SetColor(line.GetColor());
        if(g.mColor!=0xffffff && top>2 && top<this->Height()-height-2) {
            int width;
            width=g.mFont->GetStringWidth(line.GetLine());
            g.DrawString(line.GetLine(),(this->Width()-width)/2,top);
        }
        top+=height;
    }
    this->mTotalTextHeight=top-this->mScrollTop;
}
}

namespace WONAPI {
// FUNCTION: WON 0x007f6de0
// Creates an empty clipped scrolling text box that scrolls one pixel every 60 ms
ScrollingTextComponent::ScrollingTextComponent()
    :mLastColor(0),mTimeSinceLastScroll(0),mScrollSpeed(60),mScrollTop(0),mTotalTextHeight(0)
{
    this->SetComponentFlags(ComponentFlag_Clip,true);
}
// FUNCTION: WON 0x007f7490
// Creates the credits dialog with its scrolling text box
CreditsDialog::CreditsDialog()
{
    this->mScrollingText=new ScrollingTextComponent;
}
}

namespace WONAPI {
// The original (0x7f6e90, 182 bytes) stores the class's own vtable (0x856018) at 0x7f6eb2, then destroys the
// line list at 0x34, the font at 0x30 and the component base
// FUNCTION: WON 0x007f6e90
// Destroys the scrolling text box (members only)
ScrollingTextComponent::~ScrollingTextComponent() {}
}

namespace WONAPI {
// FUNCTION: WON 0x007f6f50
// Starts the scroll timer and restarts the text from the bottom
void ScrollingTextComponent::Start()
{
    this->RequestTimer(true);
    this->Reset();
}
// FUNCTION: WON 0x007f6f70
// Stops the scroll timer
void ScrollingTextComponent::Stop()
{
    this->RequestTimer(false);
}
// FUNCTION: WON 0x007f6f80
// Moves the text back to just below the bottom edge
void ScrollingTextComponent::Reset()
{
    this->mScrollTop=this->Height();
}
// FUNCTION: WON 0x007f7750
// Starts scrolling the credits from the bottom
void CreditsDialog::StartScrolling()
{
    this->mScrollingText->Start();
}
// FUNCTION: WON 0x007f7760
// Stops scrolling the credits
void CreditsDialog::StopScrolling()
{
    this->mScrollingText->Stop();
}
// FUNCTION: WON 0x007f7770
// Moves the credits back to the bottom
void CreditsDialog::ResetScrolling()
{
    this->mScrollingText->Reset();
}
}

namespace WONAPI {

// FUNCTION: WON 0x007f7450
// Returns a copy of the credit line's text
GUIString CreditLineInfo::GetLine()
{
    return this->mLine;
}
}

namespace WONAPI {
// UpdateCredits is empty (the credit lines come from the layout file); the original uses the shared one-byte
// return at 0x8142a0
void CreditsDialog::UpdateCredits()
{
}
// FUNCTION: WON 0x007fa1f0
// Passes every event to the dialog base (vtable slot 0x7c)
void CreditsDialog::HandleComponentEvent(ComponentEvent* event)
{
    Dialog::HandleComponentEvent(event);
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007f6e70
// (public: virtual void * __thiscall WONAPI::ScrollingTextComponent::`scalar deleting destructor'(unsigned int)    [??_GScrollingTextComponent@WONAPI@@UA)

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007fa7a0
// (void __cdecl std::_Construct(class WONAPI::CreditLineInfo *,class WONAPI::CreditLineInfo const &)    [?_Construct@std@@YAXPAVCreditLineInfo@WONAPI@@AB)

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007f7560
// (public: virtual void * __thiscall WONAPI::CreditsDialog::`scalar deleting destructor'(unsigned int)    [??_GCreditsDialog@WONAPI@@UAEPAXI@Z])
// TEMPLATE: WON 0x007f7580
// (public: virtual __thiscall WONAPI::CreditsDialog::~CreditsDialog(void)    [??1CreditsDialog@WONAPI@@UAE@XZ])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007fa350
// (protected: void __thiscall std::list<class WONAPI::CreditLineInfo,class std::allocator<class WONAPI::CreditLineInfo> >::_Incsize(unsigned int))
