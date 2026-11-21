// C:\EEX\Empire Earth\Campaign\EECampaignProgress.cpp (file name inferred: no __FILE__). A player's
// progress through a campaign, kept in the "Users\<player>\" archive: 0x28f39a..0x290514 (the four tag keys' init
// chains, then the members). Unit flags: the exe's module set. The progress is Rust's (ee_game campaign.rs, world lane
// round 1).
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
#include "ee/EECivilizationBuilder.h"
#include "ee/EECivilization.h"

#include <memory>
#include "ee/EEStringIDs.h"
#include "rust/game/campaign.h"

// UString globals used before their definitions (gU5656B8/gU5656C8 are defined at the end of this file, gU530B88 in
// Empire Earth.cpp); no class/static/const owner is claimed (provisional names)
extern UString gU5656B8; // original UString("Civilizations")
extern UString gU5656C8; // original UString(".civ")
extern UString gU530B88; // existing Empire Earth.cpp archive extension object
// cdecl civilization helpers (the original's callers push zero and two arguments; the second returns an error code)
void CivBuilderEditCurrentCivilization();
long CivBuilderSaveCivilization(const UString& inName, bool inCampaign);
// Original cdecl scenario-civilization loader (EEAResetAllPlayerInfo, 0x2806e7).
long EEApplyCivilizationFile(UString& inName, EEPlayer* inPlayer);

// The progress is Rust's whole (ee_game campaign.rs, world lane round 1: the lists, Load, RestoreCivilization and
// SaveCompletedScenario); the class is its handle
EECampaignProgress::EECampaignProgress() : mRust(ee_campaign_progress_new()) {}
EECampaignProgress::~EECampaignProgress() { ee_campaign_progress_free(this->mRust); }
void EECampaignProgress::GetCompletedScenarios(std::vector<UWideString>& outScenarios)
    { ee_campaign_progress_completed(this->mRust, &outScenarios); }
long EECampaignProgress::RestoreCivilization(const UWideString& inUser, const UWideString& inCampaign,
                                             const UWideString& inScenario, unsigned long inChecksum)
    { return ee_campaign_progress_restore(this->mRust, (const char*)inUser, (const char*)inCampaign, (const char*)inScenario, inChecksum); }
long EECampaignProgress::Load(const UWideString& inUser, const UWideString& inCampaign, unsigned long inChecksum, bool inExtract)
    { return ee_campaign_progress_load(this->mRust, (const char*)inUser, (const char*)inCampaign, inChecksum, inExtract); }
long EECampaignProgress::SaveCompletedScenario(const UWideString& inUser, const UWideString& inCampaign,
                                               const UWideString& inScenario, unsigned long inChecksum)
    { return ee_campaign_progress_save(this->mRust, (const char*)inUser, (const char*)inCampaign, (const char*)inScenario, inChecksum); }

// A text into the Rust side's sink
static void EECampaignText(int64_t inSink, const char* inText)
{
    ee_campaign_text_push((EeCampaignText*)inSink, (const uint8_t*)inText, strlen(inText));
}

// The player's folder "<startup>Users\<player>\", a file in it
static FSFileSpec* EECampaignUserSpec(const char* inUser, const char* inFile)
{
    FSFileSpec* theSpec = new FSFileSpec;
    theSpec->SetVolume(gStartupVolume);
    theSpec->SetDirectory(gStartupDirectory + "Users\\" + inUser + "\\");
    theSpec->SetFile(UString(inFile));
    return theSpec;
}

// An archive tag's value (left as it was when the tag is missing) into the sink -> GetTag's result
static long EECampaignGetTag(FSArchive* inArchive, const char* inName, const char* inValue, int64_t inSink)
{
    UWideString theValue(inValue);
    long theResult = inArchive->GetTag(UWideString(inName), theValue);
    EECampaignText(inSink, (const char*)theValue);
    return theResult;
}

// The scenario's civilization file applied to the current player
static long EECampaignApplyCivilization(const char* inKey)
{
    UString theName(inKey);   // named: the loader takes a non-const reference
    return EEApplyCivilizationFile(theName, gWorld.GetCurrentPlayer());
}

// A line of the campaign's scenario list into the sink -> UString::Load's result
static long EECampaignLineLoad(FSMemoryFile* inList, int64_t inSink)
{
    UString theLine;
    long theResult = theLine.Load(*inList);
    EECampaignText(inSink, (const char*)theLine);
    return theResult;
}

// A scenario name without its extension (the last four characters) at the campaign's list's end
static void EECampaignPushStripped(EECampaign* inCampaign, const char* inScenario)
{
    UWideString theScenario(inScenario);
    if (theScenario.GetLength() > 4)
        theScenario.InsertString("", theScenario.GetLength() - 4, true);
    inCampaign->mScenarios.insert(inCampaign->mScenarios.end(), theScenario);
}

// "The scenario %s didn't exist on disk"
static void EECampaignScenarioMissing(UWideString& outError, const char* inScenario)
{
    UWideString theFormat;
    gResource->GetString(IDS_SCENARIO_DIDNT_EXIST_ON_DISK, theFormat);
    sprintf(outError, (const char*)theFormat, inScenario);
}

// The progress' calls (ee_game campaign.rs EeCampaignCpp): p the object named, a the integers
extern "C" int64_t ee_campaigncpp(EeCampaignCpp inOp, uintptr_t p, const int64_t* a)
{
    typedef EeCampaignCpp O;
    switch (inOp) {
    case O::StartupVolume: EECampaignText(a[0], (const char*)gStartupVolume);
        break;
    case O::StartupDirectory: EECampaignText(a[0], (const char*)gStartupDirectory);
        break;
    case O::ArchiveExtension: EECampaignText(a[0], (const char*)gU530B88);
        break;
    case O::PathNew: return (int64_t)(intptr_t)new FSPath;
    case O::PathDereference: ((FSPath*)p)->Dereference();
        break;
    case O::PathAddUserDirectory: ((FSPath*)p)->AddDirectory(*std::unique_ptr<FSFileSpec>(EECampaignUserSpec((const char*)a[0], "")), true);
        break;
    case O::PathFindFile: return ((FSPath*)p)->FindFile(UString((const char*)a[0]), *(FSFileSpec**)a[1]);
    case O::SpecNewUser: return (int64_t)(intptr_t)EECampaignUserSpec((const char*)a[0], (const char*)a[1]);
    case O::SpecDelete: delete (FSFileSpec*)p;
        break;
    case O::SpecRemove: ((FSFileSpec*)p)->Delete();
        break;
    case O::ArchiveNew: return (int64_t)(intptr_t)new FSArchive(*(FSFileSpec*)p);
    case O::ArchiveOpen: return ((FSArchive*)p)->Open();
    case O::ArchiveClose: ((FSArchive*)p)->Close();
        break;
    case O::ArchiveDelete: delete (FSArchive*)p;
        break;
    case O::ArchiveGetTag: return EECampaignGetTag((FSArchive*)p, (const char*)a[0], (const char*)a[1], a[2]);
    case O::ArchiveSetTag: return ((FSArchive*)p)->SetTag(UWideString((const char*)a[0]), UWideString((const char*)a[1]));
    case O::ArchiveFindFile: return ((FSArchive*)p)->FindFile(UString((const char*)a[0]), *(FSMemoryFile**)a[1]);
    case O::ArchiveCreate: return ((FSArchive*)p)->Create(*(FSFileSpec*)a[0], *(std::vector<UString>*)a[1], 0x1000);
    case O::MemoryCopy: return ((FSMemoryFile*)p)->Copy(UString((const char*)a[0]), false, true);
    case O::NamesNew: return (int64_t)(intptr_t)new std::vector<UString>;
    case O::NamesPush: ((std::vector<UString>*)p)->push_back(UString((const char*)a[0]));
        break;
    case O::NamesDelete: delete (std::vector<UString>*)p;
        break;
    case O::WidePush: ((std::vector<UWideString>*)p)->push_back(UWideString((const char*)a[0]));
        break;
    case O::WideClear: ((std::vector<UWideString>*)p)->clear();
        break;
    case O::MbsICmp: return PlatMbsICmp(reinterpret_cast<const unsigned char*>((const char*)a[0]), reinterpret_cast<const unsigned char*>((const char*)a[1]));
    case O::BuiltInCampaign: return EECampaignFormManager::GetBuiltInCampaign(UWideString((const char*)a[0]));
    case O::AvailablePoints: return gWorld.GetCurrentPlayer()->mCivilization.GetAvailablePoints();
    case O::AddPoints: gWorld.GetCurrentPlayer()->mCivilization.AddPoints((unsigned long)a[0]);
        break;
    case O::CivBuilderEdit: CivBuilderEditCurrentCivilization();
        break;
    case O::CivBuilderSave: return CivBuilderSaveCivilization(UString((const char*)a[0]), true);
    case O::CivBuilderFree: CivBuilderFreeCivilization();   // the edited civilization deleted (ee_screens civ_whole.rs)
        break;
    case O::ApplyCivilization: return EECampaignApplyCivilization((const char*)a[0]);
    // EECampaign::Load (p: the campaign, or the UWideString / file named)
    case O::MemberRequiredCiv: return (int64_t)(intptr_t)&((EECampaign*)p)->mRequiredCiv;
    case O::MemberBackgroundImage: return (int64_t)(intptr_t)&((EECampaign*)p)->mBackgroundImage;
    case O::MemberCampaignName: return (int64_t)(intptr_t)&((EECampaign*)p)->mCampaignName;
    case O::MemberDescription: return (int64_t)(intptr_t)&((EECampaign*)p)->mDescription;
    case O::MemberAuthorName: return (int64_t)(intptr_t)&((EECampaign*)p)->mAuthorName;
    case O::MemberAuthorEmail: return (int64_t)(intptr_t)&((EECampaign*)p)->mAuthorEmail;
    case O::MemberPassword: return (int64_t)(intptr_t)&((EECampaign*)p)->mPassword;
    case O::WideAssign: *(UWideString*)p = (const char*)a[0];
        break;
    case O::DataPathFindFile: return gDataPath->FindFile(UString((const char*)a[0]), *(FSFileSpec**)a[1]);
    case O::DataPathExists: return gDataPath->FindFile(UWideString((const char*)a[0])) == kErrorOK;
    case O::ArchiveGetTagInto: return ((FSArchive*)p)->GetTag(UWideString((const char*)a[0]), *(UWideString*)a[1]);
    case O::LineLoad: return EECampaignLineLoad((FSMemoryFile*)p, a[0]);
    case O::MemoryDelete: delete (FSMemoryFile*)p;
        break;
    case O::Decrypt: *(UWideString*)a[1] = ((EECampaign*)p)->DecryptPassword(UWideString((const char*)a[0]));
        break;
    case O::ScenarioListFile: EECampaignText(a[0], (const char*)((EECampaign*)p)->GetScenarioListFile());
        break;
    case O::ScenariosClear: ((EECampaign*)p)->mScenarios.clear();
        break;
    case O::ScenariosPushStripped: EECampaignPushStripped((EECampaign*)p, (const char*)a[0]);
        break;
    case O::ErrorReadingTag: gResource->GetString(IDS_THERE_WAS_ERROR_READING_TAG, *(UWideString*)p);
        break;
    case O::ErrorCouldntOpen: gResource->GetString(IDS_FILE_COULDNT_BE_OPENED, *(UWideString*)p);
        break;
    case O::ErrorScenarioMissing: EECampaignScenarioMissing(*(UWideString*)p, (const char*)a[0]);
        break;
    }
    return 0;
}

// The campaign civilizations' file name parts (their init chains 0x690514/0x690546 close the unit, so they are
// defined after its last function)
UString gU5656C8(".civ");
UString gU5656B8("Civilizations");
