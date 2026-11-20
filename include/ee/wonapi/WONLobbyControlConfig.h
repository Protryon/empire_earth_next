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

class ChatCtrlConfig : public ContainerConfig {
public:
    virtual bool Virtual24(const std::string&); //3b9e10
    virtual void SetComponent(Component*); // at 0x007c8380
    ChatCtrl* mChatCtrl; // offset 0x98, borrowed (written at 0x007c838e)
};

class RoomLabelCtrlConfig : public ContainerConfig {
public:
    virtual bool Virtual24(const std::string&);
    virtual void SetComponent(Component*);
    RoomLabelCtrl* mRoomLabel; // offset 0x98, borrowed; 0x0079afb0 writes it before calling ContainerConfig
};

class ServerListCtrlConfig : public ContainerConfig {
public:
    ServerListCtrlConfig(); // at 0x007c8360 (defined out of line)
    virtual bool Virtual24(const std::string&);
    virtual bool GetNamedValue(const std::string&,int&);
    virtual void SetComponent(Component*);
    void HandleAddColumn();
    ServerListCtrl* mServerList; //98 borrowed
    MultiListHeader* mHeader; //9c borrowed
    int mNumColumns; //a0
};

class ImageTextButtonConfig : public ComponentConfig {
public:
    ImageTextButtonConfig() : ComponentConfig(EeWonCfgKind::ImageButton) {}
    virtual bool Virtual24(const std::string&);
    virtual void SetComponent(Component*);
    ImageTextButton* mImageTextButton; // offset 0x7c, borrowed (RTTI 0x00851d64)
};

class CrossPromotionButtonConfig : public ComponentConfig {
public:
    CrossPromotionButtonConfig() : ComponentConfig(EeWonCfgKind::ImageButton) {}
    virtual bool Virtual24(const std::string&);
    virtual void SetComponent(Component*);
    CrossPromotionButton* mCrossPromotionButton; //7c borrowed
};

class GameBrowserCtrlConfig : public ContainerConfig {
public:
    virtual bool Virtual24(const std::string&); // HandleInstruction at 0x007bc450
    virtual void SetComponent(Component* component) { ContainerConfig::SetComponent(component); this->mBrowser=static_cast<GameBrowserCtrl*>(component); } // inline
    GameBrowserCtrl* mBrowser; //98, borrowed
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
