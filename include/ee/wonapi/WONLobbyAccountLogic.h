// AccountLogic (login, account creation, lost password/user name, terms of use) and InitLogic (lobby start-up:
// DNS test, HTTP documents, directory query, version check, firewall test); 0x5c and 0x90 bytes

#ifndef EE_WONAPI_WONLOBBYACCOUNTLOGIC_H
#define EE_WONAPI_WONLOBBYACCOUNTLOGIC_H
#include "ee/wonapi/WONLobbyLogic.h"
#include "ee/wonapi/WONLobbyDialogs.h"
namespace WONAPI {
class ComponentEvent;
class HTTPSession;
class AccountLogic : public LobbyLogic {
public:
    AccountLogic();
    virtual ~AccountLogic();
    virtual void InitComponent(ComponentConfig* inConfig);
    virtual void HandleControlEvent(int inControlId);
    virtual void HandleComponentEvent(ComponentEvent* inEvent);
    void ShowCredits();
    bool ShowTOU();
    bool CheckLoginStatus();
    void HandleCreateAccount(ComponentEvent* inEvent);
    void HandleLostPassword(ComponentEvent* inEvent);
    void HandleLostUsername(ComponentEvent* inEvent);
    void HandleLogin(ComponentEvent* inEvent);
    void QueryAccount();
    void HandleUpdateAccount(ComponentEvent* inEvent);
    std::wstring mLastUserName; // 0x40
    bool mNeedTOU; // 0x50
    SmartPtr<CreditsDialog> mCreditsDialog; // 0x54
    SmartPtr<TOUDialog> mTOUDialog; // 0x58
};
class InitLogic : public LobbyLogic {
public:
    InitLogic();
    virtual ~InitLogic();
    virtual void KillHook();
    virtual void InitComponent(ComponentConfig* inConfig);
    virtual void HandleControlEvent(int inControlId);
    virtual void HandleComponentEvent(ComponentEvent* inEvent);
    void ChooseNetworkAdapter();
    bool SetError(int inError, const GUIString& inDescription);
    void Start(bool inRetry);
    void Init();
    void StartDNSTest();
    void DNSCompletion(AsyncOp* inOp);
    void StartHTTPDocuments();
    void HTTPDocumentCompletion(AsyncOp* inOp);
    void ParseCrossPromotion(const ByteBuffer* inDocument);
    void CrossPromotionImageCompletion(AsyncOp* inOp);
    void StartDirectoryQuery();
    void DirectoryCompletion(AsyncOp* inOp);
    void StartVersionCheck();
    void StartFirewallTest();
    void VersionCompletion(AsyncOp* inOp);
    void AcceptCompletion(AsyncOp* inOp);
    void FirewallCompletion(AsyncOp* inOp);
    void HandleTOU(RustServerOp* inOp);
    void LoadTOUTimes();
    void KillFirewallTest();
    SmartPtr<HTTPSession> mHTTPSession; // 0x40
    std::string mCrossPromotion; // 0x44, narrow string (assigned at 0x7ddaf1)
    SmartPtr<AcceptOp> mAcceptOp; // 0x54
    SmartPtr<AsyncSocket> mFirewallSocket; // 0x58
    IPAddr mFirewallAddress; // 0x5c
    SmartPtr<ChooseNetworkAdapterDialog> mNetworkAdapterDialog; // 0x84
    unsigned long mErrors; // 0x88
    unsigned long mIgnoredErrors; // 0x8c
};
}
#endif
