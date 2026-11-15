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
    AccountLogic(); // 0x7db150
    virtual ~AccountLogic(); // 0x7db1d0
    virtual void InitComponent(ComponentConfig* inConfig); // 0x7db2a0
    virtual void HandleControlEvent(int inControlId); // 0x7dc2e0
    virtual void HandleComponentEvent(ComponentEvent* inEvent); // 0x7dc340
    EeWonLogic* mRust; // mLastUserName (0x40), mNeedTOU (0x50)
    SmartPtr<CreditsDialog> mCreditsDialog; // 0x54
    SmartPtr<TOUDialog> mTOUDialog; // 0x58
};
class InitLogic : public LobbyLogic {
public:
    InitLogic(); // 0x7dd100
    virtual ~InitLogic(); // 0x7dd200
    virtual void KillHook() { this->Call(EeWonLogicCall::InitKillHook); } // 0x7dfb00
    virtual void InitComponent(ComponentConfig* inConfig); // 0x7dd340
    virtual void HandleControlEvent(int inControlId); // 0x7dfb20
    virtual void HandleComponentEvent(ComponentEvent* inEvent) { this->Call(EeWonLogicCall::InitEvent, inEvent->mType); } // 0x7dfb50
    void Init() { this->Call(EeWonLogicCall::InitInit); } // 0x7dd630
    void KillFirewallTest() { this->Call(EeWonLogicCall::InitKillFirewall); } // 0x7dfa80
    // the completions (Rust's)
    void DNSCompletion(AsyncOp* inOp) { this->Call(EeWonLogicCall::InitDns, (intptr_t)inOp); } // 0x7dd810
    void HTTPDocumentCompletion(AsyncOp* inOp) { this->Call(EeWonLogicCall::InitDocument, (intptr_t)inOp); } // 0x7de280
    void CrossPromotionImageCompletion(AsyncOp* inOp) { this->Call(EeWonLogicCall::InitPromotionImage, (intptr_t)inOp); } // 0x7dda10
    void DirectoryCompletion(AsyncOp* inOp) { this->Call(EeWonLogicCall::InitDirectory, (intptr_t)inOp); } // 0x7de4c0
    void VersionCompletion(AsyncOp* inOp) { this->Call(EeWonLogicCall::InitVersion, (intptr_t)inOp); } // 0x7de8f0
    void AcceptCompletion(AsyncOp* inOp) { this->Call(EeWonLogicCall::InitAccept, (intptr_t)inOp); } // 0x7deb50
    void FirewallCompletion(AsyncOp* inOp) { this->Call(EeWonLogicCall::InitFirewall, (intptr_t)inOp); } // 0x7ded20
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
