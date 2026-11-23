// C:\EEX\Empire Earth\WONAPI\WONLobby\GameCode\EmpireEarth\EELobbyResource.cpp (file name inferred: no __FILE__;
// it sorts after EELobbyPlayerOptionsCtrl.cpp and before the sound classes). The game lobby's text resources, in the
// shape of WONAPI's generated resource units: 73 GUIString globals (init chains 0x69a43d..0x69b1ed, definition order
// = table order), the loader registration, the section loader and the reset: 0x69a43d..0x69ce06 (the
// GUIString::operator= COMDAT closes it). Every name here is a provisional name. R6 WONLobby round 5: each string's
// section and key are ee_won's eeresource.rs rows (in this table's order); the loader and the reset run over them.
// R6 WONLobby round 6: the 73 globals are one array of the rows' strings (row i the original's table entry i); the
// lobby's code names a string by its global's name through its row (EE_LOBBY_STRING, EELobbyGameCode.h). Their init
// chains (0x69a43d..0x69b1ed: four SYNTHETIC functions each) went with them.
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <list>

#include "lle/LowLevelEngine.h"

#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "rust/won_lobby/eeresource.h"
#include "ee/wonapi/WONLobbyGuard.h"

using namespace WONAPI;

// 0x8d0ec8: the strings by id (EELobbyResource_GetString), the original's 73 globals in their order
static const size_t kEELobbyStringCount = 73;
static GUIString sEELobbyStrings[kEELobbyStringCount];

static void EELobbyResource_Load(ResourceConfigTable* inTable);

// Registers the game lobby's string loader with a resource table, creating the table when none is given
// Not identical: the original stores the returned smart pointer copy only once, after the NULL test
ResourceConfigTablePtr EELobbyResource_Init(ResourceConfigTable* inCollection)
{
    ResourceConfigTablePtr theCollection = inCollection;
    if (inCollection == NULL) {
        inCollection = new ResourceConfigTable;
        theCollection = inCollection;
    }
    inCollection->RegisterGlueResourceFunc(EELobbyResource_Load);
    return theCollection;
}

// Returns the game lobby string with the given table index (epochs, game options, dialog texts)
GUIString& EELobbyResource_GetString(int inID)
{
    return sEELobbyStrings[inID];
}

// the string of a global's name (its row in ee_won's table); the program stops for a name the table lacks (the
// look-up is ee_won's)
GUIString& EELobbyResource_Named(const char* inName)
{
    return sEELobbyStrings[ee_won_eeresource_index(inName, kEELobbyStringCount)];
}

// the loader's and the reset's statements (R6 WON round 34: their loops are ee_won's eeresource.rs; a read that
// throws is kept and the loader rethrows it, never through a Rust frame)
static intptr_t EELobbyResourceHost(EeWonEeResOp inOp, void* inTable, intptr_t a, intptr_t b, intptr_t c)
{
    switch (inOp) {
    case EeWonEeResOp::Config: return WON_GUARDED(static_cast<ResourceConfigTable*>(inTable)->SafeGetConfig(reinterpret_cast<const char*>(a)));
    case EeWonEeResOp::Read: return WON_GUARDED_V(sEELobbyStrings[a] = reinterpret_cast<ResourceConfig*>(b)->SafeGetString(reinterpret_cast<const char*>(c)));
    case EeWonEeResOp::Erase: sEELobbyStrings[a] = GetEmptyGUIStringData(); return 0;
    }
    return 0;
}

// Loads the game options, player options, connect, file dialog, game status and lobby texts from resources (ee_won's
// rows in order: a section is looked up where it changes, as the original's statements did)
static void EELobbyResource_Load(ResourceConfigTable* inTable)
{
    WonRethrow(ee_won_eeresource_load(inTable, kEELobbyStringCount, EELobbyResourceHost));
}

// Resets every game lobby string to the empty string (ee_won's rows in order)
void EELobbyResource_Destroy()
{
    ee_won_eeresource_destroy(kEELobbyStringCount, EELobbyResourceHost);
}
