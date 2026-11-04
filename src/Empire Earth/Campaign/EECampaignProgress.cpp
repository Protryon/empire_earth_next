// C:\EEX\Empire Earth\Campaign\EECampaignProgress.cpp (file name inferred: no __FILE__). A player's
// progress through a campaign, kept in the "Users\<player>\" archive: 0x28f39a..0x290514 (the four tag keys' init
// chains, then the members). Unit flags: the exe's module set.
#include <windows.h>
#include <mbstring.h>
#include <stdlib.h>
#include <set>
#include <algorithm>

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
#include "lle/FSMemoryFile.h"
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
#include "lle/UIEditBox.h"
#include "lle/UIListBox.h"
#include "lle/GEFont.h"
#include "lle/GEText.h"
#include "lle/UILBString.h"
#include "lle/UIComboBox.h"
#include "lle/UIFileComboBox.h"
#include "lle/UIFormManager.h"
#include "lle/UResourceFile.h"

#include "ee/EmpireEarth.h"
#include "ee/EEUserInterface.h"
#include "ee/EEUIManager.h"
#include "ee/EECampaignFormManager.h"
#include "ee/EECampaignProgress.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEPlayer.h"
#include "ee/EECBEditableCivilization.h"
#include "ee/EECivilization.h"

// UString globals used before their definitions (gU5656B8/gU5656C8 are defined at the end of this file, gU530B88 in
// Empire Earth.cpp); no class/static/const owner is claimed (provisional names)
extern UString gU5656B8; // original UString("Civilizations")
extern UString gU5656C8; // original UString(".civ")
extern UString gU530B88; // existing Empire Earth.cpp archive extension object
// cdecl civilization helpers (the original's callers push zero and two arguments; the second returns an error code)
void Fn280BB1();
long Fn28015D(const UString& inName, bool inCampaign);

// the progress archive's tag keys (unguarded file-scope globals, 0x565720 down to 0x5656d8)
const UWideString kEECampaignProgressChecksumKey("Checksum");
const UWideString kEECampaignProgressNumScenariosCompletedKey("NumScenariosCompleted");
const UWideString kEECampaignProgressNumCivPointsAvailableKey("NumCivPointsAvailable");
const UWideString kEECampaignProgressScenarioNameKey("ScenarioName");

// FUNCTION: EXE 0x68f462
// Creates empty campaign progress: no completed scenarios
EECampaignProgress::EECampaignProgress()
{
    this->mCivilizationKeys.clear();
    this->mCompletedScenarios.clear();
    this->mCivPointsAvailable.clear();
    this->mNumScenariosCompleted = 0;
}

// FUNCTION: EXE 0x68f4b9
// Destroys the campaign progress
EECampaignProgress::~EECampaignProgress()
{
}

// FUNCTION: EXE 0x68f4d5
// Builds a saved-civilization key "<campaign>.<scenario>.<checksum>"
void EECampaignProgress::MakeKey(UWideString& outKey, const UWideString& inCampaign, const UWideString& inScenario,
                                 unsigned long inChecksum)
{
    sprintf(outKey, "%s.%s.%d", (const char*)inCampaign, (const char*)inScenario, inChecksum);
}

// FUNCTION: EXE 0x68f4fc
// Returns a copy of the completed scenario names
void EECampaignProgress::GetCompletedScenarios(std::vector<UWideString>& outScenarios)
{
    outScenarios.clear();
    outScenarios.resize(this->mCompletedScenarios.size());
    for (long i = this->mCompletedScenarios.size() - 1; i >= 0; i--)
        outScenarios[i] = this->mCompletedScenarios[i];
}

// The pooled empty byte string (referenced at 0x68f763)
extern char gU4E5B98[];
// Original cdecl scenario-civilization loader (EEAResetAllPlayerInfo, 0x2806e7).
long Fn2806E7(UString& inName, EEPlayer* inPlayer);

// FUNCTION: EXE 0x68f56d
// Before a scenario of a user campaign, loads the player's progress and gives the current player the civilization
// saved after the previous scenario, plus the civ points then available; returns the first error
long EECampaignProgress::RestoreCivilization(const UWideString& inUser, const UWideString& inCampaign,
                                  const UWideString& inScenario, unsigned long inChecksum)
{
    std::vector<UWideString> theCompleted;
    FSFileSpec theDirectory;
    UWideString theKey;
    UWideString theChecksum;
    UWideString thePreviousScenario;
    UString theDestination;
    FSPath* thePath = new FSPath;
    bool theExtracted = false;
    long theResult = kErrorOK;

    if (EECampaignFormManager::GetBuiltInCampaign(inCampaign) == 0) {
        theResult = this->Load(inUser, inCampaign, inChecksum, false);
        if (theResult == kErrorOK) {
            sprintf(theChecksum, "%d", inChecksum);
            theDirectory.SetVolume(gStartupVolume);
            theDirectory.SetDirectory(gStartupDirectory + "Users\\" + (const char*)inUser + "\\");
            thePath->AddDirectory(theDirectory, true);
            FSFileSpec* theFile;
            theResult = thePath->FindFile(UString((const char*)(inCampaign + theChecksum + (const char*)gU530B88)),
                                         theFile);
            if (theResult == kErrorOK) {
                FSArchive* theArchive = new FSArchive(*theFile);
                theResult = theArchive->Open();
                if (theResult == kErrorOK) {
                    long theAvailable = 0;
                    thePreviousScenario = gU4E5B98;
                    this->GetCompletedScenarios(theCompleted);
                    if (std::find(theCompleted.begin(), theCompleted.end(), inScenario) == theCompleted.end())
                        theCompleted.push_back(inScenario);
                    for (unsigned long i = 0; i < theCompleted.size(); i++) {
                        if (i > 0) {
                            thePreviousScenario = theCompleted[i - 1];
                            theAvailable = this->mCivPointsAvailable[i - 1];
                        }
                        if (_mbsicmp(reinterpret_cast<const unsigned char*>((const char*)theCompleted[i]),
                                      reinterpret_cast<const unsigned char*>((const char*)inScenario)) == 0)
                            break;
                    }
                    if (thePreviousScenario.GetLength() > 0) {
                        this->MakeKey(theKey, inCampaign, thePreviousScenario, inChecksum);
                        FSMemoryFile* theCivilization;
                        theResult = theArchive->FindFile(gU5656B8 + "\\" + (const char*)theKey + gU5656C8,
                                                        theCivilization);
                        if (theResult == kErrorOK) {
                            theDestination = gStartupVolume + gStartupDirectory + "Users\\" + (const char*)inUser
                                             + "\\" + gU5656B8 + "\\" + (const char*)theKey + gU5656C8;
                            theResult = theCivilization->Copy(theDestination, false, true);
                            if (theResult == kErrorOK) {
                                theExtracted = true;
                                UString theName((const char*)theKey);
                                theResult = Fn2806E7(theName, gWorld.GetCurrentPlayer());
                                if (theResult == kErrorOK && theAvailable != 0)
                                    gWorld.GetCurrentPlayer()->mCivilization.AddPoints(theAvailable);
                            }
                        }
                    }
                }
                theArchive->Close();
                delete theArchive;
                delete theFile;
            } else {
                theResult = kErrorOK;
            }
            if (theExtracted) {
                FSFileSpec* theTemporary;
                if (thePath->FindFile(gU5656B8 + "\\" + (const char*)theKey + gU5656C8, theTemporary) == kErrorOK) {
                    theTemporary->Delete();
                    delete theTemporary;
                }
            }
        }
    }
    // The search path is released on every path (it was released only when a civilization was extracted).
    thePath->Dereference();
    return theResult;
}

// FUNCTION: EXE 0x68fb05
// Reads a player's progress archive for a campaign: completed scenarios, their civ points and saved civilizations
// (optionally extracted to the user's folder); clears the progress on error; a missing archive is no error
long EECampaignProgress::Load(const UWideString& inUser, const UWideString& inCampaign,
                              unsigned long inChecksum, bool inExtract)
{
    // The original constructs and destroys an otherwise unused empty vector here. Its element spelling
    // remains provisional: the shared empty-container lifetime alone does not establish that identity.
    std::vector<UWideString> theUnused;
    FSFileSpec theDirectory;
    UString theDestination;
    UWideString theChecksum;
    UWideString theKey;
    UWideString theScenario;
    UWideString theTag;
    UWideString thePoints;
    FSArchive* theArchive = NULL;
    FSPath* thePath = new FSPath;
    FSFileSpec* theFile;
    long theResult;
    long theAvailable;

    sprintf(theChecksum, "%d", inChecksum);
    theDirectory.SetVolume(gStartupVolume);
    theDirectory.SetDirectory(gStartupDirectory + "Users\\" + (const char*)inUser + "\\");
    thePath->AddDirectory(theDirectory, true);
    theResult = thePath->FindFile(UString((const char*)(inCampaign + theChecksum + (const char*)gU530B88)), theFile);
    if (theResult == kErrorOK) {
        theArchive = new FSArchive(*theFile);
        theResult = theArchive->Open();
        if (theResult == kErrorOK) {
            theArchive->GetTag(kEECampaignProgressNumScenariosCompletedKey, theScenario);
            this->mNumScenariosCompleted = atoi((const char*)theScenario);
            this->mCompletedScenarios.clear();
            for (unsigned long i = 0; i < (unsigned long)this->mNumScenariosCompleted; i++) {
                sprintf(theTag, "%s%.03d", (const char*)kEECampaignProgressScenarioNameKey, i);
                theArchive->GetTag(theTag, theScenario);
                this->MakeKey(theKey, inCampaign, theScenario, inChecksum);
                sprintf(theTag, "%s%.03d", (const char*)kEECampaignProgressNumCivPointsAvailableKey, i);
                theAvailable = theArchive->GetTag(theTag, thePoints) == kErrorOK ? atoi((const char*)thePoints) : 0;
                this->mCompletedScenarios.push_back(theScenario);
                this->mCivilizationKeys.push_back(theKey);
                this->mCivPointsAvailable.push_back(theAvailable);
                FSMemoryFile* theCivilization;
                theResult = theArchive->FindFile(gU5656B8 + "\\" + (const char*)theKey + gU5656C8, theCivilization);
                if (theResult != kErrorOK)
                    break;
                if (!inExtract)
                    continue;
                theDestination = gStartupVolume + gStartupDirectory + "Users\\" + (const char*)inUser + "\\"
                                 + gU5656B8 + "\\" + (const char*)theKey + gU5656C8;
                theResult = theCivilization->Copy(theDestination, false, true);
                if (theResult != kErrorOK)
                    break;
            }
        }
        theArchive->Close();
        delete theFile;
    } else {
        theResult = kErrorOK;
    }
    if (theResult != kErrorOK) {
        this->mCivilizationKeys.clear();
        this->mCompletedScenarios.clear();
        this->mCivPointsAvailable.clear();
        this->mNumScenariosCompleted = 0;
    }
    delete theArchive;
    thePath->Dereference();
    return theResult;
}

// FUNCTION: EXE 0x68ffed
// Records a won scenario: saves the current civilization, then rewrites the player's progress archive with every
// completed scenario, its civ points and civilization file; returns the first error
long EECampaignProgress::SaveCompletedScenario(const UWideString& inUser, const UWideString& inCampaign,
                                  const UWideString& inScenario, unsigned long inChecksum)
{
    std::vector<UString> theArchiveFiles;
    std::vector<UString> theTemporaryFiles;
    FSFileSpec theDirectory;
    UWideString theChecksum;
    UWideString theKey;
    UWideString theCount;
    UWideString theTag;
    UWideString thePoints;
    UString theFileName;
    FSPath* thePath = new FSPath;
    bool theAlreadyCompleted = false;
    long theResult;

    this->MakeKey(theKey, inCampaign, inScenario, inChecksum);
    sprintf(theChecksum, "%d", inChecksum);
    theDirectory.SetVolume(gStartupVolume);
    theDirectory.SetDirectory(gStartupDirectory + "Users\\" + (const char*)inUser + "\\");
    thePath->AddDirectory(theDirectory, true);
    long theAvailable = gWorld.GetCurrentPlayer()->mCivilization.GetAvailablePoints();
    if (std::find(this->mCompletedScenarios.begin(), this->mCompletedScenarios.end(), inScenario) != this->mCompletedScenarios.end())
        theAlreadyCompleted = true;
    Fn280BB1();
    theResult = Fn28015D(UString((const char*)theKey), true);
    if (theResult == kErrorOK) {
        if (!theAlreadyCompleted) {
            this->mNumScenariosCompleted++;
            this->mCompletedScenarios.push_back(inScenario);
            this->mCivilizationKeys.push_back(theKey);
            this->mCivPointsAvailable.push_back(theAvailable);
        }
        theDirectory.SetFile(UString((const char*)(inCampaign + theChecksum + (const char*)gU530B88)));
        FSArchive* theArchive = new FSArchive(theDirectory);
        sprintf(theCount, "%d", this->mNumScenariosCompleted);
        theArchive->SetTag(kEECampaignProgressChecksumKey, theChecksum);
        theArchive->SetTag(kEECampaignProgressNumScenariosCompletedKey, theCount);
        theArchiveFiles.clear();
        theTemporaryFiles.clear();
        bool theComplete = true;
        for (unsigned long i = 0; i < (unsigned long)this->mNumScenariosCompleted; i++) {
            sprintf(theTag, "%s%.03d", (const char*)kEECampaignProgressScenarioNameKey, i);
            theArchive->SetTag(theTag, this->mCompletedScenarios[i]);
            sprintf(theTag, "%s%.03d", (const char*)kEECampaignProgressNumCivPointsAvailableKey, i);
            long theValue;
            if (theAlreadyCompleted && _mbsicmp(reinterpret_cast<const unsigned char*>((const char*)this->mCompletedScenarios[i]),
                                               reinterpret_cast<const unsigned char*>((const char*)inScenario)) == 0)
                theValue = theAvailable;
            else
                theValue = this->mCivPointsAvailable[i];
            sprintf(thePoints, "%d", theValue);
            theArchive->SetTag(theTag, thePoints);
            theFileName = gU5656B8 + "\\" + (const char*)this->mCivilizationKeys[i] + gU5656C8;
            FSFileSpec* theFile;
            if (thePath->FindFile(theFileName, theFile) != kErrorOK) {
                theComplete = false;
                break;
            }
            theArchiveFiles.push_back(theFileName);
            theTemporaryFiles.push_back(theFileName);
            delete theFile;
        }
        if (theComplete)
            theResult = theArchive->Create(theDirectory, theArchiveFiles, 0x1000);
        for (std::vector<UString>::iterator it = theTemporaryFiles.begin(); it != theTemporaryFiles.end(); ++it) {
            FSFileSpec* theFile;
            if (thePath->FindFile(*it, theFile) == kErrorOK) {
                theFile->Delete();
                delete theFile;
            }
        }
        thePath->Dereference();
        theArchive->Close();
        delete theArchive;
    }
    delete gEditableCivilization;
    gEditableCivilization = NULL;
    return theResult;
}

// SYNTHETIC: EXE 0x68f39a
// SYNTHETIC: EXE 0x68f3a4
// SYNTHETIC: EXE 0x68f3b5
// SYNTHETIC: EXE 0x68f3c1
// SYNTHETIC: EXE 0x68f3cc
// SYNTHETIC: EXE 0x68f3d6
// SYNTHETIC: EXE 0x68f3e7
// SYNTHETIC: EXE 0x68f3f3
// SYNTHETIC: EXE 0x68f3fe
// SYNTHETIC: EXE 0x68f408
// SYNTHETIC: EXE 0x68f419
// SYNTHETIC: EXE 0x68f425
// SYNTHETIC: EXE 0x68f430
// SYNTHETIC: EXE 0x68f43a
// SYNTHETIC: EXE 0x68f44b
// SYNTHETIC: EXE 0x68f457

// The campaign civilizations' file name parts (their init chains 0x690514/0x690546 close the unit, so they are
// defined after its last function)
UString gU5656C8(".civ");
UString gU5656B8("Civilizations");
// SYNTHETIC: EXE 0x690514
// SYNTHETIC: EXE 0x69051e
// SYNTHETIC: EXE 0x69052f
// SYNTHETIC: EXE 0x69053b
// SYNTHETIC: EXE 0x690546
// SYNTHETIC: EXE 0x690550
// SYNTHETIC: EXE 0x690561
// SYNTHETIC: EXE 0x69056d
