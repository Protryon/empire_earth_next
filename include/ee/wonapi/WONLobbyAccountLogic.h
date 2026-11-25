// AccountLogic (login, account creation, lost password/user name, terms of use) and InitLogic (lobby start-up:
// DNS test, HTTP documents, directory query, version check, firewall test); 0x5c and 0x90 bytes in the original

#ifndef EE_WONAPI_WONLOBBYACCOUNTLOGIC_H
#define EE_WONAPI_WONLOBBYACCOUNTLOGIC_H
#include "ee/wonapi/WONLobbyLogic.h"
#include "ee/wonapi/WONLobbyDialogs.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "rust/won_lobby/logics.h"
namespace WONAPI {
// Both are Rust objects since R6 WONLobby round 8 (rust/crates/ee_won_lobby/src/logics.rs) and, since R6 WON round
// 36, RustLogics of the kinds Account and Init (ee/wonapi/WONLobbyLogic.h): what the classes held for their hosts'
// ops is in the shell's slots, named here. The Init kind's cross-promotion link is the shell's mText (0x44), its
// firewall test address the shell's mAddr (0x5c).
enum { kAccountCredits, kAccountTOU }; // mDialog: mCreditsDialog (0x54), mTOUDialog (0x58)
enum { kInitAdapter }; // mDialog: mNetworkAdapterDialog (0x84)
enum { kInitSession, kInitAcceptOp, kInitFirewallSocket }; // mSlot: mHTTPSession (0x40), mAcceptOp (0x54), mFirewallSocket (0x58)
}
#endif
