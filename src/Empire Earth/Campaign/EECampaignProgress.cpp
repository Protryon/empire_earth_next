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
#include "lle/FSRestorable.h"
#include "lle/FSPersistent.h"
#include "ee/EESMMediaStrings.h"
#include "ee/EEPSaveGameHeader.h"
#include "rust/sim/scenario.h"
#include "ee/EEScenarioManager.h"
#include "ee/EESMEffect.h"
#include "ee/EEPersistentTemplates.h"
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

// A scenario name without its extension (the last four characters) at the campaign's list's end
static void EECampaignPushStripped(EECampaign* inCampaign, const char* inScenario)
{
    UWideString theScenario(inScenario);
    if (theScenario.GetLength() > 4)
        theScenario.InsertString("", theScenario.GetLength() - 4, true);
    inCampaign->mScenarios.insert(inCampaign->mScenarios.end(), theScenario);
}

// The built-in campaigns (EECampaignFormManager::GetBuiltInCampaign, ee_game campaign.rs built_in_campaign)
static const UWideString* const kEECampaignBuiltIns[] = {&gCampaignAOCRoman, &gCampaignAOCPacific, &gCampaignAOCAsian, &gCampaignEEDemo, &gCampaignEELearning};
static const long* const kEECampaignBuiltInChecksums[] = {&gAOCRomanChecksum, &gAOCPacificChecksum, &gAOCAsianChecksum, &gEEDemoChecksum, &gEELearningChecksum};

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
    case O::SelectForm: return (int64_t)(intptr_t)gUIManager->GetFormByID(kFormCampaignScenarioSelect);
    case O::FormManager: return (int64_t)(intptr_t)((UIForm*)p)->GetFormManager();
    case O::SetFormManager: ((UIForm*)p)->SetFormManager((UIFormManager*)a[0]);
        break;
    case O::NewFormManager: return (int64_t)(intptr_t)static_cast<UIFormManager*>(new EECampaignFormManager);
    case O::PathDereference: ((FSPath*)p)->Dereference();
        break;
    case O::PathAddDirectory: ((FSPath*)p)->AddDirectory(*(FSFileSpec*)a[0], true);
        break;
    case O::PathFindFile: return ((FSPath*)p)->FindFile(UString((const char*)a[0]), *(FSFileSpec**)a[1]);
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
    case O::ApplyCivilization: return EEApplyCivilizationFile(*(UString*)p, gWorld.GetCurrentPlayer());
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
    case O::LineNew: return (int64_t)(intptr_t)new UString((const char*)a[0]);
    case O::LineLoad: return ((UString*)p)->Load(*(FSMemoryFile*)a[0]);
    case O::LineText: EECampaignText(a[0], (const char*)*(UString*)p);
        break;
    case O::LineDelete: delete (UString*)p;
        break;
    case O::MemoryDelete: delete (FSMemoryFile*)p;
        break;
    case O::ScenariosClear: ((EECampaign*)p)->mScenarios.clear();
        break;
    case O::ScenariosPushStripped: EECampaignPushStripped((EECampaign*)p, (const char*)a[0]);
        break;
    case O::ErrorReadingTag: gResource->GetString(IDS_THERE_WAS_ERROR_READING_TAG, *(UWideString*)p);
        break;
    case O::ErrorCouldntOpen: gResource->GetString(IDS_FILE_COULDNT_BE_OPENED, *(UWideString*)p);
        break;
    // the editor side (ee_game campaign.rs campaign_save, list_scenario_files; p the object named)
    case O::WideText: EECampaignText(a[0], (const char*)*(UWideString*)p);
        break;
    case O::WideAppend: *(UWideString*)p += (const char*)a[0];
        break;
    case O::WideNew: return (int64_t)(intptr_t)new UWideString;
    case O::WideDelete: delete (UWideString*)p;
        break;
    case O::MemberDirectory: return (int64_t)(intptr_t)&((EECampaign*)p)->mDirectory;
    case O::MemberScenarios: return (int64_t)(intptr_t)&((EECampaign*)p)->mScenarios;
    case O::WideListNew: return (int64_t)(intptr_t)new std::vector<UWideString>;
    case O::WideListDelete: delete (std::vector<UWideString>*)p;
        break;
    case O::WideListLen: return (int64_t)((std::vector<UWideString>*)p)->size();
    case O::WideListAt: EECampaignText(a[0], (const char*)(*(std::vector<UWideString>*)p)[(size_t)a[1]]);
        break;
    case O::WideListContains: return std::find(((std::vector<UWideString>*)p)->begin(), ((std::vector<UWideString>*)p)->end(), UWideString((const char*)a[0])) != ((std::vector<UWideString>*)p)->end();
    case O::ScenarioExtension: EECampaignText(a[0], (const char*)gScenarioExtension);
        break;
    case O::CampaignExtension: EECampaignText(a[0], (const char*)gCampaignExtension);
        break;
    case O::ScenarioPrefix: EECampaignText(a[0], (const char*)gU52A280);
        break;
    case O::ResourceString: gResource->GetString((long)a[0], *(UWideString*)p);
        break;
    case O::Sprintf: sprintf(*(UWideString*)p, (const char*)*(UWideString*)a[0], (const char*)a[1], (const char*)a[2]);
        break;
    case O::SpecNew: return (int64_t)(intptr_t)new FSFileSpec;
    case O::SpecSetStartupVolume: ((FSFileSpec*)p)->SetVolume(gStartupVolume);
        break;
    case O::SpecSetDirectory: ((FSFileSpec*)p)->SetDirectory(UString((const char*)a[0]));
        break;
    case O::SpecSetFile: ((FSFileSpec*)p)->SetFile(UString((const char*)a[0]));
        break;
    case O::SpecCreate: return ((FSFileSpec*)p)->Create();
    case O::SpecOpenWrite: return ((FSFileSpec*)p)->Open(kFileAccessWrite);
    case O::SpecClose: return ((FSFileSpec*)p)->Close();
    case O::SpecCopy: return ((FSFileSpec*)p)->Copy(UString((const char*)a[0]), false, false);
    case O::SpecPathText: EECampaignText(a[0], (const char*)(((FSFileSpec*)p)->GetVolume() + ((FSFileSpec*)p)->GetDirectory() + ((FSFileSpec*)p)->GetFile()));
        break;
    case O::LineSave: return UString((const char*)a[0]).Save(*(FSFileSpec*)p);
    case O::ArchiveStatus: gDataPath->GetFileArchiveStatus(UWideString((const char*)a[0]), *(bool*)a[1]);
        break;
    case O::HeaderNew: return (int64_t)(intptr_t)new EEPSaveGameHeader;
    case O::HeaderLoad: return ((EEPSaveGameHeader*)p)->LoadFromFile(*(FSFileSpec*)a[0]);
    case O::HeaderListMedia: return EESMMediaStrings(((EEPSaveGameHeader*)p)->mMediaStrings).ListMediaFiles(UWideString((const char*)a[0]), *(std::vector<UWideString>*)a[1], *(UWideString*)a[2], a[3] != 0);
    case O::HeaderDelete: delete (EEPSaveGameHeader*)p;
        break;
    case O::PersistentLoad: return EELoadPersistent(UString((const char*)a[0]), 6, *(FSPersistent**)a[1]);
    case O::PersistentManager: return (int64_t)(intptr_t)(dynamic_cast<EEPTPRestorable<EEScenarioManager>*>((FSPersistent*)p) ? dynamic_cast<EEPTPRestorable<EEScenarioManager>*>((FSPersistent*)p)->mObject : NULL);
    case O::ManagerEffects: return (int64_t)((EEScenarioManager*)p)->mEffects.size();
    case O::ManagerEffect: return (int64_t)(intptr_t)((EEScenarioManager*)p)->mEffects[(size_t)a[0]];
    case O::EffectIsSound: return ((EESMEffect*)p)->mType == EE_SCENARIO_SM_EFFECT_MEDIA && (((EESMEffect*)p)->mMediaAction == EE_SCENARIO_SM_MEDIA_ACTION_PLAY_SOUND || ((EESMEffect*)p)->mMediaAction == EE_SCENARIO_SM_MEDIA_ACTION_SEND_DIALOGUE);
    case O::EffectFileName: EECampaignText(a[0], (const char*)((EESMEffect*)p)->mFileName);
        break;
    case O::PersistentFreeManager: delete std::exchange(dynamic_cast<EEPTPRestorable<EEScenarioManager>*>((FSPersistent*)p)->mObject, nullptr);
        break;
    case O::PersistentDelete: delete (FSPersistent*)p;
        break;
    // EECampaignFormManager (p: the manager)
    case O::FormCampaignName: return (int64_t)(intptr_t)&((EECampaignFormManager*)p)->mCampaignName;
    case O::FormSetChecksum: ((EECampaignFormManager*)p)->mChecksum = (unsigned long)a[0];
        break;
    case O::FileChecksum: return EEGetFileChecksum(*(FSFileSpec*)p, UString((const char*)a[0]), 0);
    case O::BuiltInName: EECampaignText(a[0], (const char*)*kEECampaignBuiltIns[a[1]]);
        break;
    case O::BuiltInChecksum: return *kEECampaignBuiltInChecksums[a[0]];
    }
    return 0;
}

// The campaign civilizations' file name parts (their init chains 0x690514/0x690546 close the unit, so they are
// defined after its last function)
UString gU5656C8(".civ");
UString gU5656B8("Civilizations");
