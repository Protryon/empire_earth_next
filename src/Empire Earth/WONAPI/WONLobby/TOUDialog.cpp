// The terms of use dialog: system and game terms in one page, accepted only after ticking the agree box
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONRouting.h"
#include "ee/wonapi/WONLobbyEvents.h"
#include "ee/wonapi/WONLobbyCore.h"
#include "ee/wonapi/WONLobbyDialogs.h"
namespace WONAPI {

TOUDialog::TOUDialog() : mNeedGenHTML(false) {}

// Paint has no address of its own: the original uses a shared 13-byte forwarding body
void TOUDialog::Paint(Graphics& inGraphics) { Container::Paint(inGraphics); }
// FUNCTION: WON 0x7f9910
// Finds the browser, agree box and accept/reject buttons in the layout and resets the dialog
void TOUDialog::InitComponent(ComponentInit& inInit)
{
    if(inInit.mType!=2) return;
    ComponentConfig* aConfig=static_cast<ComponentConfigInit&>(inInit).mConfig;
    SafeGetComponent(aConfig,this->mBrowser,"Browser",__FILE__,30);
    SafeGetComponent(aConfig,this->mAgreeCheck,"AgreeCheck",__FILE__,31);
    SafeGetComponent(aConfig,this->mAcceptButton,"AcceptButton",__FILE__,32);
    SafeGetComponent(aConfig,this->mRejectButton,"RejectButton",__FILE__,33);
    this->mAcceptButton->mControlId=1;
    this->mRejectButton->mControlId=-1;
    this->Reset();
    this->SubscribeToBroadcast(1);
}
// FUNCTION: WON 0x7f99c0
// Rebuilds the terms page if new terms arrived, scrolls to the top and unticks agree, disabling accept
void TOUDialog::Reset()
{
    if(this->mNeedGenHTML) {
        this->mNeedGenHTML=false;
        HTMLDocumentGen aGenerator(NULL);
        aGenerator.AddDocParam(GUIString(LobbyGetSysTOUPath()),6);
        aGenerator.AddDocParam(GUIString(LobbyGetGameTOUPath()),6);
        aGenerator.printf(GUIString(new GUIStringData("<body>%1%<br><br>%2%</body>")));
        this->mBrowser->Go(aGenerator.GetDocument());
    }
    this->mBrowser->GetHTMLView()->SetOffsets(0,0);
    this->mAgreeCheck->SetCheck(false,false);
    this->mAcceptButton->SetEnabled(false);
}
// FUNCTION: WON 0x7f9b90
// Marks the terms page for rebuilding when a terms of use download finished
void TOUDialog::HandleHTTPDocEvent(GotHTTPDocumentEvent* inEvent)
{
    if(inEvent->mOp->mDocumentType==HTTPDocType_TOU) this->mNeedGenHTML=true;
}
// FUNCTION: WON 0x7f9bb0
// Handles terms downloads and enables accept only while the agree box is ticked
void TOUDialog::HandleComponentEvent(ComponentEvent* inEvent)
{
    if(inEvent->mType==LobbyEvent_GotHTTPDocument) {
        this->HandleHTTPDocEvent(static_cast<GotHTTPDocumentEvent*>(inEvent));
        return;
    }
    if(inEvent->mType==ComponentEvent_ButtonPressed && inEvent->mComponent==this->mAgreeCheck.get()) {
        this->mAcceptButton->SetEnabled(this->mAgreeCheck->mChecked!=0);
        return;
    }
    Dialog::HandleComponentEvent(inEvent);
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007f97e0
// (public: virtual void * __thiscall WONAPI::TOUDialog::`scalar deleting destructor'(unsigned int)    [??_GTOUDialog@WONAPI@@UAEPAXI@Z])
// TEMPLATE: WON 0x007f9800
// (public: virtual __thiscall WONAPI::TOUDialog::~TOUDialog(void)    [??1TOUDialog@WONAPI@@UAE@XZ])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007f97a0
// (public: __thiscall WONAPI::TOUDialog::TOUDialog(void)    [??0TOUDialog@WONAPI@@QAE@XZ])
