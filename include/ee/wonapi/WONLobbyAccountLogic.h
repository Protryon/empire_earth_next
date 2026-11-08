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
    AccountLogic(); // 0x7db150
    virtual ~AccountLogic(); // 0x7db1d0
    virtual void InitComponent(ComponentConfig* inConfig); // 0x7db2a0
    virtual void HandleControlEvent(int inControlId); // 0x7dc2e0
    virtual void HandleComponentEvent(ComponentEvent* inEvent); // 0x7dc340
    void ShowCredits(); // 0x7db2e0
    bool ShowTOU(); // 0x7db310
    bool CheckLoginStatus(); // 0x7db330
    void HandleCreateAccount(ComponentEvent* inEvent); // 0x7db370
    void HandleLostPassword(ComponentEvent* inEvent); // 0x7db830
    void HandleLostUsername(ComponentEvent* inEvent); // 0x7db920
    void HandleLogin(ComponentEvent* inEvent); // 0x7dba10
    void QueryAccount(); // 0x7dbee0
    void HandleUpdateAccount(ComponentEvent* inEvent); // 0x7dc160
    std::wstring mLastUserName; // 0x40
    bool mNeedTOU; // 0x50
    SmartPtr<CreditsDialog> mCreditsDialog; // 0x54
    SmartPtr<TOUDialog> mTOUDialog; // 0x58
};
class InitLogic : public LobbyLogic {
public:
    InitLogic(); // 0x7dd100
    virtual ~InitLogic(); // 0x7dd200
    virtual void KillHook(); // 0x7dfb00
    virtual void InitComponent(ComponentConfig* inConfig); // 0x7dd340
    virtual void HandleControlEvent(int inControlId); // 0x7dfb20
    virtual void HandleComponentEvent(ComponentEvent* inEvent); // 0x7dfb50
    void ChooseNetworkAdapter(); // 0x7dd370
    bool SetError(int inError, const GUIString& inDescription); // 0x7dd3b0
    void Start(bool inRetry); // 0x7dd570
    void Init(); // 0x7dd630
    void StartDNSTest(); // 0x7def30
    void DNSCompletion(AsyncOp* inOp); // 0x7dd810
    void StartHTTPDocuments(); // 0x7df0c0
    void HTTPDocumentCompletion(AsyncOp* inOp); // 0x7de280
    void ParseCrossPromotion(const ByteBuffer* inDocument); // 0x7ddc00
    void CrossPromotionImageCompletion(AsyncOp* inOp); // 0x7dda10
    void StartDirectoryQuery(); // 0x7df410
    void DirectoryCompletion(AsyncOp* inOp); // 0x7de4c0
    void StartVersionCheck(); // 0x7df6c0
    void StartFirewallTest(); // 0x7df880
    void VersionCompletion(AsyncOp* inOp); // 0x7de8f0
    void AcceptCompletion(AsyncOp* inOp); // 0x7deb50
    void FirewallCompletion(AsyncOp* inOp); // 0x7ded20
    void HandleTOU(GetHTTPDocumentOp* inOp); // 0x7de090
    void LoadTOUTimes(); // 0x7ddef0
    void KillFirewallTest(); // 0x7dfa80
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
