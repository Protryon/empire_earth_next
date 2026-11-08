// The login screen's message of the day (system and game MOTD in one page) and the ping bar drawing helper
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <map>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyScreens.h"
#include "ee/wonapi/WONLobbyEvents.h"
namespace WONAPI {
MOTDCtrl::MOTDCtrl() : mNeedGenHTML(false) {}
// The original stores its own vtable (0x7c0bff) before destroying the two paths, the browser and the base
// FUNCTION: WON 0x007c0be0
// Destroys the MOTD control, releasing both MOTD paths and the browser
MOTDCtrl::~MOTDCtrl() {}
// FUNCTION: WON 0x007c0cd0
// Finds the MOTD browser in the layout, makes its links open the web browser and listens to broadcasts
void MOTDCtrl::InitComponent(ComponentInit& inInit)
{
    if(inInit.mType!=2)
        return;
    SafeGetComponent(static_cast<ComponentConfigInit&>(inInit).mConfig,this->mBrowser,"Browser",__FILE__,36);
    this->mBrowser->SetLaunchBrowserOnLink(true);
    this->SubscribeToBroadcast(1);
}
// The document parameter flags (14) are a full int
// FUNCTION: WON 0x007c0d20
// Builds and shows the MOTD page (system MOTD, then game MOTD) once both documents have arrived
void MOTDCtrl::Paint(Graphics& inGraphics)
{
    if(this->mNeedGenHTML) {
        this->mNeedGenHTML=false;
        if(!this->mSysMOTDPath.empty() && !this->mGameMOTDPath.empty()) {
            HTMLDocumentGen generator(NULL);
            generator.AddDocParam(GUIString(this->mSysMOTDPath),14);
            generator.AddDocParam(GUIString(this->mGameMOTDPath),14);
            this->mSysMOTDPath.erase();
            this->mGameMOTDPath.erase();
            generator.printf(GUIString("<body>%1%<br><br>%2%</body>"));
            this->mBrowser->Go(generator.GetDocument());
        }
    }
    Container::Paint(inGraphics);
}
// Only the document type is checked, not whether the download succeeded
// FUNCTION: WON 0x007c1050
// Takes the system and game MOTD file paths from a finished MOTD download and repaints
void MOTDCtrl::ShowMessage(GetHTTPDocumentOp* inOp)
{
    if(inOp->GetDocType()!=HTTPDocType_MOTD) return;
    this->mNeedGenHTML=true;
    this->mSysMOTDPath=inOp->GetDocumentPath(HTTPDocOwner_Sys);
    this->mGameMOTDPath=inOp->GetDocumentPath(HTTPDocOwner_Game);
    this->Invalidate();
}
// FUNCTION: WON 0x007c1280
// Handles got-HTTP-document broadcasts; passes every other event to the base component
void MOTDCtrl::HandleComponentEvent(ComponentEvent* inEvent)
{
    if(inEvent->mType==LobbyEvent_GotHTTPDocument) {
        this->ShowMessage(static_cast<GotHTTPDocumentEvent*>(inEvent)->mOp.get());
        return;
    }
    Component::HandleComponentEvent(inEvent);
}
// The ping ceiling (0x8df4cc) starts at 5000 and is taken from the lobby config while it is still 5000
static unsigned long sLobbyPingCeiling=5000;
// The text is always empty, so its branches never draw anything (as in the original)
// FUNCTION: WON 0x007c12b0
// Draws a ping as 1-10 green bars scaled by the config's ping ceiling, or the searching or failed icon
void DrawLobbyPing(Graphics* inGraphics,unsigned short inPing,int inHeight,int inAlignment,int inWidth)
{
    if(inPing==0xfffe) return;
    if(sLobbyPingCeiling==5000) {
        LobbyConfig* config=GetLobbyConfig();
        if(config!=NULL) sLobbyPingCeiling=config->mPingCeiling;
    }
    GUIString text(GetEmptyGUIStringData());
    SmartPtr<Image> image;
    int bars=0;
    if(inPing==0xfffd) image=::LobbyGlobal_Searching_Image.get();
    else if(inPing==0xfffc) { image=::LobbyGlobal_Searching_Image; image=image.get(); }
    else if(inPing==0xffff) image=::ServerListCtrl_PingFailed_Image.get();
    else {
        bars=10-int((static_cast<unsigned long>(inPing)*10)/sLobbyPingCeiling);
        if(bars<1) bars=1;
        else if(bars>10) bars=10;
    }
    int x=0;
    if(inAlignment!=0) {
        int width;
        if(text.length()!=0) width=inGraphics->mFont->GetStringWidth(text,0,text.length());
        else if(image.get()!=NULL) width=image->GetWidth();
        else width=bars*3;
        if(inAlignment==1) x=(inWidth-width)/2;
        else x=inWidth-width;
        if(x<0) x=0;
    }
    if(text.length()!=0) inGraphics->DrawString(text,x,0);
    else if(image.get()!=NULL) {
        int y=(inHeight-image->GetHeight())/2;
        if(y<0) y=0;
        inGraphics->DrawImage(image.get(),x,y);
    } else {
        inGraphics->SetColor(0x10d810);
        int height=inHeight-2;
        while(bars>0) {
            inGraphics->FillRect(x,1,2,height);
            x+=3;
            --bars;
        }
    }
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007c0b40
// (public: __thiscall WONAPI::MOTDCtrl::MOTDCtrl(void)    [??0MOTDCtrl@WONAPI@@QAE@XZ])
// TEMPLATE: WON 0x007c0bc0
// (public: virtual void * __thiscall WONAPI::MOTDCtrl::`scalar deleting destructor'(unsigned int)    [??_GMOTDCtrl@WONAPI@@UAEPAXI@Z])

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007c0fb0
// (public: __thiscall WONAPI::HTMLDocumentGen::~HTMLDocumentGen(void))
// TEMPLATE: WON 0x0071b370
// (protected: void __thiscall std::vector<class WONAPI::SmartPtr<class WONAPI::HTMLDocument>,class std::allocator<class WONAPI::SmartPtr<class WONAPI::HT)
