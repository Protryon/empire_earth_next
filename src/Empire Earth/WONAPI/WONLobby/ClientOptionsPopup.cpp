// Right-click menu of a player in a user list: whisper, invite, friends, ignore, block, AFK and admin actions.
// Room spec bits: 2 = game room, 4 = Internet server (Init's locals isLAN and isGame hold these two bits)
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <stdio.h>
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
// FUNCTION: WON 0x7f5e00
// Destroys the player menu, releasing its menu and player
ClientOptionsPopup::~ClientOptionsPopup() {}
// FUNCTION: WON 0x007f5ea0
// Adds a menu entry with the given text and action
void ClientOptionsPopup::AddItem(const GUIString& inText,int inAction)
{
    this->mPopupMenu->AddItem(inText,NULL,NULL,inAction);
}
// FUNCTION: WON 0x7f5ec0
// Builds the menu for a player (name in bold, user id for moderators) from what we may do to them in this room
void ClientOptionsPopup::Init(LobbyClient* inClient,LobbyClient* inMyClient,int inRoomSpec)
{
    if(inClient==NULL || inMyClient==NULL) return;
    this->mClient=inClient;
    this->mRoomSpecFlags=inRoomSpec;
    this->mPopupMenu->Clear(false);
    bool isLAN=(inRoomSpec&2)!=0;
    bool isGame=(inRoomSpec&4)!=0;
    bool isModerator=inMyClient->IsModerator();
    bool isCaptain=inMyClient->IsCaptain(isLAN);
    bool isSelf=inMyClient->GetClientId()==inClient->GetClientId();
    SmartPtr<Font> anOldFont=this->mPopupMenu->mFont;
    SmartPtr<Font> aBoldFont=this->GetFontMod(anOldFont.get(),1,0);
    this->mPopupMenu->SetFont(aBoldFont);
    this->mPopupMenu->AddItem(inClient->mName,0)->mSelectable=false;
    if(isModerator) {
        char aBuffer[256];
        sprintf(aBuffer,"%d",inClient->GetWONUserId());
        MenuItem* anItem=this->mPopupMenu->AddItem(GUIString(aBuffer),0);
        anItem->mSelectable=false;
    }
    this->mPopupMenu->mFont=anOldFont;
    this->AddItem(::ClientOptionsCtrl_Whisper_String,16);
    if(!isSelf && inMyClient->IsCaptain(1) && inClient->GetGameGroupId()!=inMyClient->GetGameGroupId()) {
        if(!this->mClient->mInvited) this->AddItem(::ClientOptionsCtrl_Invite_String,6);
        else this->AddItem(::ClientOptionsCtrl_Uninvite_String,13);
    }
    if(isGame) {
        if(!isSelf) this->AddItem(::FriendOptionsCtrl_Locate_String,17);
        if(!inClient->IsFriend()) this->AddItem(::ClientOptionsCtrl_AddToFriends_String,1);
        else this->AddItem(::ClientOptionsCtrl_RemoveFromFriends_String,9);
        this->mPopupMenu->AddSeperator();
    }
    if(isSelf && isGame) {
        if(inClient->IsAway()) this->AddItem(::ClientOptionsCtrl_ClearAFK_String,4);
        else this->AddItem(::ClientOptionsCtrl_SetAFK_String,10);
    } else {
        if(!inClient->mIgnored) this->AddItem(::ClientOptionsCtrl_Ignore_String,5);
        else this->AddItem(::ClientOptionsCtrl_Unignore_String,12);
        if(isGame) {
            if(!inClient->mBlocked) this->AddItem(::ClientOptionsCtrl_Block_String,3);
            else this->AddItem(::ClientOptionsCtrl_Unblock_String,11);
        }
    }
    if(isGame) {
        if(isModerator || isCaptain) {
            this->mPopupMenu->AddSeperator();
            if(isModerator) this->AddItem(::ClientOptionsCtrl_Warn_String,15);
            if(inClient->IsMuted(isLAN)) this->AddItem(::ClientOptionsCtrl_UnMute_String,14);
            else this->AddItem(::ClientOptionsCtrl_Mute_String,8);
            if(!isSelf) {
                this->AddItem(::ClientOptionsCtrl_Kick_String,7);
                this->AddItem(::ClientOptionsCtrl_Ban_String,2);
            }
        }
    } else if(isCaptain && !isSelf) {
        this->mPopupMenu->AddSeperator();
        this->AddItem(::ClientOptionsCtrl_Kick_String,7);
    }
    this->SetSize(this->mPopupMenu->Width(),this->mPopupMenu->Height());
}
// FUNCTION: WON 0x7f6310
// On a left click on an entry sends its action for the player and room, then closes the menu
void ClientOptionsPopup::HandleComponentEvent(ComponentEvent* inEvent)
{
    if(inEvent->mType!=ComponentEvent_ListItemUpClicked) {
        Container::HandleComponentEvent(inEvent);
        return;
    }
    ListItemClickedEvent* anEvent=static_cast<ListItemClickedEvent*>(inEvent);
    if(anEvent->mItem.get()==NULL || anEvent->mButton!=0) return;
    int anAction=static_cast<MenuItem*>(anEvent->mItem.get())->mMenuId;
    if(anAction!=0) this->DeliverEvent(new ClientActionEvent(anAction,this->GetRoomSpecFlags(),this->mClient.get()),false);
    this->GetRoot()->ClosePopup(this);
}
}

namespace WONAPI {
// FUNCTION: WON 0x007f5d10
// Creates the player menu popup with an empty menu
ClientOptionsPopup::ClientOptionsPopup()
{
    this->mPopupMenu=new PopupMenu;
    this->AddChild(this->mPopupMenu.get());
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007f5de0
// (protected: virtual void * __thiscall WONAPI::ClientOptionsPopup::`scalar deleting destructor'(unsigned int)    [??_GClientOptionsPopup@WONAPI@@MAEPAXI)
