// SDK config classes (declarations only).
#ifndef EE_WONAPI_WONLOBBYCONTROLCONFIG_H
#define EE_WONAPI_WONLOBBYCONTROLCONFIG_H
#include "ee/wonapi/WONLobbyControls.h"
#include "ee/wonapi/WONLobbyScreens.h"
#include "ee/wonapi/WONLobbyRoomServer.h"
#include "ee/wonapi/WONLobbyImageButton.h"
#include "ee/wonapi/WONLobbyCrossPromotion.h"
#include "ee/wonapi/WONLobbyGUI.h"
namespace WONAPI {
class ImageTextButton;
class CrossPromotionButton;
class MultiListHeader;

// R6 WON lobby and network round 39: each config's borrowed typed copy of its component (ChatCtrlConfig's mChatCtrl
// at 0x98 and its SetComponent 0x7c8380, RoomLabelCtrlConfig's mRoomLabel, ServerListCtrlConfig's mServerList,
// GameBrowserCtrlConfig's mBrowser, the two buttons' configs' and CreditsDialogConfig's) is its component's
// static_cast where used (LobbyCfgComp); the SetComponent overrides that wrote them are gone.
template<class T> T* LobbyCfgComp(ComponentConfig* inConfig) { return static_cast<T*>(inConfig->mComponent.get()); }
class ChatCtrlConfig : public ContainerConfig {
public:
    virtual bool Virtual24(const std::string&); //3b9e10
};

class RoomLabelCtrlConfig : public ContainerConfig {
public:
    virtual bool Virtual24(const std::string&);
};

class ServerListCtrlConfig : public ContainerConfig {
public:
    ServerListCtrlConfig(); // at 0x007c8360 (defined out of line)
    virtual bool Virtual24(const std::string&);
    virtual bool GetNamedValue(const std::string&,int&);
    void HandleAddColumn();
    MultiListHeader* mHeader; //9c borrowed
    int mNumColumns; //a0
};

class ImageTextButtonConfig : public ComponentConfig {
public:
    ImageTextButtonConfig() : ComponentConfig(EeWonCfgKind::ImageButton) {}
    virtual bool Virtual24(const std::string&);
};

class CrossPromotionButtonConfig : public ComponentConfig {
public:
    CrossPromotionButtonConfig() : ComponentConfig(EeWonCfgKind::ImageButton) {}
    virtual bool Virtual24(const std::string&);
};

class GameBrowserCtrlConfig : public ContainerConfig {
public:
    virtual bool Virtual24(const std::string&); // HandleInstruction at 0x007bc450
};

class GameUserListCtrlConfig : public ContainerConfig {
public:
    static Component* CompFactory() { return new LobbyRustCtrl(EeWonLobbyCtlKind::UserList); } // GameUserListCtrl
    virtual bool Virtual24(const std::string&); // HandleInstruction at 0x007bda70
};

class UserListCtrlConfig : public ContainerConfig {
public:
    static Component* CompFactory() { return new LobbyRustCtrl(EeWonLobbyCtlKind::Users); } // UserListCtrl
    virtual bool Virtual24(const std::string&); // HandleInstruction at 0x007cb770
};

}
#endif
