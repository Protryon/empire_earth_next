// C:\EEX\Empire Earth\Civilization Builder\EECBCivilizationFiles.cpp (file name inferred: no __FILE__ or RTTI of its
// own). 0x27fdfe..0x2813f4, after EECivilizationBuilder.cpp and before EECivilizationManager.cpp. The builder's half
// (its state, the file being saved, the builder's open and close, buys, returns, deletes, accept: CivBuilder*) is
// Rust's (ee_screens civ_whole.rs); this unit keeps the simulation's: the civilization timer's limits, the
// civilizations' directory, a civilization file's load (EELoadCivilization: found and read here, its bytes read by
// ee_tech civ_file.rs), its application to a player (EEApplyCivilizationFile: the AI's, the campaign's, the player
// resets') and the server's grant of a player's upgrade request (EECBRequestUpgrades), both Rust's (civ_file.rs)
// over this unit's host (EECivFileCall). Unit flags: the exe module set.
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
#include "rust/ee_tech.h"
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

// FUNCTION: EXE 0x680840
// Reads a civilization file's name and upgrade ids from the data path or the given directory: the file found and
// read here, its bytes read by Rust (ee_tech civ_file.rs: the legacy binary form or the keyed text)
static long EELoadCivilizationUncaptured(const UString& inName, const UString& inDirectory, UWideString& outName,
                                        std::vector<EECivUpgrade>& outUpgrades)
{
    FSFileSpec* theFile = NULL;
    gU565518.Close();
    if (gDataPath->FindFile(inDirectory + inName + kCivKey, theFile) != 0) {
        theFile = new FSFileSpec;
        gU565518.SetVolume(gStartupVolume);
        gU565518.SetDirectory(inDirectory);
        gU565518.SetFile(inName + kCivKey);
        *theFile = gU565518;
    }
    long theResult = theFile->Open(kFileAccessRead);
    if (theResult == 0) {
        unsigned long theSize = 0, theGot = 0;
        std::vector<unsigned char> theBytes;
        if (theFile->GetLength(theSize) == kErrorOK) {
            theBytes.resize(theSize);
            if (theSize > 0 && theFile->Read(theBytes.data(), theSize, theGot) != kErrorOK)
                theBytes.clear();
        }
        EeTechCivFileRead theRead;
        ee_tech_civ_read_file(theBytes.data(), theBytes.size(), (const char*)(inName + kCivKey), &theRead);
        theResult = theRead.result;
        if (theRead.has_name)
            outName = std::string((const char*)theRead.name, theRead.name_length).c_str();
        outUpgrades.insert(outUpgrades.end(), theRead.upgrades, theRead.upgrades + theRead.count);
        if (theRead.error_length > 0)
            PlatMessageShow("Empire Earth: civilization", std::string((const char*)theRead.error, theRead.error_length).c_str(),
                            PLAT_MESSAGE_ERROR);
        ee_tech_civ_file_free(&theRead);
    }
    theFile->Close();
    delete theFile;
    return theResult;
}

long EELoadCivilization(const UString& inName, const UString& inDirectory, UWideString& outName,
                        std::vector<EECivUpgrade>& outUpgrades)
{
    long theResult = EELoadCivilizationUncaptured(inName, inDirectory, outName, outUpgrades);
    EECivCaptureLoad(inName, inDirectory, theResult, outName, outUpgrades);   // TEST ONLY: CVLD
    return theResult;
}

// The C++ side of a file's application or a request (ee_tech civ_file.rs): the file's or the request's name, the
// upgrades, the player
struct EECivFileContext {
    UString* mFile;
    EEPlayer* mPlayer;
    unsigned char mPlayerIndex;
    UWideString mName;
    std::vector<EECivUpgrade> mUpgrades;
};

static void EECivFileCall(void* inCtx, const EeTechCivFileCall* c, EeTechCivFileReply* r)
{
    EECivFileContext& x = *static_cast<EECivFileContext*>(inCtx);
    EECivilization* theCivilization = x.mPlayer != NULL ? &x.mPlayer->mCivilization : NULL;
    unsigned long theCost;
    switch (c->op) {
    case EE_TECH_CIVF_OP_LOAD:
        r->value = EELoadCivilization(*x.mFile, EEGetCivilizationDirectory(c->a != 0), x.mName, x.mUpgrades);
        break;
    case EE_TECH_CIVF_OP_STATE:
        r->text = (const char*)x.mName;
        r->characters = (uint32_t)x.mName.GetLength();
        r->upgrades = x.mUpgrades.data();
        r->count = x.mUpgrades.size();
        break;
    case EE_TECH_CIVF_OP_NAME_STRING:
        gResource->GetString((long)c->a, x.mName);
        break;
    case EE_TECH_CIVF_OP_CIV_RESET:
        theCivilization->mPurchasedUpgrades.clear();
        theCivilization->mAvailablePoints = theCivilization->mMaxPoints;
        theCivilization->SetPointsAndMax(0);
        theCivilization->SetName(x.mName);
        break;
    case EE_TECH_CIVF_OP_CIV_BUY_FREE:
        theCivilization->CanAfford((EECivUpgrade)c->a, &theCost, NULL);
        theCivilization->SetPointsAndMax(theCivilization->GetMaxPoints() + theCost);
        theCivilization->Buy((EECivUpgrade)c->a, x.mPlayer->mPlayerID, true);
        break;
    case EE_TECH_CIVF_OP_CIV_DONE:
        theCivilization->SetPoints(0);
        break;
    case EE_TECH_CIVF_OP_CAN_BUY:
        r->value = ::gWorld.mGameOptions.CanBuyCivilization();
        break;
    case EE_TECH_CIVF_OP_PURCHASED:
        r->value = x.mPlayer->mCivUpgradesPurchased;
        break;
    case EE_TECH_CIVF_OP_ANY_CIV:
        r->value = gAllowCustomCivs || gIsScenarioGame || gUIManager->GetInScenarioEditor();
        break;
    case EE_TECH_CIVF_OP_CAN_AFFORD_ALL:
        r->value = theCivilization->CanAffordAll(x.mUpgrades);
        break;
    case EE_TECH_CIVF_OP_GRANT: {
        EEMRecord* theMessage = new EEMRecord(kMessageCivUpgradesPurchased, {x.mPlayerIndex, (long)c->a, x.mUpgrades, x.mName});
        if (gCommunications->GetIsMultiplayerSession())
            gCommunications->ProcessLocalUserInput(theMessage, kSlotNone);
        else {
            theMessage->Execute();
            theMessage->Dereference();
        }
        break;
    }
    case EE_TECH_CIVF_OP_REFUSE: {
        UWideString theFormat;
        UWideString theText;
        gResource->GetString(IDS_PLAYER_HAS_CIVILIZATION_NAMED_WHICH, theFormat);
        sprintf(theText, theFormat, (const char*)x.mPlayer->mName, (const char*)x.mName,
                (const char*)::gWorld.GetCurrentPlayer()->mName);
        gCommunications->ProcessLocalUserInput(new EEMRecord(kMessageSelectPlayerChatType, {0L}), kSlotNone);
        gCommunications->ProcessLocalUserInput(new EEMRecord(kMessageInGameChat, {theText}), kSlotNone);
        gPhysics->GetRandom(0L, 1L);
        break;
    }
    }
}

// FUNCTION: EXE 0x6806e7
// Gives a player the civilization from a file (the game's, else the user's), buying its upgrades at no cost;
// a game file's premade civilization keeps its localized name (ee_tech civ_file.rs apply_file)
long EEApplyCivilizationFile(UString& ioName, EEPlayer* inPlayer)
{
    UString theName = ioName;
    EECivFileContext theContext{&ioName, inPlayer, 0, UWideString(), {}};
    EeTechCivFileHost theHost{&theContext, &EECivFileCall};
    long theResult = ee_tech_civ_apply_file(&theHost, (const char*)ioName);
    EECivCaptureApplyFile(theName, inPlayer, theResult);   // TEST ONLY: CVAF
    return theResult;
}

// FUNCTION: EXE 0x680a05
// Grants a player's civilization upgrade request when allowed, valid and affordable (broadcast in multiplayer);
// otherwise posts a chat line naming the refused civilization (ee_tech civ_file.rs request_upgrades)
void EECBRequestUpgrades(unsigned char inPlayer, UWideString& inName, std::vector<EECivUpgrade>& inUpgrades)
{
    EECivFileContext theContext{NULL, ::gWorld.mPlayers[inPlayer], inPlayer, inName, inUpgrades};
    EeTechCivFileHost theHost{&theContext, &EECivFileCall};
    ee_tech_civ_request_upgrades(&theHost);
}
