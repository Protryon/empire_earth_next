// C:\EEX\Empire Earth\Actions\EEAWonNet.cpp (file name inferred: no __FILE__; RTTI class EEAWonNet, vtable
// 0x43914c). The WON lobby's UI-thread action: 0x52f3..0x5d47.
// R6 WON lobby and network round 37: the three constructors and Virtual4 (the room list's and game room's forms
// fed, with its statics) are Rust (ee_won_lobby wonaction.rs); this unit is the action's host, one statement a
// case (ee/EEAWonNetHost.inl).
#include <windows.h>
#include <mbstring.h>
#include <algorithm>
#include <string>
#include <list>

#include "lle/LowLevelEngine.h"
#include "lle/GESurface.h"
#include "lle/GEScreen.h"
#include "lle/GERhombus.h"
#include "lle/FSArchive.h"
#include "lle/UIForm.h"
#include "lle/UITextLabel.h"
#include "lle/UIListBox.h"
#include "lle/GEText.h"
#include "lle/UResourceFile.h"

#include "ee/EmpireEarth.h"
#include "ee/EEUIManager.h"
#include "ee/EEUICWonNetGamesBox.h"
#include "ee/EEUICWonNetRoomsBox.h"
#include "ee/EEAWonNet.h"

#include "ee/EEAWonNetHost.inl"

// Applies a WON lobby update on the UI thread: room and user lists, games box lines and selection, chat text
void EEAWonNet::Virtual4() { ee_won_action_run(this->mRust, WonActionHost); }
