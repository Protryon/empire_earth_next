// Text area constructor
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include <deque>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyTextAreaConfig.h"
#include "ee/wonapi/WONComponentFlags.h"
namespace WONAPI {
// FUNCTION: WON 0x007117c0
// Creates an empty text area with word wrap and selection on, the named TextArea/TextAreaLink fonts
// (an underlined text font when the link font is the default) and 5-pixel padding
TextArea::TextArea()
{
    this->mSearchLine=new TextLine;
    this->mTextFlags=TextFlag_WordWrap|TextFlag_AllowSelection|TextFlag_ShowPartialLines|TextFlag_ScrollOnBottom;
    this->mHasBeenVisible=false;
    this->mTextColor=0x8000000e;
    this->SetFont(this->GetNamedFont("TextArea"));
    this->mLinkColor=0x0000ff;
    this->SetLinkFont(this->GetNamedFont("TextAreaLink"));
    if(this->mLinkFont.get()==this->GetRoot()->GetWindowManager()->GetDefaultFont())
    {
        FontDescriptor descriptor=this->mFont->GetDescriptor();
        descriptor.mStyle|=FontStyle_Underline;
        this->SetLinkFont(this->GetFont(descriptor));
    }
    this->SetComponentFlags(ComponentFlag_WantFocus,true);
    this->mMaxChars=80*2000;
    this->mMaxLines=0;
    this->mWrapIndentSize=0;
    this->mLineAlign=HorzAlign_Left;
    this->mTopPad=this->mLeftPad=this->mRightPad=this->mBottomPad=5;
    this->Reset(false);
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x00711b00
// (public: virtual void * __thiscall WONAPI::TextArea::`scalar deleting destructor'(unsigned int)    [??_GTextArea@WONAPI@@UAEPAXI@Z])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x00711b20
// (public: __thiscall std::map<class WONAPI::GUIString,int,struct std::less<class WONAPI::GUIString>,class std::allocator<struct std::pair<class WONAPI::)
