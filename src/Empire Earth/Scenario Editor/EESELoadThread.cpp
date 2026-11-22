// C:\EEX\Empire Earth\Scenario Editor\EESELoadThread.cpp (__FILE__ 0x4c2f70, the ctor's ESVERIFYs). The scenario
// editor's loading thread: 0x172125..0x172b0f (ctor, the inline EESEAChangeLoadText members it is the first user of,
// dtor, the handlers, Main, HandleMessage, the posting wrappers). After EESEInstructions.cpp (..0x172125), before the
// next unit's globals chain 0x172b0f. Unit flags: the exe module set. The bodies (and the text actions' of
// EESEIAExport.cpp) are Rust's (ee_game screens editor_selt.rs); this unit is their single statements.
#include <windows.h>
#include <string>

#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GESurface.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GEScreen.h"
#include "lle/GEViewport.h"
#include "lle/GEVertexArray.h"

#include "lle/GERhombus.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"

#include "lle/GETerrainMesh.h"
#include "lle/UResourceFile.h"

#include "ee/EmpireEarth.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEMap.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEAction.h"
#include "ee/EEUIManager.h"
#include "ee/EEUserInterface.h"
#include "ee/EEUIFScenarioEditor.h"
#include "ee/EEARust.h"
#include "ee/EESELoadThread.h"
#include "ee/EESEUnit.h"
#include "ee/EEGhostUnit.h"
#include "ee/EEDatabaseTables.h"
#include "ee/EEStringIDs.h"
#include "ee/EESEIAExport.h"
#include "ee/EEScenarioManager.h"
#include "rust/game/screens.h"
EESELoadThread::EESELoadThread(EEUIFScenarioEditor* inEditor, long inThreadPriority)
{
    ee_screens_selt_construct(this, inEditor, inThreadPriority);
}

EESELoadThread::~EESELoadThread()
{
    ee_screens_selt_destroy(this);
}

unsigned long EESELoadThread::Main(void* inThis)
{
    return ee_screens_selt_main(inThis);
}

void EESELoadThread::HandleMessage(unsigned long inMessage, void* inParameter1, void* inParameter2)
{
    ee_screens_selt_message(this, inMessage, inParameter1, inParameter2);
}


void EESEImportTextAnswer(UIForm* inInfo);   // the questions' answers (EESEIAExport.cpp)
void EESEExportTextAnswer(UIForm* inFile);

namespace {

EESELoadThread* T(const uintptr_t* p) { return reinterpret_cast<EESELoadThread*>(p[0]); }
EEPlayer* P(const uintptr_t* p) { return reinterpret_cast<EEPlayer*>(p[1]); }
EETerrainTable* R(const uintptr_t* p) { return reinterpret_cast<EETerrainTable*>(p[1]); }
EEUnitClass* K(const uintptr_t* p) { return reinterpret_cast<EEUnitClass*>(p[1]); }
FSFileSpec* S(const uintptr_t* p) { return reinterpret_cast<FSFileSpec*>(p[1]); }
EESEImportInfo* I(const uintptr_t* p) { return reinterpret_cast<EESEImportInfo*>(p[1]); }
void** Q(const uintptr_t* p) { return reinterpret_cast<void**>(p[2]); }
EEUIManager* M(const uintptr_t* p) { return static_cast<EEUIManager*>(reinterpret_cast<UIForm*>(p[0])->GetManager()); }
int64_t H(const void* inObject) { return (int64_t)(uintptr_t)inObject; }
// the statement's text (p2, a2 bytes)
UWideString Text(const uintptr_t* p, const int64_t* a) { return UWideString(std::string((const char*)p[2], (size_t)a[2]).c_str()); }
EEUIFErrorCallback Answer(const int64_t* a) { return a[0] != 0 ? EESEImportTextAnswer : EESEExportTextAnswer; }

// a text into the caller's buffer (p2, a3 bytes): its whole length
int64_t Write(const UWideString& inText, const uintptr_t* p, const int64_t* a) { return EESEWrite((void*)p[2], (int32_t)a[3], inText); }

}   // namespace

// the load thread's and the text actions' single statements (editor_selt.rs EE_SCREENS_SELTX_*)
extern "C" int64_t ee_screenscpp_seltx(uint32_t inOp, const uintptr_t* p, const int64_t* a)
{
    switch (inOp) {
    case EE_SCREENS_SELTX_SET_DONE: T(p)->mIsDone = a[0] != 0;
        return 0;
    case EE_SCREENS_SELTX_SET_EDITOR: T(p)->mEditor = reinterpret_cast<EEUIFScenarioEditor*>(p[1]);
        return 0;
    case EE_SCREENS_SELTX_CREATE_EVENT_CREATED: ESVERIFY_AT(TSScheduler::CreateEvent(T(p)->mThreadCreated) == keseOK, 80);
        return 0;
    case EE_SCREENS_SELTX_CREATE_EVENT_DONE: ESVERIFY_AT(TSScheduler::CreateEvent(T(p)->mThreadDone) == keseOK, 81);
        return 0;
    case EE_SCREENS_SELTX_CREATE_EVENT_QUEUE: ESVERIFY_AT(TSScheduler::CreateEvent(T(p)->mMessageQueueDone) == keseOK, 82);
        return 0;
    case EE_SCREENS_SELTX_CREATE_THREAD:
        ESVERIFY_AT(TSScheduler::CreateThread(T(p)->mThread, EESELoadThread::Main, (void*)T(p)) == keseOK, 85);
        return 0;
    case EE_SCREENS_SELTX_SET_PRIORITY: ESVERIFY_AT(T(p)->mThread->SetPriority((long)a[0]) == keseOK, 86);
        return 0;
    case EE_SCREENS_SELTX_WAIT_CREATED: T(p)->mThreadCreated->Wait(kTSWaitInfinite);
        return 0;
    case EE_SCREENS_SELTX_SET_NAME: T(p)->mThread->SetName(gScenarioEditorThreadName);
        return 0;
    case EE_SCREENS_SELTX_NEW_LOAD_TEXT: T(p)->mChangeLoadText = new EESEAChangeLoadText;
        return 0;
    case EE_SCREENS_SELTX_LOAD_TEXT_RELEASE: T(p)->mChangeLoadText->Dereference();
        return 0;
    case EE_SCREENS_SELTX_DESTROY_EVENT_CREATED: TSScheduler::DestroyEvent(T(p)->mThreadCreated);
        return 0;
    case EE_SCREENS_SELTX_DESTROY_EVENT_DONE: TSScheduler::DestroyEvent(T(p)->mThreadDone);
        return 0;
    case EE_SCREENS_SELTX_DESTROY_EVENT_QUEUE: TSScheduler::DestroyEvent(T(p)->mMessageQueueDone);
        return 0;
    case EE_SCREENS_SELTX_DESTROY_THREAD: TSScheduler::DestroyThread(T(p)->mThread);
        return 0;
    case EE_SCREENS_SELTX_SIGNAL_CREATED: T(p)->mThreadCreated->Set();
        return 0;
    case EE_SCREENS_SELTX_PRIORITY_ABOVE_NORMAL: T(p)->mThread->SetPriority(PLAT_PRIORITY_ABOVE_NORMAL);
        return 0;
    case EE_SCREENS_SELTX_IS_DONE: return T(p)->mIsDone;
    case EE_SCREENS_SELTX_RECEIVE: return T(p)->mThread->Receive(*reinterpret_cast<unsigned long*>(p[1]), Q(p)[0], Q(p)[1]);
    case EE_SCREENS_SELTX_DELETE: delete T(p);
        return 0;
    case EE_SCREENS_SELTX_LEAVE_EDITOR: gUserInterface->LeaveScenarioEditor();
        return 0;
    case EE_SCREENS_SELTX_START_TEST: gUserInterface->StartScenarioTest();
        return 0;
    case EE_SCREENS_SELTX_START_TEST2: gUserInterface->StartScenarioTest2();
        return 0;
    case EE_SCREENS_SELTX_POST_EVENT: gUserInterface->PostEvent(reinterpret_cast<UIEvent*>(p[1]), NULL);
        return 0;
    case EE_SCREENS_SELTX_DEREFERENCE: reinterpret_cast<UReferenceCounter*>(p[1])->Dereference();
        return 0;
    case EE_SCREENS_SELTX_SHOW_LOADED: gUserInterface->ShowLoadedObject(reinterpret_cast<UReferenceCounter*>(p[1]));
        return 0;
    case EE_SCREENS_SELTX_QUEUE_DONE: T(p)->mMessageQueueDone->Set();
        return 0;
    case EE_SCREENS_SELTX_READ_LOCK: gWorld.mLock->ReadLock();
        return 0;
    case EE_SCREENS_SELTX_READ_UNLOCK: gWorld.mLock->ReadUnlock();
        return 0;
    case EE_SCREENS_SELTX_WRITE_LOCK: gWorld.mLock->WriteLock();
        return 0;
    case EE_SCREENS_SELTX_WRITE_UNLOCK: gWorld.mLock->WriteUnlock();
        return 0;
    case EE_SCREENS_SELTX_PLAYER: return H(gWorld.mPlayers[a[0]]);
    case EE_SCREENS_SELTX_POST_TEXT: T(p)->mChangeLoadText->Post(Text(p, a));
        return 0;
    case EE_SCREENS_SELTX_LOAD_ALL_MEDIA: P(p)->LoadAllClassMedia();
        return 0;
    case EE_SCREENS_SELTX_UNLOAD_ALL_MEDIA: P(p)->UnloadAllClassMedia();
        return 0;
    case EE_SCREENS_SELTX_TECH_LOAD_MEDIA: P(p)->mTechnologies->Get((unsigned long)a[0])->LoadMedia(P(p));
        return 0;
    case EE_SCREENS_SELTX_TECH_UNLOAD_MEDIA: P(p)->mTechnologies->Get((unsigned long)a[0])->UnloadMedia(P(p));
        return 0;
    case EE_SCREENS_SELTX_TERRAIN: return H(EETerrainTable::Get((unsigned long)a[0]));
    case EE_SCREENS_SELTX_MAP_EXISTS: return gWorld.mMap != NULL;
    case EE_SCREENS_SELTX_TERRAIN_NAME_ID: return EETerrainTable::Get((unsigned long)a[0])->mNameID;
    case EE_SCREENS_SELTX_TERRAIN_LOAD: R(p)->Load(gWorld.mMap->mTerrainMesh);
        return 0;
    case EE_SCREENS_SELTX_TERRAIN_UNLOAD: R(p)->Unload(gWorld.mMap->mTerrainMesh);
        return 0;
    case EE_SCREENS_SELTX_TERRAIN_CLASSES: return (int64_t)R(p)->GetNumUnitClasses();
    case EE_SCREENS_SELTX_TERRAIN_CLASS: return (int64_t)R(p)->GetUnitClass((unsigned long)a[0]);
    case EE_SCREENS_SELTX_CLASS: return H(gUnitClassTable[a[0]]);
    case EE_SCREENS_SELTX_SET_CLASS_MEDIA: P(p)->SetUnitClassMedia((long)a[0], 0x7f);
        return 0;
    case EE_SCREENS_SELTX_LOAD_CLASS_BUTTONS: P(p)->LoadUnitClassButtons((long)a[0]);
        return 0;
    case EE_SCREENS_SELTX_UNLOAD_CLASS_MEDIA: P(p)->UnloadUnitClassMedia((long)a[0]);
        return 0;
    case EE_SCREENS_SELTX_UNLOAD_CLASS_BUTTONS: P(p)->UnloadUnitClassButtons((long)a[0]);
        return 0;
    case EE_SCREENS_SELTX_CLASS_NAME_ID: return K(p)->mNameStringID;
    case EE_SCREENS_SELTX_CLASS_TYPE: return K(p)->mUnitType;
    case EE_SCREENS_SELTX_CLASS_LAND_BUILDING: return K(p)->IsLandBuilding();
    case EE_SCREENS_SELTX_GHOST: EEGhostUnit::Create(T(p)->mEditor->GetMouseLocation(), (long)a[0], P(p),
                            a[1] != 0 ? ee_screens_seun_angle(T(p)->mEditor->GetUnitTab()) : 0.0f, false);
        return 0;
    case EE_SCREENS_SELTX_RELOAD_TECH: P(p)->ReloadTechTree(a[0] != 0);
        return 0;
    case EE_SCREENS_SELTX_FLUSH_PLACEMENTS: gWorld.FlushQueuedPlacements();
        return 0;
    case EE_SCREENS_SELTX_ADD_PENDING: P(p)->AddPendingUnits(0, 0);
        return 0;
    case EE_SCREENS_SELTX_REMOVE_KILLED: P(p)->RemoveKilledUnits();
        return 0;
    case EE_SCREENS_SELTX_RELEASE_PENDING: P(p)->ReleasePendingUnits();
        return 0;
    case EE_SCREENS_SELTX_EXPORT_FILE: return H(&reinterpret_cast<EESEIAExport*>(p[0])->mFile);
    case EE_SCREENS_SELTX_IMPORT_FILE: return H(&reinterpret_cast<EESEIAImport*>(p[0])->mFile);
    case EE_SCREENS_SELTX_IMPORT_VALIDATE: return reinterpret_cast<EESEIAImport*>(p[0])->mValidate == true;
    case EE_SCREENS_SELTX_FOPEN_W: return S(p)->FOpen("w");
    case EE_SCREENS_SELTX_FOPEN_R: return S(p)->FOpen("r");
    case EE_SCREENS_SELTX_CLOSE: S(p)->Close();
        return 0;
    case EE_SCREENS_SELTX_FEOF: return feof((FILE*)*S(p)) != 0;
    case EE_SCREENS_SELTX_FGETS: return fgets(reinterpret_cast<char*>(p[2]), (int)a[3], (FILE*)*S(p)) != NULL;
    case EE_SCREENS_SELTX_REFRESH_GROUP: gUIManager->GetScenarioEditorForm()->RefreshCurrentGroup();
        return 0;
    case EE_SCREENS_SELTX_RESUME_GROUP: gUIManager->GetScenarioEditorForm()->ResumeCurrentGroup();
        return 0;
    case EE_SCREENS_SELTX_WRITE_HEADER: return (int64_t)(unsigned long)fprintf((FILE*)*S(p), "{Version_%ld}\n\n", gSETextVersion);
    case EE_SCREENS_SELTX_TEXT_VERSION: return (int64_t)gSETextVersion;
    case EE_SCREENS_SELTX_EXPORT_TEXT: return gWorld.mScenarioManager->ExportText(*S(p));
    case EE_SCREENS_SELTX_IMPORT_TEXT: return gWorld.mScenarioManager->ImportTriggerText(*S(p), (unsigned long)a[0], a[1] != 0);
    case EE_SCREENS_SELTX_ERROR_COUNT: return (int64_t)gWorld.mScenarioManager->mImportErrors.size();
    case EE_SCREENS_SELTX_ERROR: return Write(gWorld.mScenarioManager->mImportErrors[(size_t)a[0]], p, a);
    case EE_SCREENS_SELTX_NAME_CHANGED: return UWideString(S(p)->GetFile()) != gWorld.mScenarioManager->mExportFileName;
    case EE_SCREENS_SELTX_SET_EXPORT_NAME: gWorld.mScenarioManager->mExportFileName = UWideString(S(p)->GetFile());
        return 0;
    case EE_SCREENS_SELTX_MESSAGE: gUIManager->DisplayMessage(Text(p, a), 1, NULL, NULL, true);
        return 0;
    case EE_SCREENS_SELTX_FORM_MESSAGE: M(p)->DisplayMessage(Text(p, a), 1, NULL, NULL, true);
        return 0;
    case EE_SCREENS_SELTX_QUESTION: gUIManager->DisplayQuestion(Text(p, a), 1, reinterpret_cast<UIForm*>(p[1]), Answer(a), NULL);
        return 0;
    case EE_SCREENS_SELTX_FORM_QUESTION: M(p)->DisplayQuestion(Text(p, a), 1, reinterpret_cast<UIForm*>(p[1]), Answer(a), NULL);
        return 0;
    case EE_SCREENS_SELTX_SPEC_NEW: return H(new FSFileSpec);
    case EE_SCREENS_SELTX_SPEC_COPY: return H(new FSFileSpec(*S(p)));
    case EE_SCREENS_SELTX_SPEC_DELETE: delete S(p);
        return 0;
    case EE_SCREENS_SELTX_SPEC_VOLUME: S(p)->SetVolume(gStartupVolume);
        return 0;
    case EE_SCREENS_SELTX_SPEC_DIRECTORY: S(p)->SetDirectory(gStartupDirectory + "data\\" + "Scenarios\\");
        return 0;
    case EE_SCREENS_SELTX_SPEC_FILE: S(p)->SetFile((const char*)Text(p, a));
        return 0;
    case EE_SCREENS_SELTX_INFO_NEW: return H(new EESEImportInfo{*S(p), a[0] != 0});
    case EE_SCREENS_SELTX_INFO_DELETE: delete I(p);
        return 0;
    case EE_SCREENS_SELTX_POST_EXPORT: gUserInterface->PostAction(new EESEIAExport(*S(p)));
        return 0;
    case EE_SCREENS_SELTX_POST_IMPORT: gUserInterface->PostAction(new EESEIAImport(I(p)->mFile, I(p)->mValidate));
        return 0;
    }
    return 0;
}
