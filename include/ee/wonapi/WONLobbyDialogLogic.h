// DialogLogic: runs the lobby's modal dialogs (status, error, OK/Cancel, Yes/No) on a dialog stack
#ifndef EE_WONAPI_WONLOBBYDIALOGLOGIC_H
#define EE_WONAPI_WONLOBBYDIALOGLOGIC_H
#include "ee/wonapi/WONLobbyLogic.h"
#include "ee/wonapi/WONLobbyDialogs.h"
#include "rust/won_lobby/dlglogic.h"
// The class is one Rust object (R6 WONLobby round 2: ee_won dlglogic.rs holds the dialog stack, the two dialogs,
// the status operation and runs every method) and, since R6 WON round 36, the RustLogic of kind Dialog
// (ee/wonapi/WONLobbyLogic.h): its methods are the shell's Call(EeWonDlgLogicCall, ..) with the texts' addresses.
// The host is in DialogLogic.cpp.
#endif
