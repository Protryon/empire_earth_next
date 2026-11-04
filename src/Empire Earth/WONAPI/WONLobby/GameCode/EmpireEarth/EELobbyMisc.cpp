// C:\EEX\Empire Earth\WONAPI\WONLobby\GameCode\EmpireEarth\EELobbyMisc.cpp (name PROVISIONAL: no __FILE__, it sorts
// between EELobbyInit.cpp and EELobbyPlayer.cpp). The lobby's glue to the game: the lobby lock (a WONAPI critical
// section the renderer takes too), saved games in the game room (header checks, set-up from the saved options), the
// game options and map types, the player colours, the version check and the lobby music: 0x297ee7..0x299838 (the
// std::set/map COMDATs 0x298b02..0x299838 close it). Module flags (r12-misc-h5, round 12).
#include <windows.h>
#include <string>
#include <list>
#include <set>
#include <map>

#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/FSFileSpec.h"
#include "lle/FSRestorable.h"

#include "ee/EmpireEarth.h"
#include "ee/EEGameOptions.h"
#include "ee/EEGamePreferences.h"
#include "ee/EESMMediaStrings.h"
#include "ee/EEPSaveGameHeader.h"
#include "ee/EESaveGameFiles.h"
#include "ee/EERandomMapGenerators.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/EELobbySound.h"
#include "ee/wonapi/EELobbyGameCode.h"

using namespace WONAPI;


static CriticalSection gEELobbyCriticalSection;     // 0x565858

// FUNCTION: EXE 0x697f15
void Fn297F15()
{
    gEELobbyCriticalSection.Enter();
}

// FUNCTION: EXE 0x697f21
void Fn297F21()
{
    gEELobbyCriticalSection.Leave();
}

// FUNCTION: EXE 0x697f2d
bool EELobbyGetSavedGameInfo(const char* inFile, unsigned long& outLength, unsigned long& outChecksum)
{
    EEPSaveGameHeader theHeader;
    EEGameOptions theOptions;
    FSFileSpec theSpec;
    theSpec.SetFile(inFile);
    if (theSpec.Open(1) != keseOK)
        return false;
    unsigned long theLength = 0;
    theSpec.GetLength(theLength);
    outLength = theLength;
    theSpec.Close();
    theHeader.SetMajorVersion(kSaveGameMajorVersion);
    theHeader.SetMinorVersion(kSaveGameMinorVersion);
    if (theHeader.LoadFromFile(theSpec) != keseOK)
        return false;
    outChecksum = theHeader.mChecksum;
    return true;
}

// FUNCTION: EXE 0x698005
// NEAR: register choice (esi/edi swapped, saved in the prologue in the original) and the player-missing return is
// laid out at the end there; the found-flag shape is worse (81%)
bool EELobbyLoadSavedGame(const char* inFile)
{
    EEPSaveGameHeader theHeader;
    EEGameOptions theOptions;
    FSFileSpec theSpec;
    theSpec.SetFile(inFile);
    if (theSpec.Open(1) != keseOK) {
        LobbyMessageBox(gEELobbyInterface_LoadGameError, gEELobbyInterface_ErrorReadingFile);
        return false;
    }
    theSpec.Close();
    theHeader.SetMajorVersion(kSaveGameMajorVersion);
    theHeader.SetMinorVersion(kSaveGameMinorVersion);
    if (theHeader.LoadFromFile(theSpec) != keseOK) {
        LobbyMessageBox(gEELobbyInterface_LoadGameError, gEELobbyInterface_ErrorReadingFile);
        return false;
    }
    theOptions = theHeader.mGameOptions;
    EELobbyGame* theGame = (EELobbyGame*)GetMyLobbyGame();
    if (theGame == NULL)
        return false;
    std::set<GUIString> theNames;
    int i; for (i = 0; i < 8; i++) {
        if (theOptions.mPlayers[i].mType == 1 || theOptions.mPlayers[i].mType == 2)
            theNames.insert(EELobbyToGUIString(theOptions.mPlayers[i].mName));
    }
    for (i = 0; i < 8; i++) {
        EELobbyPlayerSlot* theSlot = theGame->GetPlayerSlot(i);
        if (theSlot != NULL && theSlot->mOptions[0] == 3 && theNames.find(theSlot->GetName()) == theNames.end()) {
            LobbyMessageBox(gEELobbyInterface_LoadGameError,
                            LobbyFormatString(gEELobbyInterface_PlayerNotInGame, theSlot->GetName()));
            return false;
        }
    }
    EELobbySetupSavedGame(theOptions, inFile, theHeader);
    return true;
}

// FUNCTION: EXE 0x6981ff
GUIString EELobbyToGUIString(const UWideString& inText)
{
    return GUIString(StringToWString(std::string(inText), -1));
}

// FUNCTION: EXE 0x698266
void EELobbySetupSavedGame(const EEGameOptions& inOptions, const char* inFile, const EEPSaveGameHeader& inHeader)
{
    EELobbyGame* theGame = (EELobbyGame*)GetMyLobbyGame();
    if (theGame == NULL)
        return;
    WONFile theFile(inFile);
    theGame->SetOption(0, 1);
    {
        GUIString theName(theFile.GetFileName());
        unsigned long theCount = inHeader.mChecksum;
        theGame->SetGameFile(theName, theFile.GetSize(), theCount, true);
    }
    EELobbySetGameOptions(inOptions, theGame);
    std::set<long> theFreeOptions;
    std::set<long> theFreeSlots;
    std::map<GUIString, long> theNames;
    long i;
    for (i = 0; i < 8; i++) {
        const EEGameOptionsPlayer& thePlayer = inOptions.mPlayers[i];
        if (thePlayer.mType == 1 || thePlayer.mType == 2)
            theNames[EELobbyToGUIString(thePlayer.mName)] = i;
        theFreeOptions.insert(i);
        theFreeSlots.insert(i);
    }
    for (i = 0; i < 8; i++) {
        EELobbyPlayerSlot* theSlot = theGame->GetPlayerSlot(i);
        if (theSlot->mOptions[0] == 3) {
            long theIndex = theNames[theSlot->GetName()];
            EELobbySetSlot(theSlot, inOptions.mPlayers[theIndex]);
            theFreeOptions.erase(theIndex);
            theFreeSlots.erase(i);
        }
    }
    while (theFreeOptions.size() != 0 && theFreeSlots.size() != 0) {
        EELobbyPlayerSlot* theSlot = theGame->GetPlayerSlot(*theFreeSlots.begin());
        EELobbySetSlot(theSlot, inOptions.mPlayers[*theFreeOptions.begin()]);
        theFreeSlots.erase(theFreeSlots.begin());
        theFreeOptions.erase(theFreeOptions.begin());
    }
}

// FUNCTION: EXE 0x69848e
GUIString::GUIString(const std::string& inText) : mData(new GUIStringData(inText))
{
}

// FUNCTION: EXE 0x6984ca
// NEAR: the original's switch compares unsigned (`jbe`): EEGameOptionsPlayer::mType is long in the shared header
void EELobbySetSlot(EELobbyPlayerSlot* inSlot, const EEGameOptionsPlayer& inPlayer)
{
    switch (inPlayer.mType) {
    case 0:
        inSlot->Fn299983();
        break;
    case 1:
    case 2:
        if (inSlot->mClient.mPtr != NULL && inSlot->mClient->mPlayer != NULL) {
            inSlot->mOptions[4] = inPlayer.mType == 2;
            inSlot->SetPlayer(inSlot->mClient.mPtr, (EELobbyPlayer*)inSlot->mClient->mPlayer);
        } else if (inPlayer.mName == "") {
            inSlot->Fn29997B();
        } else {
            inSlot->SetName(EELobbyToGUIString(inPlayer.mName));
            inSlot->mOptions[4] = inPlayer.mType == 2;
        }
        break;
    case 3:
        inSlot->Fn299973();
        return;
    case 4:
        inSlot->Fn29997B();
        return;
    }
    if (inPlayer.mType == 1)
        inSlot->mOptions[2] = (unsigned char)inPlayer.mColor;
    if (inPlayer.mType != 2) {
        inSlot->mOptions[1] = (unsigned char)inPlayer.mTeam;
        inSlot->mOptions[3] = (unsigned char)inPlayer.mStartingCitizens;
    }
}

// FUNCTION: EXE 0x6985a8
void EELobbySetGameOptions(const EEGameOptions& inOptions, EELobbyGame* inGame)
{
    unsigned long theMapType = 0;
    EERMFindGenerator(inOptions.mMapType, theMapType);
    inGame->SetOption(2, theMapType);
    inGame->SetOption(1, inOptions.mMapSize);
    inGame->SetOption(3, inOptions.mResources);
    inGame->SetOption(4, inOptions.mEpochStart);
    inGame->SetOption(5, inOptions.mEpochEnd);
    inGame->SetOption(6, inOptions.mVariant - 1);
    inGame->SetOption(7, inOptions.mDifficulty);
    switch (inOptions.mGameSpeed) {
    case 0:
        inGame->SetOption(8, 0);
        break;
    case 1:
        inGame->SetOption(8, 1);
        break;
    case 2:
        inGame->SetOption(8, 2);
        break;
    case 3:
        inGame->SetOption(8, 3);
        break;
    }
    inGame->SetOption(9, inOptions.mUnitLimit);
    inGame->SetOption(10, inOptions.mWondersForVictory);
    inGame->SetOption(11, inOptions.mRevealMap != false);
    inGame->SetOption(12, inOptions.mAllowCustomCivs != false);
    inGame->SetOption(13, inOptions.mLockTeams != false);
    inGame->SetOption(14, inOptions.mLockSpeed != false);
    inGame->SetOption(15, inOptions.mVictory != false);
}

// FUNCTION: EXE 0x6986e9
GUIString& EELobbyGetMapTypeName(int inMapType)
{
    static GUIString theName;
    if (inMapType >= 0 && (unsigned long)inMapType < EERMGetNumberOfGenerators()) {
        theName = EELobbyToGUIString(EERMGetGeneratorDisplayName(inMapType));
        return theName;
    }
    return gEmptyGUIString;
}

// FUNCTION: EXE 0x69876a
void EELobbyFn29876A(ComboBox* inCombo)
{
    inCombo->Clear(true);
    int theCount = EERMGetNumberOfGenerators();
    for (int i = 0; i < theCount; i++) {
        GUIString theName = EELobbyToGUIString(EERMGetGeneratorDisplayName(i));
        inCombo->AddString(theName);
    }
}

// FUNCTION: EXE 0x6987bd
void EELobbyFn2987BD()
{
    int theCount = EERMGetNumberOfGenerators();
    for (int i = 0; i < theCount; i++) {
        GUIString theName = EELobbyToGUIString(EERMGetGeneratorDisplayName(i));
        LobbyAddGameOption("MapType", theName, true);
    }
}

// FUNCTION: EXE 0x698824
unsigned long EELobbyFn298824()
{
    return EERMGetNumberOfGenerators();
}

// FUNCTION: EXE 0x698829
void EELobbyFn298829(unsigned long* outColors, int inCount)
{
    if (inCount > 16)
        inCount = 16;
    for (int i = 0; i < inCount; i++) {
        GEColor theColor = EEGetPlayerColor(i);
        int theRed = theColor.GetRed() * 255.0f;
        int theGreen = theColor.GetGreen() * 255.0f;
        int theBlue = theColor.GetBlue() * 255.0f;
        if (theRed < 0)
            theRed = 0;
        if (theGreen < 0)
            theGreen = 0;
        if (theBlue < 0)
            theBlue = 0;
        if (theRed > 255)
            theRed = 255;
        if (theGreen > 255)
            theGreen = 255;
        if (theBlue > 255)
            theBlue = 255;
        outColors[i] = (theRed << 8 | theGreen) << 8 | theBlue;
    }
}

// FUNCTION: EXE 0x6988d8
void EELobbyFn2988D8(EELobbyGame* inGame)
{
    EELobbySetGameOptions(gGamePreferences.GetSettings().mGameOptions, inGame);
}

// FUNCTION: EXE 0x6988e9
const GUIString& EELobbyFn2988E9()
{
    static GUIString theDirectory;
    theDirectory = GetEmptyGUIStringData();
    theDirectory.append("data\\");
    theDirectory.append("Saved Games\\");
    return theDirectory;
}

// FUNCTION: EXE 0x6989a9
bool EECheck2989A9(bool inFlag)
{
    return inFlag ? gGameOptions.mIsMultiplayer : true;
}

// FUNCTION: EXE 0x6989b8
void EELobbyGetVersion(GUIString& outVersion, unsigned long& outChecksum)
{
    outVersion = EELobbyToGUIString(gGameVersion);
    outChecksum = gBinaryChecksum;
}

// FUNCTION: EXE 0x6989ef
void EELobbyFn2989EF(ResourceCollection* inCollection)
{
    ResourceConfigTable* theTable = inCollection->GetTable("LobbyGlobal", false);
    theTable->AddSound("LobbyMusic", new EELobbyMusic(SoundDescriptor("", 2)));
}

// SYNTHETIC: EXE 0x697ee7
// SYNTHETIC: EXE 0x697ef1
// SYNTHETIC: EXE 0x697efd
// SYNTHETIC: EXE 0x697f09
// SYNTHETIC: EXE 0x69875a
// SYNTHETIC: EXE 0x698999

// the std::set<GUIString>, std::set<long> and std::map<GUIString, long> COMDATs that close the unit (0x298bf9 /
// 0x298c1c, a std::list ctor/clear used by EELobbyInit.cpp and the 0x296aaa unit, are ICF copies of another type's)
// TEMPLATE: EXE 0x698b02
// TEMPLATE: EXE 0x698b2d
// TEMPLATE: EXE 0x698b7e
// TEMPLATE: EXE 0x698bce
// TEMPLATE: EXE 0x698c5c
// TEMPLATE: EXE 0x698c87
// TEMPLATE: EXE 0x698d56
// TEMPLATE: EXE 0x698d99
// TEMPLATE: EXE 0x698dba
// TEMPLATE: EXE 0x698e2b
// TEMPLATE: EXE 0x698e4c
// TEMPLATE: EXE 0x698f1b
// TEMPLATE: EXE 0x698f8c
// TEMPLATE: EXE 0x698ffd
// TEMPLATE: EXE 0x69902f
// TEMPLATE: EXE 0x6992e7
// TEMPLATE: EXE 0x699323
// TEMPLATE: EXE 0x6994b7
// TEMPLATE: EXE 0x69976f
// TEMPLATE: EXE 0x6997ab
// TEMPLATE: EXE 0x6997e0
// TEMPLATE: EXE 0x699809
