// SDK config classes (declarations only).
#ifndef EE_WONAPI_WONLOBBYCONTROLCONFIG_H
#define EE_WONAPI_WONLOBBYCONTROLCONFIG_H
#include "ee/wonapi/WONLobbyControls.h"
#include "ee/wonapi/WONLobbyScreens.h"
#include "ee/wonapi/WONLobbyRoomServer.h"
#include "ee/wonapi/WONLobbyImageButton.h"
#include "ee/wonapi/WONLobbyCrossPromotion.h"
#include "ee/wonapi/WONLobbyMSConfig.h"
namespace WONAPI {
class ImageTextButton;
class CrossPromotionButton;
class MaxSplitterButton;
class MultiListHeader;
class DirectConnectCtrl : public Container {
public:
    DirectConnectCtrl(); // empty, at 0x007bada0
    virtual ~DirectConnectCtrl(); // empty; stores its own vptr (0x007bae0f)
    virtual void InitComponent(ComponentInit& inInit); // at 0x007baeb0
    virtual void HandleComponentEvent(ComponentEvent* inEvent); // at 0x007baf20
    SmartPtr<ServerListCtrl> mConnectionList; //84
    SmartPtr<Button> mAddButton; //88
    SmartPtr<Button> mRemoveButton; //8c
};

class ChatCtrlConfig : public ContainerConfig {
public:
    static ComponentConfig* CfgFactory();
    static Component* CompFactory() { return new ChatCtrl; }
    virtual bool Virtual24(const std::string&); //3b9e10
    virtual void SetComponent(Component*); // at 0x007c8380
    ChatCtrl* mChatCtrl; // offset 0x98, borrowed (written at 0x007c838e)
};

class RoomLabelCtrlConfig : public ContainerConfig {
public:
    static ComponentConfig* CfgFactory();
    static Component* CompFactory() { return new RoomLabelCtrl; }
    virtual bool Virtual24(const std::string&);
    virtual void SetComponent(Component*);
    RoomLabelCtrl* mRoomLabel; // offset 0x98, borrowed; 0x0079afb0 writes it before calling ContainerConfig
};

class ServerListCtrlConfig : public ContainerConfig {
public:
    ServerListCtrlConfig(); // at 0x007c8360 (defined out of line)
    static ComponentConfig* CfgFactory();
    static Component* CompFactory() { return new ServerListCtrl; }
    virtual bool Virtual24(const std::string&);
    virtual bool GetNamedValue(const std::string&,int&);
    virtual void SetComponent(Component*);
    void HandleAddColumn();
    ServerListCtrl* mServerList; //98 borrowed
    MultiListHeader* mHeader; //9c borrowed
    int mNumColumns; //a0
};

class ImageTextButtonConfig : public ImageButtonConfig {
public:
    static ComponentConfig* CfgFactory();
    static Component* CompFactory() { return new ImageTextButton; }
    virtual bool Virtual24(const std::string&);
    virtual void SetComponent(Component*);
    ImageTextButton* mImageTextButton; // offset 0x7c, borrowed (RTTI 0x00851d64)
};

class CrossPromotionButtonConfig : public ImageButtonConfig {
public:
    static ComponentConfig* CfgFactory();
    static Component* CompFactory() { return new CrossPromotionButton; }
    virtual bool Virtual24(const std::string&);
    virtual void SetComponent(Component*);
    CrossPromotionButton* mCrossPromotionButton; //7c borrowed
};

class GameBrowserCtrlConfig : public ContainerConfig {
public:
    static ComponentConfig* CfgFactory();
    static Component* CompFactory() { return new GameBrowserCtrl; }
    virtual bool Virtual24(const std::string&); // HandleInstruction at 0x007bc450
    virtual void SetComponent(Component* component) { ContainerConfig::SetComponent(component); this->mBrowser=static_cast<GameBrowserCtrl*>(component); } // inline
    GameBrowserCtrl* mBrowser; //98, borrowed
};

class GameUserListCtrlConfig : public ContainerConfig {
public:
    static ComponentConfig* CfgFactory();
    static Component* CompFactory() { return new GameUserListCtrl; }
    virtual bool Virtual24(const std::string&); // HandleInstruction at 0x007bda70
    virtual void SetComponent(Component* component) { ContainerConfig::SetComponent(component); this->mUserList=static_cast<GameUserListCtrl*>(component); } // inline
    GameUserListCtrl* mUserList; //98, borrowed
};

class UserListCtrlConfig : public ContainerConfig {
public:
    static ComponentConfig* CfgFactory();
    static Component* CompFactory() { return new UserListCtrl; }
    virtual bool Virtual24(const std::string&); // HandleInstruction at 0x007cb770
    virtual void SetComponent(Component* component) { ContainerConfig::SetComponent(component); this->mUserList=static_cast<UserListCtrl*>(component); } // inline
    UserListCtrl* mUserList; //98, borrowed
};

class MaxSplitterButtonConfig : public MSArrowButtonConfig {
public:
    static ComponentConfig* CfgFactory();
    static Component* CompFactory();
    virtual bool Virtual24(const std::string&); // HandleInstruction at 0x007c9680
    virtual void SetComponent(Component*); // at 0x007658a0
    MaxSplitterButton* mButton; //80, borrowed
};

class BrowserComponentConfig : public ContainerConfig {
public:
    static void Init(ResourceConfigTable*); // SDK setup, at 0x007fab70
    static ComponentConfig* CfgFactory(); // at 0x007facb0
    static Component* CompFactory(); // at 0x007fac50
    virtual bool Virtual24(const std::string&); // HandleInstruction at 0x007fa8a0
    virtual bool GetPredefinedComponent(const std::string&,ComponentConfig*&,Component*&); //3fa7e0, GetPredefinedComponent
    virtual void SetComponent(Component*); // at 0x0078f960
    BrowserComponent* mBrowserComponent; //98, borrowed; complete config9c, no lifetime implementation
};
}
#endif
