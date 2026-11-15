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
#include "rust/ee_won.h"

using namespace WONAPI;

// 0x8d0ec8: the strings by id (EELobbyResource_GetString), the original's 73 globals in their order
static const size_t kEELobbyStringCount = 73;
static GUIString sEELobbyStrings[kEELobbyStringCount];

static void EELobbyResource_Load(ResourceConfigTable* inTable);

// FUNCTION: EXE 0x69b1ed
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

// FUNCTION: EXE 0x69b26c
// Returns the game lobby string with the given table index (epochs, game options, dialog texts)
GUIString& EELobbyResource_GetString(int inID)
{
    return sEELobbyStrings[inID];
}

// the string of a global's name (its row in ee_won's table); the program stops for a name the table lacks
GUIString& EELobbyResource_Named(const char* inName)
{
    intptr_t anIndex = ee_won_eeresource_find(inName);
    if (anIndex < 0 || (size_t)anIndex >= kEELobbyStringCount) {
        fprintf(stderr, "EELobbyResource: no string %s\n", inName);
        abort();
    }
    return sEELobbyStrings[anIndex];
}

// the string of row `inIndex` of ee_won's table (the same order); the program stops when the two lists disagree
static GUIString* EELobbyResourceSlotFor(size_t inIndex)
{
    if (ee_won_eeresource_count() != kEELobbyStringCount) {
        fprintf(stderr, "EELobbyResource: %u rows for %u strings\n", (unsigned)ee_won_eeresource_count(),
                (unsigned)kEELobbyStringCount);
        abort();
    }
    return &sEELobbyStrings[inIndex];
}

// FUNCTION: EXE 0x69b278
// Loads the game options, player options, connect, file dialog, game status and lobby texts from resources (ee_won's
// rows in order: a section is looked up where it changes, as the original's statements did)
static void EELobbyResource_Load(ResourceConfigTable* inTable)
{
    ResourceConfig* aTable = NULL;
    size_t aCount = ee_won_eeresource_count();
    for (size_t i = 0; i < aCount; i++) {
        EeWonEeResource aRow;
        if (!ee_won_eeresource_row(i, &aRow))
            break;
        GUIString* aGlobal = EELobbyResourceSlotFor(i);
        if (aRow.new_section)
            aTable = inTable->SafeGetConfig(aRow.section);
        *aGlobal = aTable->SafeGetString(aRow.key);
    }
}

// FUNCTION: EXE 0x69c961
// Resets every game lobby string to the empty string (ee_won's rows in order)
void EELobbyResource_Destroy()
{
    size_t aCount = ee_won_eeresource_count();
    for (size_t i = 0; i < aCount; i++)
        *EELobbyResourceSlotFor(i) = GetEmptyGUIStringData();
}

// TEMPLATE: EXE 0x69cdf2
// (SmartPtr<GUIStringData>::operator=(const SmartPtr&), refused inside GUIString::operator= at the first 48 sites of
// EELobbyResource_Load)
