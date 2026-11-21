// ee/wonapi/WONStatus.h. The WON SDK's WONStatus codes (WONStatus itself is `typedef long`, the type the mangled
// names use) are Rust's constants since R6 WONLobby round 30 (rust/crates/ee_won_net/src/status_codes.rs; the names
// as WONStatusToString 0x69deb0 spells them, checked against status.rs's table of the original's names).
#ifndef EE_WONSTATUS_H
#define EE_WONSTATUS_H
#include "rust/won_net/status_codes.h"
#endif // EE_WONSTATUS_H
