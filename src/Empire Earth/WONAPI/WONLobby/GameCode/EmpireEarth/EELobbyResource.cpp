// C:\EEX\Empire Earth\WONAPI\WONLobby\GameCode\EmpireEarth\EELobbyResource.cpp (file name inferred: no __FILE__;
// it sorts after EELobbyPlayerOptionsCtrl.cpp and before the sound classes). The game lobby's text resources, in the
// shape of WONAPI's generated resource units: 73 GUIString globals (init chains 0x69a43d..0x69b1ed, definition order
// = table order), the loader registration, the section loader and the reset: 0x69a43d..0x69ce06 (the
// GUIString::operator= COMDAT closes it). Every name here is a provisional name.
#include <windows.h>
#include <string>
#include <list>

#include "lle/LowLevelEngine.h"

#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"

using namespace WONAPI;

GUIString gEELobbyGameOptionsCtrl_Epoch1;
GUIString gEELobbyGameOptionsCtrl_Epoch2;
GUIString gEELobbyGameOptionsCtrl_Epoch3;
GUIString gEELobbyGameOptionsCtrl_Epoch4;
GUIString gEELobbyGameOptionsCtrl_Epoch5;
GUIString gEELobbyGameOptionsCtrl_Epoch6;
GUIString gEELobbyGameOptionsCtrl_Epoch7;
GUIString gEELobbyGameOptionsCtrl_Epoch8;
GUIString gEELobbyGameOptionsCtrl_Epoch9;
GUIString gEELobbyGameOptionsCtrl_Epoch10;
GUIString gEELobbyGameOptionsCtrl_Epoch11;
GUIString gEELobbyGameOptionsCtrl_Epoch12;
GUIString gEELobbyGameOptionsCtrl_Epoch13;
GUIString gEELobbyGameOptionsCtrl_Epoch14;
GUIString gEELobbyGameOptionsCtrl_Epoch15;
GUIString gEELobbyGameOptionsCtrl_Epoch16;
GUIString gEELobbyGameOptionsCtrl_GameType1;
GUIString gEELobbyGameOptionsCtrl_GameType2;
GUIString gEELobbyGameOptionsCtrl_GameType3;
GUIString gEELobbyGameOptionsCtrl_MapSize1;
GUIString gEELobbyGameOptionsCtrl_MapSize2;
GUIString gEELobbyGameOptionsCtrl_MapSize3;
GUIString gEELobbyGameOptionsCtrl_MapSize4;
GUIString gEELobbyGameOptionsCtrl_MapSize5;
GUIString gEELobbyGameOptionsCtrl_MapSize6;
GUIString gEELobbyGameOptionsCtrl_Resources1;
GUIString gEELobbyGameOptionsCtrl_Resources2;
GUIString gEELobbyGameOptionsCtrl_Resources3;
GUIString gEELobbyGameOptionsCtrl_Resources4;
GUIString gEELobbyGameOptionsCtrl_Resources5;
GUIString gEELobbyGameOptionsCtrl_GameVariant1;
GUIString gEELobbyGameOptionsCtrl_GameVariant2;
GUIString gEELobbyGameOptionsCtrl_Difficulty1;
GUIString gEELobbyGameOptionsCtrl_Difficulty2;
GUIString gEELobbyGameOptionsCtrl_Difficulty3;
GUIString gEELobbyGameOptionsCtrl_GameSpeed1;
GUIString gEELobbyGameOptionsCtrl_GameSpeed2;
GUIString gEELobbyGameOptionsCtrl_GameSpeed3;
GUIString gEELobbyGameOptionsCtrl_GameSpeed4;
GUIString gEELobbyGameOptionsCtrl_OptionOn;
GUIString gEELobbyGameOptionsCtrl_OptionOff;
GUIString gEELobbyGameOptionsCtrl_WondersOff;
GUIString gEELobbyGameOptionsCtrl_GameType;
GUIString gEELobbyGameOptionsCtrl_MapSize;
GUIString gEELobbyGameOptionsCtrl_MapType;
GUIString gEELobbyGameOptionsCtrl_Resources;
GUIString gEELobbyGameOptionsCtrl_StartingEpoch;
GUIString gEELobbyGameOptionsCtrl_EndingEpoch;
GUIString gEELobbyGameOptionsCtrl_GameUnitLimit;
GUIString gEELobbyGameOptionsCtrl_GameVariant;
GUIString gEELobbyGameOptionsCtrl_DifficultyLevel;
GUIString gEELobbyGameOptionsCtrl_GameSpeed;
GUIString gEELobbyGameOptionsCtrl_RevealMap;
GUIString gEELobbyGameOptionsCtrl_UseCustomCivs;
GUIString gEELobbyGameOptionsCtrl_LockTeams;
GUIString gEELobbyGameOptionsCtrl_LockSpeed;
GUIString gEELobbyGameOptionsCtrl_VictoryAllowed;
GUIString gEELobbyGameOptionsCtrl_CampaignInformation;
GUIString gEELobbyGameOptionsCtrl_NumberOfWonders;
GUIString gEELobbyPlayerOptionsCtrl_TransferPercent;
GUIString gEELobbyConnectDialog_LeaveGame;
GUIString gEELobbyConnectDialog_AbortLaunch;
GUIString gEELobbyFileDialog_DeleteTitle;
GUIString gEELobbyFileDialog_DeleteDetails;
GUIString gEELobbyGameStatus_NotInSavedGame;
GUIString gEELobbyGameStatus_InvalidVersion;
GUIString gEELobbyGameStatus_CorruptBinaries;
GUIString gEELobbyInterface_LoadGameError;
GUIString gEELobbyInterface_ErrorReadingFile;
GUIString gEELobbyInterface_PlayerNotInGame;
GUIString gEELobbyInterface_Players;
GUIString gEELobbyGame_StartGameError;
GUIString gEELobbyGame_EveryoneOnSameTeam;

// 0x8d0ec8: the strings by id (EELobbyResource_GetString)
static GUIString* sEELobbyStrings[] = {
    &gEELobbyGameOptionsCtrl_Epoch1,
    &gEELobbyGameOptionsCtrl_Epoch2,
    &gEELobbyGameOptionsCtrl_Epoch3,
    &gEELobbyGameOptionsCtrl_Epoch4,
    &gEELobbyGameOptionsCtrl_Epoch5,
    &gEELobbyGameOptionsCtrl_Epoch6,
    &gEELobbyGameOptionsCtrl_Epoch7,
    &gEELobbyGameOptionsCtrl_Epoch8,
    &gEELobbyGameOptionsCtrl_Epoch9,
    &gEELobbyGameOptionsCtrl_Epoch10,
    &gEELobbyGameOptionsCtrl_Epoch11,
    &gEELobbyGameOptionsCtrl_Epoch12,
    &gEELobbyGameOptionsCtrl_Epoch13,
    &gEELobbyGameOptionsCtrl_Epoch14,
    &gEELobbyGameOptionsCtrl_Epoch15,
    &gEELobbyGameOptionsCtrl_Epoch16,
    &gEELobbyGameOptionsCtrl_GameType1,
    &gEELobbyGameOptionsCtrl_GameType2,
    &gEELobbyGameOptionsCtrl_GameType3,
    &gEELobbyGameOptionsCtrl_MapSize1,
    &gEELobbyGameOptionsCtrl_MapSize2,
    &gEELobbyGameOptionsCtrl_MapSize3,
    &gEELobbyGameOptionsCtrl_MapSize4,
    &gEELobbyGameOptionsCtrl_MapSize5,
    &gEELobbyGameOptionsCtrl_MapSize6,
    &gEELobbyGameOptionsCtrl_Resources1,
    &gEELobbyGameOptionsCtrl_Resources2,
    &gEELobbyGameOptionsCtrl_Resources3,
    &gEELobbyGameOptionsCtrl_Resources4,
    &gEELobbyGameOptionsCtrl_Resources5,
    &gEELobbyGameOptionsCtrl_GameVariant1,
    &gEELobbyGameOptionsCtrl_GameVariant2,
    &gEELobbyGameOptionsCtrl_Difficulty1,
    &gEELobbyGameOptionsCtrl_Difficulty2,
    &gEELobbyGameOptionsCtrl_Difficulty3,
    &gEELobbyGameOptionsCtrl_GameSpeed1,
    &gEELobbyGameOptionsCtrl_GameSpeed2,
    &gEELobbyGameOptionsCtrl_GameSpeed3,
    &gEELobbyGameOptionsCtrl_GameSpeed4,
    &gEELobbyGameOptionsCtrl_OptionOn,
    &gEELobbyGameOptionsCtrl_OptionOff,
    &gEELobbyGameOptionsCtrl_WondersOff,
    &gEELobbyGameOptionsCtrl_GameType,
    &gEELobbyGameOptionsCtrl_MapSize,
    &gEELobbyGameOptionsCtrl_MapType,
    &gEELobbyGameOptionsCtrl_Resources,
    &gEELobbyGameOptionsCtrl_StartingEpoch,
    &gEELobbyGameOptionsCtrl_EndingEpoch,
    &gEELobbyGameOptionsCtrl_GameUnitLimit,
    &gEELobbyGameOptionsCtrl_GameVariant,
    &gEELobbyGameOptionsCtrl_DifficultyLevel,
    &gEELobbyGameOptionsCtrl_GameSpeed,
    &gEELobbyGameOptionsCtrl_RevealMap,
    &gEELobbyGameOptionsCtrl_UseCustomCivs,
    &gEELobbyGameOptionsCtrl_LockTeams,
    &gEELobbyGameOptionsCtrl_LockSpeed,
    &gEELobbyGameOptionsCtrl_VictoryAllowed,
    &gEELobbyGameOptionsCtrl_CampaignInformation,
    &gEELobbyGameOptionsCtrl_NumberOfWonders,
    &gEELobbyPlayerOptionsCtrl_TransferPercent,
    &gEELobbyConnectDialog_LeaveGame,
    &gEELobbyConnectDialog_AbortLaunch,
    &gEELobbyFileDialog_DeleteTitle,
    &gEELobbyFileDialog_DeleteDetails,
    &gEELobbyGameStatus_NotInSavedGame,
    &gEELobbyGameStatus_InvalidVersion,
    &gEELobbyGameStatus_CorruptBinaries,
    &gEELobbyInterface_LoadGameError,
    &gEELobbyInterface_ErrorReadingFile,
    &gEELobbyInterface_PlayerNotInGame,
    &gEELobbyInterface_Players,
    &gEELobbyGame_StartGameError,
    &gEELobbyGame_EveryoneOnSameTeam,
};

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
    return *sEELobbyStrings[inID];
}

// FUNCTION: EXE 0x69b278
// Loads the game options, player options, connect, file dialog, game status and lobby texts from resources
// GUIString::operator='s inner SmartPtr::operator=(const SmartPtr&) (ee/EEWonNet.h) is called out of line
// (0x69cdf2) at the first 48 assignments and expanded inline from the 49th on
static void EELobbyResource_Load(ResourceConfigTable* inTable)
{
    ResourceConfig* aTable;
    aTable = inTable->SafeGetConfig("GameOptionsCtrl");
    gEELobbyGameOptionsCtrl_Epoch1 = aTable->SafeGetString("Epoch1");
    gEELobbyGameOptionsCtrl_Epoch2 = aTable->SafeGetString("Epoch2");
    gEELobbyGameOptionsCtrl_Epoch3 = aTable->SafeGetString("Epoch3");
    gEELobbyGameOptionsCtrl_Epoch4 = aTable->SafeGetString("Epoch4");
    gEELobbyGameOptionsCtrl_Epoch5 = aTable->SafeGetString("Epoch5");
    gEELobbyGameOptionsCtrl_Epoch6 = aTable->SafeGetString("Epoch6");
    gEELobbyGameOptionsCtrl_Epoch7 = aTable->SafeGetString("Epoch7");
    gEELobbyGameOptionsCtrl_Epoch8 = aTable->SafeGetString("Epoch8");
    gEELobbyGameOptionsCtrl_Epoch9 = aTable->SafeGetString("Epoch9");
    gEELobbyGameOptionsCtrl_Epoch10 = aTable->SafeGetString("Epoch10");
    gEELobbyGameOptionsCtrl_Epoch11 = aTable->SafeGetString("Epoch11");
    gEELobbyGameOptionsCtrl_Epoch12 = aTable->SafeGetString("Epoch12");
    gEELobbyGameOptionsCtrl_Epoch13 = aTable->SafeGetString("Epoch13");
    gEELobbyGameOptionsCtrl_Epoch14 = aTable->SafeGetString("Epoch14");
    gEELobbyGameOptionsCtrl_Epoch15 = aTable->SafeGetString("Epoch15");
    gEELobbyGameOptionsCtrl_Epoch16 = aTable->SafeGetString("Epoch16");
    gEELobbyGameOptionsCtrl_GameType1 = aTable->SafeGetString("GameType1");
    gEELobbyGameOptionsCtrl_GameType2 = aTable->SafeGetString("GameType2");
    gEELobbyGameOptionsCtrl_GameType3 = aTable->SafeGetString("GameType3");
    gEELobbyGameOptionsCtrl_MapSize1 = aTable->SafeGetString("MapSize1");
    gEELobbyGameOptionsCtrl_MapSize2 = aTable->SafeGetString("MapSize2");
    gEELobbyGameOptionsCtrl_MapSize3 = aTable->SafeGetString("MapSize3");
    gEELobbyGameOptionsCtrl_MapSize4 = aTable->SafeGetString("MapSize4");
    gEELobbyGameOptionsCtrl_MapSize5 = aTable->SafeGetString("MapSize5");
    gEELobbyGameOptionsCtrl_MapSize6 = aTable->SafeGetString("MapSize6");
    gEELobbyGameOptionsCtrl_Resources1 = aTable->SafeGetString("Resources1");
    gEELobbyGameOptionsCtrl_Resources2 = aTable->SafeGetString("Resources2");
    gEELobbyGameOptionsCtrl_Resources3 = aTable->SafeGetString("Resources3");
    gEELobbyGameOptionsCtrl_Resources4 = aTable->SafeGetString("Resources4");
    gEELobbyGameOptionsCtrl_Resources5 = aTable->SafeGetString("Resources5");
    gEELobbyGameOptionsCtrl_GameVariant1 = aTable->SafeGetString("GameVariant1");
    gEELobbyGameOptionsCtrl_GameVariant2 = aTable->SafeGetString("GameVariant2");
    gEELobbyGameOptionsCtrl_Difficulty1 = aTable->SafeGetString("Difficulty1");
    gEELobbyGameOptionsCtrl_Difficulty2 = aTable->SafeGetString("Difficulty2");
    gEELobbyGameOptionsCtrl_Difficulty3 = aTable->SafeGetString("Difficulty3");
    gEELobbyGameOptionsCtrl_GameSpeed1 = aTable->SafeGetString("GameSpeed1");
    gEELobbyGameOptionsCtrl_GameSpeed2 = aTable->SafeGetString("GameSpeed2");
    gEELobbyGameOptionsCtrl_GameSpeed3 = aTable->SafeGetString("GameSpeed3");
    gEELobbyGameOptionsCtrl_GameSpeed4 = aTable->SafeGetString("GameSpeed4");
    gEELobbyGameOptionsCtrl_OptionOn = aTable->SafeGetString("OptionOn");
    gEELobbyGameOptionsCtrl_OptionOff = aTable->SafeGetString("OptionOff");
    gEELobbyGameOptionsCtrl_WondersOff = aTable->SafeGetString("WondersOff");
    gEELobbyGameOptionsCtrl_GameType = aTable->SafeGetString("GameType");
    gEELobbyGameOptionsCtrl_MapSize = aTable->SafeGetString("MapSize");
    gEELobbyGameOptionsCtrl_MapType = aTable->SafeGetString("MapType");
    gEELobbyGameOptionsCtrl_Resources = aTable->SafeGetString("Resources");
    gEELobbyGameOptionsCtrl_StartingEpoch = aTable->SafeGetString("StartingEpoch");
    gEELobbyGameOptionsCtrl_EndingEpoch = aTable->SafeGetString("EndingEpoch");
    gEELobbyGameOptionsCtrl_GameUnitLimit = aTable->SafeGetString("GameUnitLimit");
    gEELobbyGameOptionsCtrl_GameVariant = aTable->SafeGetString("GameVariant");
    gEELobbyGameOptionsCtrl_DifficultyLevel = aTable->SafeGetString("DifficultyLevel");
    gEELobbyGameOptionsCtrl_GameSpeed = aTable->SafeGetString("GameSpeed");
    gEELobbyGameOptionsCtrl_RevealMap = aTable->SafeGetString("RevealMap");
    gEELobbyGameOptionsCtrl_UseCustomCivs = aTable->SafeGetString("UseCustomCivs");
    gEELobbyGameOptionsCtrl_LockTeams = aTable->SafeGetString("LockTeams");
    gEELobbyGameOptionsCtrl_LockSpeed = aTable->SafeGetString("LockSpeed");
    gEELobbyGameOptionsCtrl_VictoryAllowed = aTable->SafeGetString("VictoryAllowed");
    gEELobbyGameOptionsCtrl_CampaignInformation = aTable->SafeGetString("CampaignInformation");
    gEELobbyGameOptionsCtrl_NumberOfWonders = aTable->SafeGetString("NumberOfWonders");

    aTable = inTable->SafeGetConfig("PlayerOptionsCtrl");
    gEELobbyPlayerOptionsCtrl_TransferPercent = aTable->SafeGetString("TransferPercent");

    aTable = inTable->SafeGetConfig("EELobbyConnectDialog");
    gEELobbyConnectDialog_LeaveGame = aTable->SafeGetString("LeaveGame");
    gEELobbyConnectDialog_AbortLaunch = aTable->SafeGetString("AbortLaunch");

    aTable = inTable->SafeGetConfig("EELobbyFileDialog");
    gEELobbyFileDialog_DeleteTitle = aTable->SafeGetString("DeleteTitle");
    gEELobbyFileDialog_DeleteDetails = aTable->SafeGetString("DeleteDetails");

    aTable = inTable->SafeGetConfig("EELobbyGameStatus");
    gEELobbyGameStatus_NotInSavedGame = aTable->SafeGetString("NotInSavedGame");
    gEELobbyGameStatus_InvalidVersion = aTable->SafeGetString("InvalidVersion");
    gEELobbyGameStatus_CorruptBinaries = aTable->SafeGetString("CorruptBinaries");

    aTable = inTable->SafeGetConfig("EELobbyInterface");
    gEELobbyInterface_LoadGameError = aTable->SafeGetString("LoadGameError");
    gEELobbyInterface_ErrorReadingFile = aTable->SafeGetString("ErrorReadingFile");
    gEELobbyInterface_PlayerNotInGame = aTable->SafeGetString("PlayerNotInGame");
    gEELobbyInterface_Players = aTable->SafeGetString("Players");

    aTable = inTable->SafeGetConfig("EELobbyGame");
    gEELobbyGame_StartGameError = aTable->SafeGetString("StartGameError");
    gEELobbyGame_EveryoneOnSameTeam = aTable->SafeGetString("EveryoneOnSameTeam");
}

// FUNCTION: EXE 0x69c961
// Resets every game lobby string to the empty string
void EELobbyResource_Destroy()
{
    gEELobbyGameOptionsCtrl_Epoch1 = GetEmptyGUIStringData();
    gEELobbyGameOptionsCtrl_Epoch2 = GetEmptyGUIStringData();
    gEELobbyGameOptionsCtrl_Epoch3 = GetEmptyGUIStringData();
    gEELobbyGameOptionsCtrl_Epoch4 = GetEmptyGUIStringData();
    gEELobbyGameOptionsCtrl_Epoch5 = GetEmptyGUIStringData();
    gEELobbyGameOptionsCtrl_Epoch6 = GetEmptyGUIStringData();
    gEELobbyGameOptionsCtrl_Epoch7 = GetEmptyGUIStringData();
    gEELobbyGameOptionsCtrl_Epoch8 = GetEmptyGUIStringData();
    gEELobbyGameOptionsCtrl_Epoch9 = GetEmptyGUIStringData();
    gEELobbyGameOptionsCtrl_Epoch10 = GetEmptyGUIStringData();
    gEELobbyGameOptionsCtrl_Epoch11 = GetEmptyGUIStringData();
    gEELobbyGameOptionsCtrl_Epoch12 = GetEmptyGUIStringData();
    gEELobbyGameOptionsCtrl_Epoch13 = GetEmptyGUIStringData();
    gEELobbyGameOptionsCtrl_Epoch14 = GetEmptyGUIStringData();
    gEELobbyGameOptionsCtrl_Epoch15 = GetEmptyGUIStringData();
    gEELobbyGameOptionsCtrl_Epoch16 = GetEmptyGUIStringData();
    gEELobbyGameOptionsCtrl_GameType1 = GetEmptyGUIStringData();
    gEELobbyGameOptionsCtrl_GameType2 = GetEmptyGUIStringData();
    gEELobbyGameOptionsCtrl_GameType3 = GetEmptyGUIStringData();
    gEELobbyGameOptionsCtrl_MapSize1 = GetEmptyGUIStringData();
    gEELobbyGameOptionsCtrl_MapSize2 = GetEmptyGUIStringData();
    gEELobbyGameOptionsCtrl_MapSize3 = GetEmptyGUIStringData();
    gEELobbyGameOptionsCtrl_MapSize4 = GetEmptyGUIStringData();
    gEELobbyGameOptionsCtrl_MapSize5 = GetEmptyGUIStringData();
    gEELobbyGameOptionsCtrl_MapSize6 = GetEmptyGUIStringData();
    gEELobbyGameOptionsCtrl_Resources1 = GetEmptyGUIStringData();
    gEELobbyGameOptionsCtrl_Resources2 = GetEmptyGUIStringData();
    gEELobbyGameOptionsCtrl_Resources3 = GetEmptyGUIStringData();
    gEELobbyGameOptionsCtrl_Resources4 = GetEmptyGUIStringData();
    gEELobbyGameOptionsCtrl_Resources5 = GetEmptyGUIStringData();
    gEELobbyGameOptionsCtrl_GameVariant1 = GetEmptyGUIStringData();
    gEELobbyGameOptionsCtrl_GameVariant2 = GetEmptyGUIStringData();
    gEELobbyGameOptionsCtrl_Difficulty1 = GetEmptyGUIStringData();
    gEELobbyGameOptionsCtrl_Difficulty2 = GetEmptyGUIStringData();
    gEELobbyGameOptionsCtrl_Difficulty3 = GetEmptyGUIStringData();
    gEELobbyGameOptionsCtrl_GameSpeed1 = GetEmptyGUIStringData();
    gEELobbyGameOptionsCtrl_GameSpeed2 = GetEmptyGUIStringData();
    gEELobbyGameOptionsCtrl_GameSpeed3 = GetEmptyGUIStringData();
    gEELobbyGameOptionsCtrl_GameSpeed4 = GetEmptyGUIStringData();
    gEELobbyGameOptionsCtrl_OptionOn = GetEmptyGUIStringData();
    gEELobbyGameOptionsCtrl_OptionOff = GetEmptyGUIStringData();
    gEELobbyGameOptionsCtrl_WondersOff = GetEmptyGUIStringData();
    gEELobbyGameOptionsCtrl_GameType = GetEmptyGUIStringData();
    gEELobbyGameOptionsCtrl_MapSize = GetEmptyGUIStringData();
    gEELobbyGameOptionsCtrl_MapType = GetEmptyGUIStringData();
    gEELobbyGameOptionsCtrl_Resources = GetEmptyGUIStringData();
    gEELobbyGameOptionsCtrl_StartingEpoch = GetEmptyGUIStringData();
    gEELobbyGameOptionsCtrl_EndingEpoch = GetEmptyGUIStringData();
    gEELobbyGameOptionsCtrl_GameUnitLimit = GetEmptyGUIStringData();
    gEELobbyGameOptionsCtrl_GameVariant = GetEmptyGUIStringData();
    gEELobbyGameOptionsCtrl_DifficultyLevel = GetEmptyGUIStringData();
    gEELobbyGameOptionsCtrl_GameSpeed = GetEmptyGUIStringData();
    gEELobbyGameOptionsCtrl_RevealMap = GetEmptyGUIStringData();
    gEELobbyGameOptionsCtrl_UseCustomCivs = GetEmptyGUIStringData();
    gEELobbyGameOptionsCtrl_LockTeams = GetEmptyGUIStringData();
    gEELobbyGameOptionsCtrl_LockSpeed = GetEmptyGUIStringData();
    gEELobbyGameOptionsCtrl_VictoryAllowed = GetEmptyGUIStringData();
    gEELobbyGameOptionsCtrl_CampaignInformation = GetEmptyGUIStringData();
    gEELobbyGameOptionsCtrl_NumberOfWonders = GetEmptyGUIStringData();
    gEELobbyPlayerOptionsCtrl_TransferPercent = GetEmptyGUIStringData();
    gEELobbyConnectDialog_LeaveGame = GetEmptyGUIStringData();
    gEELobbyConnectDialog_AbortLaunch = GetEmptyGUIStringData();
    gEELobbyFileDialog_DeleteTitle = GetEmptyGUIStringData();
    gEELobbyFileDialog_DeleteDetails = GetEmptyGUIStringData();
    gEELobbyGameStatus_NotInSavedGame = GetEmptyGUIStringData();
    gEELobbyGameStatus_InvalidVersion = GetEmptyGUIStringData();
    gEELobbyGameStatus_CorruptBinaries = GetEmptyGUIStringData();
    gEELobbyInterface_LoadGameError = GetEmptyGUIStringData();
    gEELobbyInterface_ErrorReadingFile = GetEmptyGUIStringData();
    gEELobbyInterface_PlayerNotInGame = GetEmptyGUIStringData();
    gEELobbyInterface_Players = GetEmptyGUIStringData();
    gEELobbyGame_StartGameError = GetEmptyGUIStringData();
    gEELobbyGame_EveryoneOnSameTeam = GetEmptyGUIStringData();
}

// TEMPLATE: EXE 0x69cdf2
// (SmartPtr<GUIStringData>::operator=(const SmartPtr&), refused inside GUIString::operator= at the first 48 sites of
// EELobbyResource_Load)

// SYNTHETIC: EXE 0x69a43d
// SYNTHETIC: EXE 0x69a447
// SYNTHETIC: EXE 0x69a451
// SYNTHETIC: EXE 0x69a45d
// SYNTHETIC: EXE 0x69a46d
// SYNTHETIC: EXE 0x69a477
// SYNTHETIC: EXE 0x69a481
// SYNTHETIC: EXE 0x69a48d
// SYNTHETIC: EXE 0x69a49d
// SYNTHETIC: EXE 0x69a4a7
// SYNTHETIC: EXE 0x69a4b1
// SYNTHETIC: EXE 0x69a4bd
// SYNTHETIC: EXE 0x69a4cd
// SYNTHETIC: EXE 0x69a4d7
// SYNTHETIC: EXE 0x69a4e1
// SYNTHETIC: EXE 0x69a4ed
// SYNTHETIC: EXE 0x69a4fd
// SYNTHETIC: EXE 0x69a507
// SYNTHETIC: EXE 0x69a511
// SYNTHETIC: EXE 0x69a51d
// SYNTHETIC: EXE 0x69a52d
// SYNTHETIC: EXE 0x69a537
// SYNTHETIC: EXE 0x69a541
// SYNTHETIC: EXE 0x69a54d
// SYNTHETIC: EXE 0x69a55d
// SYNTHETIC: EXE 0x69a567
// SYNTHETIC: EXE 0x69a571
// SYNTHETIC: EXE 0x69a57d
// SYNTHETIC: EXE 0x69a58d
// SYNTHETIC: EXE 0x69a597
// SYNTHETIC: EXE 0x69a5a1
// SYNTHETIC: EXE 0x69a5ad
// SYNTHETIC: EXE 0x69a5bd
// SYNTHETIC: EXE 0x69a5c7
// SYNTHETIC: EXE 0x69a5d1
// SYNTHETIC: EXE 0x69a5dd
// SYNTHETIC: EXE 0x69a5ed
// SYNTHETIC: EXE 0x69a5f7
// SYNTHETIC: EXE 0x69a601
// SYNTHETIC: EXE 0x69a60d
// SYNTHETIC: EXE 0x69a61d
// SYNTHETIC: EXE 0x69a627
// SYNTHETIC: EXE 0x69a631
// SYNTHETIC: EXE 0x69a63d
// SYNTHETIC: EXE 0x69a64d
// SYNTHETIC: EXE 0x69a657
// SYNTHETIC: EXE 0x69a661
// SYNTHETIC: EXE 0x69a66d
// SYNTHETIC: EXE 0x69a67d
// SYNTHETIC: EXE 0x69a687
// SYNTHETIC: EXE 0x69a691
// SYNTHETIC: EXE 0x69a69d
// SYNTHETIC: EXE 0x69a6ad
// SYNTHETIC: EXE 0x69a6b7
// SYNTHETIC: EXE 0x69a6c1
// SYNTHETIC: EXE 0x69a6cd
// SYNTHETIC: EXE 0x69a6dd
// SYNTHETIC: EXE 0x69a6e7
// SYNTHETIC: EXE 0x69a6f1
// SYNTHETIC: EXE 0x69a6fd
// SYNTHETIC: EXE 0x69a70d
// SYNTHETIC: EXE 0x69a717
// SYNTHETIC: EXE 0x69a721
// SYNTHETIC: EXE 0x69a72d
// SYNTHETIC: EXE 0x69a73d
// SYNTHETIC: EXE 0x69a747
// SYNTHETIC: EXE 0x69a751
// SYNTHETIC: EXE 0x69a75d
// SYNTHETIC: EXE 0x69a76d
// SYNTHETIC: EXE 0x69a777
// SYNTHETIC: EXE 0x69a781
// SYNTHETIC: EXE 0x69a78d
// SYNTHETIC: EXE 0x69a79d
// SYNTHETIC: EXE 0x69a7a7
// SYNTHETIC: EXE 0x69a7b1
// SYNTHETIC: EXE 0x69a7bd
// SYNTHETIC: EXE 0x69a7cd
// SYNTHETIC: EXE 0x69a7d7
// SYNTHETIC: EXE 0x69a7e1
// SYNTHETIC: EXE 0x69a7ed
// SYNTHETIC: EXE 0x69a7fd
// SYNTHETIC: EXE 0x69a807
// SYNTHETIC: EXE 0x69a811
// SYNTHETIC: EXE 0x69a81d
// SYNTHETIC: EXE 0x69a82d
// SYNTHETIC: EXE 0x69a837
// SYNTHETIC: EXE 0x69a841
// SYNTHETIC: EXE 0x69a84d
// SYNTHETIC: EXE 0x69a85d
// SYNTHETIC: EXE 0x69a867
// SYNTHETIC: EXE 0x69a871
// SYNTHETIC: EXE 0x69a87d
// SYNTHETIC: EXE 0x69a88d
// SYNTHETIC: EXE 0x69a897
// SYNTHETIC: EXE 0x69a8a1
// SYNTHETIC: EXE 0x69a8ad
// SYNTHETIC: EXE 0x69a8bd
// SYNTHETIC: EXE 0x69a8c7
// SYNTHETIC: EXE 0x69a8d1
// SYNTHETIC: EXE 0x69a8dd
// SYNTHETIC: EXE 0x69a8ed
// SYNTHETIC: EXE 0x69a8f7
// SYNTHETIC: EXE 0x69a901
// SYNTHETIC: EXE 0x69a90d
// SYNTHETIC: EXE 0x69a91d
// SYNTHETIC: EXE 0x69a927
// SYNTHETIC: EXE 0x69a931
// SYNTHETIC: EXE 0x69a93d
// SYNTHETIC: EXE 0x69a94d
// SYNTHETIC: EXE 0x69a957
// SYNTHETIC: EXE 0x69a961
// SYNTHETIC: EXE 0x69a96d
// SYNTHETIC: EXE 0x69a97d
// SYNTHETIC: EXE 0x69a987
// SYNTHETIC: EXE 0x69a991
// SYNTHETIC: EXE 0x69a99d
// SYNTHETIC: EXE 0x69a9ad
// SYNTHETIC: EXE 0x69a9b7
// SYNTHETIC: EXE 0x69a9c1
// SYNTHETIC: EXE 0x69a9cd
// SYNTHETIC: EXE 0x69a9dd
// SYNTHETIC: EXE 0x69a9e7
// SYNTHETIC: EXE 0x69a9f1
// SYNTHETIC: EXE 0x69a9fd
// SYNTHETIC: EXE 0x69aa0d
// SYNTHETIC: EXE 0x69aa17
// SYNTHETIC: EXE 0x69aa21
// SYNTHETIC: EXE 0x69aa2d
// SYNTHETIC: EXE 0x69aa3d
// SYNTHETIC: EXE 0x69aa47
// SYNTHETIC: EXE 0x69aa51
// SYNTHETIC: EXE 0x69aa5d
// SYNTHETIC: EXE 0x69aa6d
// SYNTHETIC: EXE 0x69aa77
// SYNTHETIC: EXE 0x69aa81
// SYNTHETIC: EXE 0x69aa8d
// SYNTHETIC: EXE 0x69aa9d
// SYNTHETIC: EXE 0x69aaa7
// SYNTHETIC: EXE 0x69aab1
// SYNTHETIC: EXE 0x69aabd
// SYNTHETIC: EXE 0x69aacd
// SYNTHETIC: EXE 0x69aad7
// SYNTHETIC: EXE 0x69aae1
// SYNTHETIC: EXE 0x69aaed
// SYNTHETIC: EXE 0x69aafd
// SYNTHETIC: EXE 0x69ab07
// SYNTHETIC: EXE 0x69ab11
// SYNTHETIC: EXE 0x69ab1d
// SYNTHETIC: EXE 0x69ab2d
// SYNTHETIC: EXE 0x69ab37
// SYNTHETIC: EXE 0x69ab41
// SYNTHETIC: EXE 0x69ab4d
// SYNTHETIC: EXE 0x69ab5d
// SYNTHETIC: EXE 0x69ab67
// SYNTHETIC: EXE 0x69ab71
// SYNTHETIC: EXE 0x69ab7d
// SYNTHETIC: EXE 0x69ab8d
// SYNTHETIC: EXE 0x69ab97
// SYNTHETIC: EXE 0x69aba1
// SYNTHETIC: EXE 0x69abad
// SYNTHETIC: EXE 0x69abbd
// SYNTHETIC: EXE 0x69abc7
// SYNTHETIC: EXE 0x69abd1
// SYNTHETIC: EXE 0x69abdd
// SYNTHETIC: EXE 0x69abed
// SYNTHETIC: EXE 0x69abf7
// SYNTHETIC: EXE 0x69ac01
// SYNTHETIC: EXE 0x69ac0d
// SYNTHETIC: EXE 0x69ac1d
// SYNTHETIC: EXE 0x69ac27
// SYNTHETIC: EXE 0x69ac31
// SYNTHETIC: EXE 0x69ac3d
// SYNTHETIC: EXE 0x69ac4d
// SYNTHETIC: EXE 0x69ac57
// SYNTHETIC: EXE 0x69ac61
// SYNTHETIC: EXE 0x69ac6d
// SYNTHETIC: EXE 0x69ac7d
// SYNTHETIC: EXE 0x69ac87
// SYNTHETIC: EXE 0x69ac91
// SYNTHETIC: EXE 0x69ac9d
// SYNTHETIC: EXE 0x69acad
// SYNTHETIC: EXE 0x69acb7
// SYNTHETIC: EXE 0x69acc1
// SYNTHETIC: EXE 0x69accd
// SYNTHETIC: EXE 0x69acdd
// SYNTHETIC: EXE 0x69ace7
// SYNTHETIC: EXE 0x69acf1
// SYNTHETIC: EXE 0x69acfd
// SYNTHETIC: EXE 0x69ad0d
// SYNTHETIC: EXE 0x69ad17
// SYNTHETIC: EXE 0x69ad21
// SYNTHETIC: EXE 0x69ad2d
// SYNTHETIC: EXE 0x69ad3d
// SYNTHETIC: EXE 0x69ad47
// SYNTHETIC: EXE 0x69ad51
// SYNTHETIC: EXE 0x69ad5d
// SYNTHETIC: EXE 0x69ad6d
// SYNTHETIC: EXE 0x69ad77
// SYNTHETIC: EXE 0x69ad81
// SYNTHETIC: EXE 0x69ad8d
// SYNTHETIC: EXE 0x69ad9d
// SYNTHETIC: EXE 0x69ada7
// SYNTHETIC: EXE 0x69adb1
// SYNTHETIC: EXE 0x69adbd
// SYNTHETIC: EXE 0x69adcd
// SYNTHETIC: EXE 0x69add7
// SYNTHETIC: EXE 0x69ade1
// SYNTHETIC: EXE 0x69aded
// SYNTHETIC: EXE 0x69adfd
// SYNTHETIC: EXE 0x69ae07
// SYNTHETIC: EXE 0x69ae11
// SYNTHETIC: EXE 0x69ae1d
// SYNTHETIC: EXE 0x69ae2d
// SYNTHETIC: EXE 0x69ae37
// SYNTHETIC: EXE 0x69ae41
// SYNTHETIC: EXE 0x69ae4d
// SYNTHETIC: EXE 0x69ae5d
// SYNTHETIC: EXE 0x69ae67
// SYNTHETIC: EXE 0x69ae71
// SYNTHETIC: EXE 0x69ae7d
// SYNTHETIC: EXE 0x69ae8d
// SYNTHETIC: EXE 0x69ae97
// SYNTHETIC: EXE 0x69aea1
// SYNTHETIC: EXE 0x69aead
// SYNTHETIC: EXE 0x69aebd
// SYNTHETIC: EXE 0x69aec7
// SYNTHETIC: EXE 0x69aed1
// SYNTHETIC: EXE 0x69aedd
// SYNTHETIC: EXE 0x69aeed
// SYNTHETIC: EXE 0x69aef7
// SYNTHETIC: EXE 0x69af01
// SYNTHETIC: EXE 0x69af0d
// SYNTHETIC: EXE 0x69af1d
// SYNTHETIC: EXE 0x69af27
// SYNTHETIC: EXE 0x69af31
// SYNTHETIC: EXE 0x69af3d
// SYNTHETIC: EXE 0x69af4d
// SYNTHETIC: EXE 0x69af57
// SYNTHETIC: EXE 0x69af61
// SYNTHETIC: EXE 0x69af6d
// SYNTHETIC: EXE 0x69af7d
// SYNTHETIC: EXE 0x69af87
// SYNTHETIC: EXE 0x69af91
// SYNTHETIC: EXE 0x69af9d
// SYNTHETIC: EXE 0x69afad
// SYNTHETIC: EXE 0x69afb7
// SYNTHETIC: EXE 0x69afc1
// SYNTHETIC: EXE 0x69afcd
// SYNTHETIC: EXE 0x69afdd
// SYNTHETIC: EXE 0x69afe7
// SYNTHETIC: EXE 0x69aff1
// SYNTHETIC: EXE 0x69affd
// SYNTHETIC: EXE 0x69b00d
// SYNTHETIC: EXE 0x69b017
// SYNTHETIC: EXE 0x69b021
// SYNTHETIC: EXE 0x69b02d
// SYNTHETIC: EXE 0x69b03d
// SYNTHETIC: EXE 0x69b047
// SYNTHETIC: EXE 0x69b051
// SYNTHETIC: EXE 0x69b05d
// SYNTHETIC: EXE 0x69b06d
// SYNTHETIC: EXE 0x69b077
// SYNTHETIC: EXE 0x69b081
// SYNTHETIC: EXE 0x69b08d
// SYNTHETIC: EXE 0x69b09d
// SYNTHETIC: EXE 0x69b0a7
// SYNTHETIC: EXE 0x69b0b1
// SYNTHETIC: EXE 0x69b0bd
// SYNTHETIC: EXE 0x69b0cd
// SYNTHETIC: EXE 0x69b0d7
// SYNTHETIC: EXE 0x69b0e1
// SYNTHETIC: EXE 0x69b0ed
// SYNTHETIC: EXE 0x69b0fd
// SYNTHETIC: EXE 0x69b107
// SYNTHETIC: EXE 0x69b111
// SYNTHETIC: EXE 0x69b11d
// SYNTHETIC: EXE 0x69b12d
// SYNTHETIC: EXE 0x69b137
// SYNTHETIC: EXE 0x69b141
// SYNTHETIC: EXE 0x69b14d
// SYNTHETIC: EXE 0x69b15d
// SYNTHETIC: EXE 0x69b167
// SYNTHETIC: EXE 0x69b171
// SYNTHETIC: EXE 0x69b17d
// SYNTHETIC: EXE 0x69b18d
// SYNTHETIC: EXE 0x69b197
// SYNTHETIC: EXE 0x69b1a1
// SYNTHETIC: EXE 0x69b1ad
// SYNTHETIC: EXE 0x69b1bd
// SYNTHETIC: EXE 0x69b1c7
// SYNTHETIC: EXE 0x69b1d1
// SYNTHETIC: EXE 0x69b1dd
