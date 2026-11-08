// Right-click menu of a friend in the friends list: locate, refresh list, remove from friends
// UNIT-FLAGS: -O2 -MD -GR -GX -DEE_DINKUM_SXL233
#include <windows.h>
#include <string>
#include <list>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyModel.h"
#include "ee/wonapi/WONLobbyEvents.h"
#include "ee/wonapi/WONLobbyDialogs.h"
namespace WONAPI {
// FUNCTION: WON 0x7f7c50
// Destroys the friend menu, releasing its menu and friend
FriendOptionsPopup::~FriendOptionsPopup() {}

void FriendOptionsPopup::AddItem(const GUIString& inText,int inAction)
{
    this->mPopupMenu->AddItem(inText,NULL,NULL,inAction);
}
// FUNCTION: WON 0x7f7cf0
// Builds the menu for a friend: the name in bold, then locate, refresh list and remove from friends
void FriendOptionsPopup::Init(LobbyFriend* inFriend,int inRoomSpec)
{
    if(inFriend==NULL) return;
    this->mFriend=inFriend;
    this->mRoomSpecFlags=inRoomSpec;
    this->mPopupMenu->Clear(false);
    SmartPtr<Font> anOldFont=this->mPopupMenu->mFont.get();
    SmartPtr<Font> aBoldFont=this->GetFontMod(anOldFont.get(),1,0);
    this->mPopupMenu->SetFont(aBoldFont);
    this->mPopupMenu->AddItem(inFriend->mName,0)->mSelectable=false;
    this->mPopupMenu->SetFont(anOldFont);
    this->mPopupMenu->AddSeperator();
    this->AddItem(::FriendOptionsCtrl_Locate_String,1);
    this->AddItem(::FriendOptionsCtrl_RefreshList_String,2);
    this->mPopupMenu->AddSeperator();
    this->AddItem(::FriendOptionsCtrl_RemoveFromFriends_String,3);
    this->SetSize(this->mPopupMenu->Width(),this->mPopupMenu->Height());
}
// FUNCTION: WON 0x7f7ea0
// On a left click on an entry sends its action for the friend, then closes the menu
void FriendOptionsPopup::HandleComponentEvent(ComponentEvent* inEvent)
{
    if(inEvent->mType!=ComponentEvent_ListItemUpClicked) {
        Container::HandleComponentEvent(inEvent);
        return;
    }
    ListItemClickedEvent* anEvent=static_cast<ListItemClickedEvent*>(inEvent);
    if(anEvent->mItem.get()==NULL || anEvent->mButton!=0) return;
    int anAction=static_cast<MenuItem*>(anEvent->mItem.get())->mMenuId;
    if(anAction!=0) this->DeliverEvent(new FriendActionEvent(anAction,this->mFriend.get(),this->mRoomSpecFlags),false);
    this->GetRoot()->ClosePopup(this);
}
}

namespace WONAPI {
// FUNCTION: WON 0x007f7b60
// Creates the friend menu popup with an empty menu
FriendOptionsPopup::FriendOptionsPopup()
{
    this->mPopupMenu=new PopupMenu;
    this->AddChild(this->mPopupMenu.get());
}
}

// Compiler-generated and header-inline functions this unit emits:
// TEMPLATE: WON 0x007f7c30
// (protected: virtual void * __thiscall WONAPI::FriendOptionsPopup::`scalar deleting destructor'(unsigned int)    [??_GFriendOptionsPopup@WONAPI@@MAEPAXI)
