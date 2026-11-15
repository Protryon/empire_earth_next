// The lobby's top container, or the "Lobby Error" dialog shown when the lobby failed to start
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
#include "ee/wonapi/WONLobbyConfig.h"
#include "ee/wonapi/WONLobbyCore.h"
#include "ee/wonapi/WONLobbyScreens.h"
#include "ee/wonapi/WONLobbyMSPainters.h"
namespace WONAPI {
extern SmartPtr<Dialog> gLobbyFailureDialog;
extern GUIString gLobbyInitError;
extern bool gLobbyInitFailed;
// FUNCTION: WON 0x00762250
// Returns the lobby container; after a failed start builds once and returns a dialog showing the error and Ok
Container* LobbyGetContainer()
{
    if(gLobbyData.get()!=NULL) return gLobbyData->mLobbyContainer.get();
    if(gLobbyInitFailed) {
    if(gLobbyFailureDialog.get()==NULL) {
        SmartPtr<Dialog> dialog=new Dialog;
        dialog->SetEndOnEscape(0);
        SmartPtr<MSDlgBack> background=new MSDlgBack;
        SmartPtr<Label> label=new Label;
        label->SetWrap(true);
        label->SetHorzAlign(HorzAlign_Center);
        label->SetAdjustHeightToFitText(true);
        GUIString text("Lobby Error\n\n");
        text.append(gLobbyInitError);
        label->SetText(text,true);
        SmartPtr<MSButton> button=new MSButton(GUIString("Ok"));
        button->SetButtonFlags(0x100,true);
        button->SetSizeAtLeast(100,0);
        button->RequestFocus();
        dialog->Fn2F6140(background.get(),3,dialog.get(),0,0,0,0,0);
        dialog->Fn2F6140(label.get(),0x1001,dialog.get(),10,0,-20,0,0);
        dialog->AddChildLayout(new CenterLayout(dialog.get(),button.get(),true,false),0);
        dialog->Fn2F6140(button.get(),0x200,label.get(),0,20,0,0,0);
        CenterLayout* vertical=new CenterLayout(dialog.get(),label.get(),false,true);
        vertical->Add(button.get());
        dialog->AddChildLayout(vertical,0);
        dialog->AddChild(background.get());
        dialog->AddChild(label.get());
        dialog->AddChild(button.get());
        gLobbyFailureDialog=dialog;
    }
    return gLobbyFailureDialog.get();
    }
    return NULL;
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x00766b30
// (public: __thiscall std::list<class WONAPI::Component *,class std::allocator<class WONAPI::Component *> >::list<class WONAPI::Component *,class std::al)
