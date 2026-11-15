// C:\EEX\Empire Earth\Civilization Builder\EECBCivilizationFiles.cpp (file name inferred: no __FILE__ or RTTI of its
// own). 0x27fdfe..0x2813f4, after EECivilizationBuilder.cpp and before EECivilizationManager.cpp. The builder's half
// (its state, the file being saved, the builder's open and close, buys, returns, deletes, accept: CivBuilder*) is
// Rust's (ee_screens civ_whole.rs); this unit keeps the simulation's: the civilization timer's limits, the
// civilizations' directory, a civilization file's load (EELoadCivilization: the legacy binary form or the keyed
// text) and its application to a player (EEApplyCivilizationFile: the AI's, the campaign's, the player resets'), the
// server's grant of a player's upgrade request (EECBRequestUpgrades). Unit flags: the exe module set.
#include <windows.h>
#include <mbstring.h>
#include <algorithm>
#include <list>

#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GESurface.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GETextureManager.h"
#include "lle/GETexture.h"
#include "GEWindow.h"
#include "lle/GEScreen.h"
#include "lle/GEViewport.h"
#include "lle/GEVertexArray.h"
#include "lle/GERasterizer.h"
#include "lle/GERhombus.h"
#include "lle/GEBitmap.h"
#include "lle/FSFileSpec.h"
#include "lle/FSArchive.h"
#include "lle/FSPath.h"
#include "lle/TMPoll.h"
#include "lle/TSCriticalSection.h"
#include "lle/TSScheduler.h"
#include "lle/SESample.h"
#include "lle/SESMInstanceNode.h"
#include "lle/SESoundManager.h"
#include "lle/Input System.h"
#include "lle/ISKeyboard.h"
#include "lle/ISMouse.h"
#include "lle/UIControl.h"
#include "lle/UIEvent.h"
#include "lle/UIEntityGroup.h"
#include "lle/UIBackground.h"
#include "lle/UIFontTable.h"
#include "lle/UIHotKey.h"
#include "lle/UIIMECandidate.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"
#include "lle/UIButton.h"
#include "lle/NEMessage.h"
#include "lle/NESynchronousCommunications.h"
#include "lle/GELine.h"
#include "lle/UITextLabel.h"
#include "lle/UIListBox.h"
#include "lle/GEFont.h"
#include "lle/GEText.h"
#include "lle/UILBString.h"
#include "lle/UIFormManager.h"
#include "lle/UResourceFile.h"

#include "ee/EmpireEarth.h"
#include "ee/EECommunications.h"
#include "ee/EEUserInterface.h"
#include "ee/EEUIManager.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEMessage.h"

#include "ee/EECBUIListBox.h"

#include "ee/EECivilizationManager.h"
#include "EEDataText.h"   // the keyed .civ text
#include "platform/PlatMessage.h"
#include "lle/UIScrollBar.h"
#include "lle/UIEditBox.h"
#include "lle/UIBlinkingButton.h"
#include "lle/FSArchive.h"
#include "lle/FSPath.h"
#include "lle/UIFileListBox.h"
#include "ee/EEPlayer.h"
#include "ee/EEUIFMainGame.h"
#include "ee/EECivilization.h"
#include "ee/EEGameOptions.h"
#include "ee/EEAction.h"
#include "ee/EEAResetAllPlayerInfo.h"
#include "ee/EEGameSound.h"
#include "ee/EEPhysics.h"
#include "ee/EECivilizationBuilder.h"
#include "ee/EEStringIDs.h"
#include "ee/EECivCapture.h"
extern char gEmptyString[];                 // 0x4e5b98 (pooled "")

// chain at 0x67fe94 (wrapper)
//   object 0x965518  ctor __imp_??0FSFileSpec@@QAE@XZ  dtor __imp_??1FSFileSpec@@UAE@XZ
// the civilization file being saved or loaded (the builder's host, EECivilizationBuilder.cpp, saves through it)
FSFileSpec gU565518;
// SYNTHETIC: EXE 0x67fe94   (wrapper)
// SYNTHETIC: EXE 0x67fe9e   (ctor)
// SYNTHETIC: EXE 0x67fea9   (registrar)
// SYNTHETIC: EXE 0x67feb5   (dtor)

// FUNCTION: EXE 0x67ffae
// Returns the civilization choice time limit (300 s)
unsigned long EEGetCivTimerLimit()
{
    return 0x493e0;
}

// FUNCTION: EXE 0x67ffb4
// Returns the time of the civilization timer's first warning (240 s)
unsigned long EEGetCivTimerFirstWarning()
{
    return 0x3a980;
}

// FUNCTION: EXE 0x67ffba
// Returns the time of the civilization timer's second warning (270 s)
unsigned long EEGetCivTimerSecondWarning()
{
    return 0x41eb0;
}

// FUNCTION: EXE 0x67ffc0
// Returns the civilization folder: the user's full path (Users\default\Civilizations\) or the relative one
UString EEGetCivilizationDirectory(bool inX)
{
    UString theDirectory;
    if (inX)
        theDirectory = gStartupDirectory + "Users\\default\\" + kCivilizationsKey + "\\";
    else
        theDirectory = kCivilizationsKey + "\\";
    return theDirectory;
}

// FUNCTION: EXE 0x6806e7
// Gives a player the civilization from a file (the game's, else the user's), buying its upgrades at no cost;
// a game file's premade civilization keeps its localized name
static long EEApplyCivilizationFileUncaptured(UString& ioName, EEPlayer* inPlayer);
long EEApplyCivilizationFile(UString& ioName, EEPlayer* inPlayer)
{
    UString theName = ioName;
    long theResult = EEApplyCivilizationFileUncaptured(ioName, inPlayer);
    EECivCaptureApplyFile(theName, inPlayer, theResult);   // TEST ONLY: CVAF
    return theResult;
}

static long EEApplyCivilizationFileUncaptured(UString& ioName, EEPlayer* inPlayer)
{
    long theNameID;
    unsigned long theCost;
    std::vector<EECivUpgrade> theUpgrades;
    UWideString theName;
    long theResult = EELoadCivilization(ioName, EEGetCivilizationDirectory(false), theName, theUpgrades);
    if (theResult != 0) {
        theResult = EELoadCivilization(ioName, EEGetCivilizationDirectory(true), theName, theUpgrades);
    } else {
        gCivilizationManager.MatchesPremadeCivilization(UWideString((const char*)ioName), theName, theUpgrades, &theNameID);
        if (theNameID != 0)
            gResource->GetString(theNameID, theName);
    }
    if (theResult == 0) {
        EECivilization& theCivilization = inPlayer->mCivilization;
        theCivilization.mPurchasedUpgrades.clear();
        theCivilization.mAvailablePoints = theCivilization.mMaxPoints;
        theCivilization.SetPointsAndMax(0);
        theCivilization.SetName(theName);
        for (std::vector<EECivUpgrade>::iterator i = theUpgrades.begin(); i != theUpgrades.end(); i++) {
            theCivilization.CanAfford(*i, &theCost, NULL);
            theCivilization.SetPointsAndMax(theCivilization.GetMaxPoints() + theCost);
            theCivilization.Buy(*i, inPlayer->mPlayerID, true);
        }
        theCivilization.SetPoints(0);
    }
    return theResult;
}

// Reads a keyed .civ text (the builder writes it: ee_screens civ_whole.rs keyed_file); 0, or kErrorFailed with the problem shown
static long EEReadKeyedCivilization(const std::string& inText, const UString& inFile, UWideString& outName,
                                    std::vector<EECivUpgrade>& outUpgrades)
{
    std::string theError, theName;
    std::vector<EEDataText::CivKey> theKeys;
    std::string theWhere = std::string((const char*)inFile);
    if (EEDataText::ReadCivilization(inText, theWhere.c_str(), theName, theKeys, theError)) {
        outName = theName.c_str();
        for (const EEDataText::CivKey& theItem : theKeys) {
            EECivUpgrade theUpgrade;
            if (!gCivilizationManager.FindUpgradeKey(theItem.mKey, theUpgrade)) {
                theError = theWhere + " line " + std::to_string(theItem.mLine) + ": '" + theItem.mKey +
                           "' is not an upgrade (<dbcivilization id>.<civbuilder attribute id>)";
                break;
            }
            outUpgrades.push_back(theUpgrade);
        }
    }
    if (theError.empty())
        return 0;
    PlatMessageShow("Empire Earth: civilization", theError.c_str(), PLAT_MESSAGE_ERROR);
    return kErrorFailed;
}

// FUNCTION: EXE 0x680840
// Reads a civilization file's name and upgrade ids from the data path or the given directory
// (the legacy binary form: its (category << 6) | level ids converted; or the keyed text, EEKeyedCivilization)
static long EELoadCivilizationUncaptured(const UString& inName, const UString& inDirectory, UWideString& outName,
                                        std::vector<EECivUpgrade>& outUpgrades);
long EELoadCivilization(const UString& inName, const UString& inDirectory, UWideString& outName,
                        std::vector<EECivUpgrade>& outUpgrades)
{
    long theResult = EELoadCivilizationUncaptured(inName, inDirectory, outName, outUpgrades);
    EECivCaptureLoad(inName, inDirectory, theResult, outName, outUpgrades);   // TEST ONLY: CVLD
    return theResult;
}

static long EELoadCivilizationUncaptured(const UString& inName, const UString& inDirectory, UWideString& outName,
                                        std::vector<EECivUpgrade>& outUpgrades)
{
    FSFileSpec* theFile = NULL;
    unsigned long theLength = 0;
    unsigned long theRead = 0;
    unsigned long theCount = 0;
    unsigned long theUpgrade;
    char theBuffer[0x200];
    long theResult;
    gU565518.Close();
    bool theMissing = gDataPath->FindFile(inDirectory + inName + kCivKey, theFile) != 0;
    if (theMissing) {
        theFile = new FSFileSpec;
        gU565518.SetVolume(gStartupVolume);
        gU565518.SetDirectory(inDirectory);
        gU565518.SetFile(inName + kCivKey);
        *theFile = gU565518;
    }
    theResult = theFile->Open(kFileAccessRead);
    if (theResult == 0) {
        unsigned long theSize = 0;
        if (theFile->GetLength(theSize) == kErrorOK && theSize >= 6) {   // the keyed text?
            std::string theText(theSize, '\0');
            unsigned long theGot = 0;
            if (theFile->Read(&theText[0], theSize, theGot) == kErrorOK && theGot == theSize &&
                (theText.compare(0, 6, "eedata") == 0 || theText[0] == '#')) {
                theFile->Close();
                theResult = EEReadKeyedCivilization(theText, inName + kCivKey, outName, outUpgrades);
                delete theFile;
                return theResult;
            }
            theFile->Close();
            theResult = theFile->Open(kFileAccessRead);
        }
        if (theResult == 0)
            theResult = theFile->Read(theLength);
        if (theResult == 0) {
            if (theLength == 0) {
                outName = gEmptyString;
            } else {
                memset(theBuffer, 0, sizeof(theBuffer));
                if (theFile->Read(theBuffer, theLength, theRead) == kErrorOK)
                    outName = theBuffer;
            }
            theResult = theFile->Read(theCount);
            if (theResult == 0) {
                for (unsigned long i = 0; i < theCount && theResult == 0; i++) {
                    theResult = theFile->Read(theUpgrade);
                    if (theResult == 0)
                        outUpgrades.push_back(EECivilizationManager::FromLegacy(theUpgrade));
                }
            }
        }
    }
    if (theFile != NULL) {
        theFile->Close();
        delete theFile;
    }
    return theResult;
}

// FUNCTION: EXE 0x680a05
// Grants a player's civilization upgrade request when allowed, valid and affordable (broadcast in multiplayer);
// otherwise posts a chat line naming the refused civilization
void EECBRequestUpgrades(unsigned char inPlayer, UWideString& inName, std::vector<EECivUpgrade>& inUpgrades)
{
    UWideString theFormat;
    UWideString theText;
    EEPlayer* thePlayer = ::gWorld.mPlayers[inPlayer];
    long theNameID = 0;
    bool theAllowed;
    if (::gWorld.mGameOptions.CanBuyCivilization())
        theAllowed = thePlayer->mCivUpgradesPurchased == false;
    else
        theAllowed = true;
    bool theValid;
    if (!gAllowCustomCivs && !gIsScenarioGame && !gUIManager->GetInScenarioEditor())
        theValid = gCivilizationManager.MatchesPremadeCivilization(inName, inName, inUpgrades, &theNameID);
    else
        theValid = true;
    if (theAllowed && theValid && thePlayer->mCivilization.CanAffordAll(inUpgrades)) {
        EEMCivUpgradesPurchased* theMessage = new EEMCivUpgradesPurchased(inPlayer, inUpgrades, inName, theNameID);
        if (gCommunications->GetIsMultiplayerSession())
            gCommunications->ProcessLocalUserInput(theMessage, kSlotNone);
        else {
            theMessage->Execute();
            theMessage->Dereference();
        }
    } else {
        gResource->GetString(IDS_PLAYER_HAS_CIVILIZATION_NAMED_WHICH, theFormat);
        sprintf(theText, theFormat, (const char*)thePlayer->mName, (const char*)inName,
                (const char*)::gWorld.GetCurrentPlayer()->mName);
        gCommunications->ProcessLocalUserInput(new EEMRecord(kMessageSelectPlayerChatType, {0L}), kSlotNone);
        gCommunications->ProcessLocalUserInput(new EEMRecord(kMessageInGameChat, {theText}), kSlotNone);
        gPhysics->GetRandom(0L, 1L);
    }
}

