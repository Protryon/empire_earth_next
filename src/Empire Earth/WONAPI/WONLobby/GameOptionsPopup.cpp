// Right-click menu of a game in the game list: join, ping, query details, show/hide details, remove
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyEvents.h"
#include "ee/wonapi/WONLobbyDialogs.h"
namespace WONAPI {
// FUNCTION: WON 0x7f8050
// Destroys the game menu, releasing its menu and game
GameOptionsPopup::~GameOptionsPopup() {}

// FUNCTION: WON 0x007f80f0
// Adds a menu entry with the given text and action
void GameOptionsPopup::AddItem(const GUIString& inText,int inAction)
{
    this->mPopupMenu->AddItem(inText,NULL,NULL,inAction);
}
// FUNCTION: WON 0x7f8110
// Builds the menu for a game: its name in bold, then the actions it allows, and sizes the popup to the menu
void GameOptionsPopup::Init(LobbyGame* inGame)
{
    this->mPopupMenu->Clear(false);
    this->mGame=inGame;
    SmartPtr<Font> anOldFont=this->mPopupMenu->mFont.get();
    SmartPtr<Font> aBoldFont=this->mPopupMenu->GetFontMod(anOldFont.get(),1,0);
    this->mPopupMenu->SetFont(aBoldFont);
    this->mPopupMenu->AddItem(inGame->mName,0)->mSelectable=false;
    this->mPopupMenu->SetFont(anOldFont);
    this->mPopupMenu->AddSeperator();
    if(inGame->CanJoin()) this->AddItem(::GameOptionsPopup_Join_String,1);
    bool allowPing=true;
    if(inGame->IsInProgress()) {
        LobbyConfig* aConfig=GetLobbyConfig();
        if(aConfig!=NULL) allowPing=!aConfig->GetNoPingInProgress();
    }
    if(allowPing) {
        if(inGame->GetGameType()!=1) this->AddItem(::GameOptionsPopup_Ping_String,2);
        if(allowPing) this->AddItem(::GameOptionsPopup_QueryDetails_String,3);
    }
    if(inGame->GetShowDetails()) this->AddItem(::GameOptionsPopup_HideDetails_String,5);
    else this->AddItem(::GameOptionsPopup_ShowDetails_String,4);
    if(inGame->GetGameType()==3) {
        this->mPopupMenu->AddSeperator();
        this->AddItem(::GameOptionsPopup_Remove_String,6);
    }
    this->SetSize(this->mPopupMenu->Width(),this->mPopupMenu->Height());
}
// FUNCTION: WON 0x7f82f0
// On a left click on an entry sends its action for the game and closes the menu; the rest goes to Container
void GameOptionsPopup::HandleComponentEvent(ComponentEvent* inEvent)
{
    if(inEvent->mType!=ComponentEvent_ListItemUpClicked) {
        Container::HandleComponentEvent(inEvent);
        return;
    }
    ListItemClickedEvent* anEvent=static_cast<ListItemClickedEvent*>(inEvent);
    if(anEvent->mItem.get()==NULL || anEvent->mButton!=0) return;
    int anAction=static_cast<MenuItem*>(anEvent->mItem.get())->mMenuId;
    if(anAction!=0) this->DeliverEvent(new GameActionEvent(anAction,this->mGame.get()),false);
    this->GetRoot()->ClosePopup(this);
}
}

namespace WONAPI {
// FUNCTION: WON 0x007f7f60
// Creates the game menu popup with an empty menu
GameOptionsPopup::GameOptionsPopup()
{
    this->mPopupMenu=new PopupMenu;
    this->AddChild(this->mPopupMenu.get());
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007f8030
// (protected: virtual void * __thiscall WONAPI::GameOptionsPopup::`scalar deleting destructor'(unsigned int)    [??_GGameOptionsPopup@WONAPI@@MAEPAXI@Z])
