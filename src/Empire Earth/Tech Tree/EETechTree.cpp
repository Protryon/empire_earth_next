// C:\EEX\Empire Earth\Tech Tree\EETechTree.cpp (__FILE__ 0x4c5c08; RTTI EETechTree > RTSTechTree, vtables 0x446114 /
// 0x446120). A player's tech tree: 0x1cba2f..0x1cdf25 (the eight key chains and sUnitClassInfo's chain, the ctor ..
// EEEvent::GetEffect's COMDAT; EETechTreeEpoch.cpp's key chain 0x1cdf25 opens the next unit). Classes in
// ee/EEPlayer.h. Unit flags: the exe's module set.
// port (R5b.4, round 29; player/tech lane rounds 1 and 2): the tree's, epochs' and nodes' bodies are Rust (ee_sim tech
// nodes.rs, ee_game tech_tree) over the records the classes derive from; the classes call them inline
// (EETechTreeEpoch.cpp, EETechTreeNode.cpp 0x1cedce..0x1cfeef and the capture unit EETechCapture.cpp are gone). What is
// left: the members that need a complete class the header does not have, and the one-statement host cases of the
// tree's and the calamities' Rust.
#include <windows.h>

#include "lle/LowLevelEngine.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"
#include "lle/USingleton.h"

#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EECalamityManager.h"
#include "ee/EECalamityClass.h"
#include "ee/EECalamity.h"
#include "ee/EEContagionManager.h"
#include "ee/EECalamityUpdate.h"
#include "ee/EEGameVariant.h"
#include "ee/EEButtonObject.h"
#include "ee/EEStringIDs.h"
#include "ee/EEUnitSetManager.h"
#include "ee/EEDatabaseTables.h"
#include "ee/EEEvent.h"
#include "ee/EEMedia.h"
#include "ee/EESoundObject.h"
#include "ee/EEAEManager.h"
#include "ee/EmpireEarth.h"
#include "ee/EEUserInterface.h"
#include "ee/EEUIManager.h"
#include "ee/EEUIFMainGame.h"
#include "rust/game/gamecpp.h"

static_assert(kTechEpochPrehistoric == 0 && kTechNone == 100000 && kTechFirstNodeID == 1000 &&
              IDS_PREHISTORIC_AGE == 30145, "ee_game tech_tree load.rs");

// Returns the owner's tech tree node of the technology that makes the unit's class (was EECPEngineHelpers.cpp's:
// it reaches EETechTreeEpochOf and EETechTreeEpoch::Get through the inline EETechTree::Get)
EETechTreeNode* EEUnit::GetTechTreeNode()
{
    return this->GetPlayer()->GetTechTree()->Get(this->GetUnitClass()->GetTechnology());
}

// Adds a sync checksum of every tech's and epoch's researched/obsolete state, weighted by position (ee_tech
// tree_checksum)
void EETechTree::GetChecksum(ESChecksum& ioChecksum)
{
    ioChecksum.AddChecksum("EETechTree", (unsigned long)(uint32_t)this->Walk(EeTechTreeOp::Checksum));
}

#define P64(x) ((int64_t)(intptr_t)(x))
#define TVARIANT EEGameVariant::GetGameVariant(gWorld.mGameOptions.mVariant)
#define TREE ((EETechTree*)p)
#define TPL ((EEPlayer*)p)
#define TFILE ((FSFileSpec*)p)
#define TEFFECT ((EEEventEffect*)p)

// the cost factors (host.rs; EEPWorldHelper.cpp's default)
double EECore10OpAF(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f);   // EEGraphics.cpp
double EECore9OpD7F(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f)
{
    typedef EeGameCpp O;
    switch (inOp) {
    case O::TkEpochCostFactor: return gWorldTable.GetFloatValue(kWorldUnitEpochCostFactor);
    case O::TkCostSetFactor: return TVARIANT->GetCostSetFactor();
    case O::TkStatsCostScale: return ((EEUnitStats*)p)->GetCostScale();
    case O::TkJitFactor: return gWorld.mGameOptions.mResources == 4 ? gWorldTable.GetFloatValue(kWorldJitManufacturingCostFactorDeathmatch) : gWorldTable.GetFloatValue(kWorldJitManufacturingCostFactor);
    default: return EECore10OpAF(inOp, p, a, f);
    }
}

// the tech tree's and the calamity host's C++ calls and members for Rust (EEServer.cpp's EECore11OpC6 default): one
// statement each
int64_t EECore11OpC7(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f)
{
    typedef EeGameCpp O;
    switch (inOp) {
    // (EECore10OpB7) the tech tree's loads, copies, costs and sweep (gamecpp tech_load.rs, tech_capture.rs; p the node, the epoch, the
    // tree or the player): one statement each
    case O::TbButtonGet: return P64(EEButtonObject::Get((unsigned long)a[0]));
    case O::TbSwapButton: ((EEPlayer*)p)->SwapClassButtonMedia((long)a[0], (EEButtonObject*)a[1], (EEButtonObject*)a[2]);
        break;
    case O::TbGlobalTree: return P64(&gTechTree);
    case O::TbIsHuman: return ((EEPlayer*)p)->IsHuman();
    case O::TbClassFamily: return ((EEUnitClass*)p)->mFamily;
    case O::TbWorldPlayers: return (int64_t)gWorld.GetNumPlayers();
    case O::TbWorldPlayer: return P64(gWorld.GetPlayer((unsigned long)a[0]));
    // (EECore9OpD7) the C++ calls and members of the nodes' and epochs' bodies in Rust (gamecpp tech.rs; p the node or the epoch, the
    // player for the Pl ops): one statement each
    case O::TkUpgrade: return P64(static_cast<EEUnitStats*>(((EEPlayer*)p)->mUnitClassUpgrades[a[0]]));
    case O::TkVariant: return P64(static_cast<EeDataGameVariantFields*>(TVARIANT));
    case O::TkInCostSet: return gUnitSetManager.GetSet(TVARIANT->mCostSet) != NULL && gUnitSetManager.GetSet(TVARIANT->mCostSet)->Contains((long)a[0]);
    case O::TkPlayerJit: return ((EEPlayer*)p)->mJITManufacturing;
    case O::TkClassHasRallyPoint: return gUnitClassTable[a[0]] != NULL && gUnitClassTable[a[0]]->HasRallyPoint();
    case O::TkPushId: ((std::vector<long>*)a[0])->push_back((long)a[1]);
        break;
    // (EECore11OpC6) a class's run (gamecpp calamity_io.rs class_run): its host made and deleted around it (EEContagionManager.cpp's
    // EECore11OpC5 default); p the class, a0 the calamity, a1 and a2 the sounds
    case O::C11CalHostNew: return P64((new EECalamityUpdateHost{(EECalamityClass*)p, (EECalamity*)a[0], {(EESoundObject*)a[1], (EESoundObject*)a[2]}, {}, NULL, {}, {}}));
    case O::C11CalHostDelete: delete (EECalamityUpdateHost*)p;
        break;
    // (EECore9OpD6) the C++ calls and members of the tech tree's host in Rust (gamecpp tech.rs; p the tree unless noted): one statement
    // each
    case O::TkCurrentPlayer: return P64(gWorld.GetCurrentPlayer());
    case O::TkPlId: return (int64_t)TPL->mPlayerID;
    case O::TkPostShowEpochName: gUserInterface->PostShowEpochName();
        break;
    case O::TkMainGameForm: return P64(gUIManager->GetMainGameForm());
    case O::TkRefreshGroup: gUIManager->GetMainGameForm()->mRefreshGroup = true;
        break;
    case O::TkUnitId: return (int64_t)((EEComplexUnit*)p)->mUnitID;
    case O::TkTrigger: EEEvent::Trigger((long)a[0], TREE->mPlayerIndex, (long)a[1], TREE->mPlayer, (EEComplexUnit*)a[2]);
        break;
    case O::TkEpochReached: TPL->EpochReached((long)a[0]);
        break;
    case O::TkStatReset: TPL->mSavedGroups.Reset((long)a[0], 1);
        break;
    case O::TkStatSet: TPL->mSavedGroups.Set((long)a[0], (long)a[1]);
        break;
    case O::TkUnloadClassMedia: TPL->UnloadUnitClassMedia((long)a[0]);
        break;
    case O::TkClassMedia: TPL->SetUnitClassMedia((long)a[0], 0x7f);
        break;
    case O::TkClassButtonsLoad: TPL->LoadUnitClassButtons((long)a[0]);
        break;
    case O::TkClassButtonsUnload: TPL->UnloadUnitClassButtons((long)a[0]);
        break;
    case O::TkEpochMediaLoaded: TPL->EpochMediaLoaded((long)a[0]);
        break;
    case O::TkEpochMediaUnloaded: TPL->EpochMediaUnloaded((long)a[0]);
        break;
    case O::TkEventsEnabled: gEventsEnabled = a[0] != 0;
        break;
    case O::TkIsLocal: return gWorld.mPlayers[TREE->mPlayerIndex] == gWorld.GetCurrentPlayer();
    case O::TkAssert: ESVERIFY_AT(false, a[0]);
        break;
    case O::TkEvent: return P64(EEEvent::Get((long)a[0]));
    case O::TkEventEffects: return (int64_t)((EEEvent*)p)->GetNumEffects();
    case O::TkEventEffect: return P64(EEEvent::GetEventEffect(((EEEvent*)p)->GetEffect((long)a[0])));
    case O::TkEffKind: return TEFFECT->mKind;
    case O::TkEffUnitClass: return TEFFECT->mUnitClass;
    case O::TkEffTechnology: return TEFFECT->mTechnology;
    case O::TkEffButton: return TEFFECT->mButton;
    case O::TkEffAreaEffect: return TEFFECT->mAreaEffect;
    case O::TkEffMediaIndex: return TEFFECT->mMediaIndex;
    case O::TkEffNewUnitClass: return TEFFECT->mNewUnitClass;
    case O::TkEffSoundId: return TEFFECT->mSoundID;
    case O::TkEffAttribute: return TEFFECT->mAttribute;
    case O::TkButtonAcquire: EEButtonObject::Get(a[0])->Acquire();
        break;
    case O::TkButtonRelease: EEButtonObject::Get(a[0])->Release();
        break;
    case O::TkItemLoad: EEMedia::Get(a[0])->LoadMedia();
        break;
    case O::TkItemUnload: EEMedia::Get(a[0])->UnloadMedia();
        break;
    case O::TkAeTemplateLoad: gAEManager.LoadTemplateMedia((unsigned long)a[0]);
        break;
    case O::TkAeTemplateUnload: gAEManager.UnloadTemplateMedia((unsigned long)a[0]);
        break;
    case O::TkSoundLoad: EESoundObject::Get(a[0])->LoadMedia();
        break;
    case O::TkSoundUnload: EESoundObject::Get(a[0])->UnloadMedia();
        break;
    case O::TkClassCount: return (int64_t)EEUnitClassCount();
    case O::TkClassAt: return P64(gUnitClassTable[a[0]]);
    case O::TkClassType: return (int64_t)((EEUnitClass*)p)->mUnitType;
    case O::TkClassTechnology: return gUnitClassTable[a[0]] != NULL ? gUnitClassTable[a[0]]->mTechnology : 100000;
    case O::TkFileOpenRead: TFILE->Open(kFileAccessRead);
        break;
    case O::TkFileLength: TFILE->GetLength(*(unsigned long*)a[0]);
        break;
    case O::TkFilePosition: return TFILE->GetPosition(*(long*)a[0]);
    case O::TkFileClose: TFILE->Close();
        break;
    default: ESVERIFY_AT(inOp == EeGameCpp::UnRts, 1);   // the end of the chain
    }
    return 0;
}
