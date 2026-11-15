// AccountLogic (login, account creation, lost password/user name, terms of use) and InitLogic (lobby start-up:
// DNS test, HTTP documents, directory query, version check, firewall test); 0x5c and 0x90 bytes

#ifndef EE_WONAPI_WONLOBBYACCOUNTLOGIC_H
#define EE_WONAPI_WONLOBBYACCOUNTLOGIC_H
#include "ee/wonapi/WONLobbyLogic.h"
#include "ee/wonapi/WONLobbyDialogs.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "rust/ee_won.h"
namespace WONAPI {
class ComponentEvent;
class HTTPSession;
// Both are Rust objects since R6 WONLobby round 8 (rust/crates/ee_won/src/logics.rs): the shells keep what
// the host's ops reach (the dialogs, the HTTP session, the firewall test's socket, op and address, the advert's link).
class AccountLogic : public LobbyLogic {
public:
    AccountLogic();
    virtual ~AccountLogic();
    virtual void InitComponent(ComponentConfig* inConfig);
    virtual void HandleControlEvent(int inControlId);
    virtual void HandleComponentEvent(ComponentEvent* inEvent);
    EeWonLogic* mRust; // mLastUserName (0x40), mNeedTOU (0x50)
    SmartPtr<CreditsDialog> mCreditsDialog; // 0x54
    SmartPtr<TOUDialog> mTOUDialog; // 0x58
};
class InitLogic : public LobbyLogic {
public:
    InitLogic();
    virtual ~InitLogic();
    virtual void KillHook() { this->Call(EeWonLogicCall::InitKillHook); }
    virtual void InitComponent(ComponentConfig* inConfig);
    virtual void HandleControlEvent(int inControlId);
    virtual void HandleComponentEvent(ComponentEvent* inEvent) { this->Call(EeWonLogicCall::InitEvent, inEvent->mType); }
    void Init() { this->Call(EeWonLogicCall::InitInit); }
    void KillFirewallTest() { this->Call(EeWonLogicCall::InitKillFirewall); }
    // the completions (Rust's)
    void DNSCompletion(AsyncOp* inOp) { this->Call(EeWonLogicCall::InitDns, (intptr_t)inOp); }
    void HTTPDocumentCompletion(AsyncOp* inOp) { this->Call(EeWonLogicCall::InitDocument, (intptr_t)inOp); }
    void CrossPromotionImageCompletion(AsyncOp* inOp) { this->Call(EeWonLogicCall::InitPromotionImage, (intptr_t)inOp); }
    void DirectoryCompletion(AsyncOp* inOp) { this->Call(EeWonLogicCall::InitDirectory, (intptr_t)inOp); }
    void VersionCompletion(AsyncOp* inOp) { this->Call(EeWonLogicCall::InitVersion, (intptr_t)inOp); }
    void AcceptCompletion(AsyncOp* inOp) { this->Call(EeWonLogicCall::InitAccept, (intptr_t)inOp); }
    void FirewallCompletion(AsyncOp* inOp) { this->Call(EeWonLogicCall::InitFirewall, (intptr_t)inOp); }
    intptr_t Call(EeWonLogicCall inCall, intptr_t inA = 0) { return ee_won_logic_call(this->mRust, inCall, inA, 0, 0); }
    EeWonLogic* mRust; // mErrors (0x88), mIgnoredErrors (0x8c)
    SmartPtr<HTTPSession> mHTTPSession; // 0x40
    std::string mCrossPromotion; // 0x44, narrow string (assigned at 0x7ddaf1)
    SmartPtr<AcceptOp> mAcceptOp; // 0x54
    SmartPtr<AsyncSocket> mFirewallSocket; // 0x58
    IPAddr mFirewallAddress; // 0x5c
    SmartPtr<ChooseNetworkAdapterDialog> mNetworkAdapterDialog; // 0x84
};
}
#endif
