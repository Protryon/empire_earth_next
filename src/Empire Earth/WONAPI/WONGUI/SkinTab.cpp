// Skinned tab control: frame, transparency and standard tabs (file name inferred)
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <vector>
#include <map>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbySkinTab.h"
#include "ee/wonapi/WONLobbyMSPainters.h"
namespace WONAPI {
// FUNCTION: WON 0x0071AAA0
// Sets the tab frame: adds a stretched frame image over the page area and, when opaque, a dialog back inside it
void WONTabCtrl::SetFrame(const WONFrame &theFrame)
{
    this->mFrame = theFrame;
    SmartPtr<ImageComponent> aFrame = new ImageComponent;
    aFrame->mStretch = true;
    aFrame->SetImage(this->mFrame.mImage.get());
    // CLI_SameLeft | CLI_SameTop | CLI_SameSize = 0x5003.
    this->Fn2F6140(aFrame.get(),0x5003,this->mContainerLayout.get(),0,0,0,0,0);
    this->AddChild(aFrame.get());
    if(!this->mIsTransparent)
    {
        this->mBack = new MSDlgBack;
        this->Fn2F6140(this->mBack.get(),0x5003,aFrame.get(),this->mFrame.mLeft,this->mFrame.mTop,
            -this->mFrame.mRight-this->mFrame.mLeft,-this->mFrame.mTop-this->mFrame.mBottom,0);
        this->AddChild(this->mBack.get());
    }
}
// FUNCTION: WON 0x0071ac50
// Removes the dialog back when made transparent; never stores the flag or re-adds the back
void WONTabCtrl::SetTransparent(bool isTransparent)
{
    if(this->mBack.get()!=NULL && isTransparent)
        this->RemoveChild(this->mBack.get());
}
// FUNCTION: WON 0x0071AC70
// Adds a page with a skinned tab button showing the given text
void WONTabCtrl::AddStandardTab(const GUIString &theText,Container *theContainer)
{
    SmartPtr<WONButton> aButton = this->mSkin->GetTabButton();
    aButton->SetText(theText,true);
    aButton->SetDesiredWidth(0);
    this->AddTab(aButton.get(),theContainer);
}
}
