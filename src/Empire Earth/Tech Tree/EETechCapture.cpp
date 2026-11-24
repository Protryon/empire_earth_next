// New (port), TEST ONLY: the tech tree's, event effects' and calamities' records of the simulation capture
// (ee/EETechCapture.h; tools/sim_capture.py; docs/RUST_PLAN.md R5b.2). Reads the world, writes nothing to it.
#include <windows.h>
#include <algorithm>
#include <set>
#include <string.h>
#include <vector>

#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GETerrainMesh.h"

#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "lle/USingleton.h"
#include "ee/EECalamityManager.h"
#include "ee/EECalamityClass.h"
#include "ee/EETechCapture.h"
#include "ee/EECalamityCapture.h"
#include "ee/EECalamity.h"
#include "ee/EEMap.h"
#include "ee/EEContagionManager.h"
#include "rust/sim/tech.h"   // ee_tech_capture_* (ee_sim tech capture_rec.rs)
#include "rust/game/gamecpp.h"   // the calamities' records (ee_game gamecpp calamity_capture.rs)
#include "ee/EEGameVariant.h"
#include "ee/EEButtonObject.h"
#include "ee/EETechTreeRecord.h"
#include "ee/EEStringIDs.h"
#include <iterator>
#include <utility>
#include <stddef.h>
#include <stdlib.h>
#include "lle/UResourceFile.h"
#include "lle/UIHotKey.h"
#include "ee/EEUnitSetManager.h"
#include "ee/EEDatabaseTables.h"
#include "ee/EEEvent.h"
#include "ee/EEMedia.h"
#include "ee/EESoundObject.h"
#include "ee/EEAEManager.h"
#include "ee/EmpireEarth.h"
#include "lle/FSRestorable.h"
#include "lle/GE3DPoint.h"
#include "ee/EEUnitKind.h"
#include "ee/EEEffect.h"
#include "ee/EECalamityUpdate.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"
#include "ee/EEUserInterface.h"
#include "ee/EEUIManager.h"
#include "ee/EEUIFMainGame.h"

static long EETechCapturePlayerIndexImpl(EETechTree* inTree)
{
    return inTree != NULL ? inTree->mPlayerIndex : -1;
}

static void EETechCaptureCostImpl(EETechTreeNode* inNode, long inKind, long inResource, long inUnitClass,
                                  float inScale, long inCost)
{
    ee_tech_capture_cost(inKind, EETechCapturePlayerIndex(inNode->mTree), inNode->mData->mID,
                         inNode->mTree != NULL ? inNode->mTree->mCurrentEpoch : -1, inResource, inUnitClass, inScale, inCost);
}


// CCAS: an outermost CanCastAt's record (ee_game gamecpp calamity_capture.rs)

static long CanCastRecordImpl(bool inOutermost, EECalamityClass* inClass, EEComplexUnit* inUnit,
                              const GE3DPoint& inLocation, unsigned long inX, unsigned long inY, bool inCheck,
                              EEUnit* inTargetUnit, long inResult)
{
    return ee_gamecpp_capture_can_cast_record(inOutermost, inClass, inUnit, (const float(*)[3])&inLocation.mX, inX, inY,
                                              inCheck, inTargetUnit, inResult);
}

// ------------------------------------------------------------------------- round 10, lane B: the records' C++


#define P64(x) ((int64_t)(intptr_t)(x))
int32_t EETechEpochNodes(EETechTreeEpoch* inEpoch, EeTechHostIo* io);   // EETechTree.cpp
#define TC_UPGRADE ((EEUnitClassUpgrade*)p)
#define TC_PLAYER ((EEPlayer*)p)

// The game calls these three through the header's hook pointers, set here before main: the game's units name no
// symbol of this TEST ONLY unit (tools/cpp_deps.py cuts; PORT_LEARNINGS "Cycle cuts")
static const bool sCaptureHooksSet = (EECalamityCanCastRecord = CanCastRecordImpl, EETechCapturePlayerIndex = EETechCapturePlayerIndexImpl,
                                      EETechCaptureCost = EETechCaptureCostImpl, true);

// ------------------------------------------------------------------------- round 11, lane C: the hooks' C++

int64_t EECore11OpE(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f);

#define TCU ((EEUnit*)p)
#define TCX ((EEComplexUnit*)p)

#define BNODE ((EETechTreeNode*)p)
#define BEPOCH ((EETechTreeEpoch*)p)
#define BTREE ((EETechTree*)p)
#define TNODE ((EETechTreeNode*)p)
#define TEPOCH ((EETechTreeEpoch*)p)
#define TVARIANT EEGameVariant::GetGameVariant(gWorld.mGameOptions.mVariant)
#define TREE ((EETechTree*)p)
#define TPL ((EEPlayer*)p)
#define TFILE ((FSFileSpec*)p)
#define TEFFECT ((EEEventEffect*)p)
// the EVUN unit step's members (gamecpp tech_hooks.rs; p the unit or its complex unit; EECalamityUpdate.cpp's
// EECore11OpC6 default): one statement each
int64_t EECore11OpC7(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f)
{
    typedef EeGameCpp O;
    switch (inOp) {
    case O::C11TcUnitComplex: return P64(TCU->mComplexUnit);
    case O::C11TcUnitId: return (int64_t)TCU->mUnitID;
    case O::C11TcUnitType: return (int64_t)TCU->mUnitClass->mUnitType;
    case O::C11TcUnitHitPoints: return (long)TCU->GetHitPoints();
    case O::C11TcUnitTileNode: return TCU->mTileNode != NULL;
    case O::C11TcComplexBuilt: return TCX->mIsBuilt;
    case O::C11TcComplexPrevious: return TCX->mPreviousPart != NULL;
    case O::C11TcComplexFlightTime: return (long)TCX->mFlightTime;
    // (EECore10OpB6) the upgrade's and the player's record fields (gamecpp tech_capture.rs; p the record, the upgrade or the player): one
    // statement each
    case O::TcBytes: ((EESimCaptureRecord*)p)->Bytes((const void*)a[0], (unsigned int)a[1]);
        break;
    case O::TcUpgradeFirst: return P64(&TC_UPGRADE->mAreaEffectScale);
    case O::TcUpgradeEnd: return P64(&TC_UPGRADE->mIsInitialized);   // (the bools before it included)
    case O::TcUpgradeMediaCount: return P64(&TC_UPGRADE->mMediaCount);
    case O::TcUpgradeButtonCount: return P64(&TC_UPGRADE->mButtonLoadCount);
    case O::TcPlayerId: return TC_PLAYER->mPlayerID;
    case O::TcPlayerPopulationBonus: return TC_PLAYER->mPopulationLimitBonus;
    case O::TcPlayerTributeFee: return P64(&TC_PLAYER->mTributeFee);
    case O::TcPlayerBB44: return TC_PLAYER->mBB44;
    // (EECore10OpB7) the tech tree's loads, copies, costs and sweep (gamecpp tech_load.rs, tech_capture.rs; p the node, the epoch, the
    // tree or the player): one statement each
    case O::TbNewData: return P64(BNODE->mData = new RTSTechTreeNodeData);
    case O::TbDeleteData: delete std::exchange(BNODE->mData, nullptr);
        break;
    case O::TbShareData: BNODE->mData = ((const EETechTreeNode*)a[0])->mData;
        break;
    case O::TbSetTree: BNODE->mTree = (EETechTree*)a[0];
        break;
    case O::TbNewNode: return P64(new EETechTreeNode);
    case O::TbClassesResize: BNODE->mData->mUnitClasses.resize((unsigned long)a[0], 0);
        break;
    case O::TbClassRead: return ((FSFile*)a[0])->Read(BNODE->mData->mUnitClasses[a[1]]);
    case O::TbClassCount: return (int64_t)BNODE->mData->mUnitClasses.size();
    case O::TbClassAt: return (int64_t)BNODE->mData->mUnitClasses[a[0]];
    case O::TbCostsInsert: BNODE->mData->mClassCosts.insert(std::map<long, long*>::value_type((long)BNODE->mData->mUnitClasses[a[0]], new long[7]{(long)a[1], (long)a[2], (long)a[3], (long)a[4], (long)a[5], (long)a[6], (long)a[7]}));
        break;
    case O::TbCostsCount: return (int64_t)BNODE->mData->mClassCosts.size();
    case O::TbCostsDelete: delete[] std::next(BNODE->mData->mClassCosts.begin(), a[0])->second;
        break;
    case O::TbCostsClear: BNODE->mData->mClassCosts.clear();
        break;
    case O::TbButton: return P64(BNODE->mButton);
    case O::TbCopyButton: BNODE->mButton = ((const EETechTreeNode*)a[0])->mButton;
        break;
    case O::TbSetButton: BNODE->mButton = EEButtonObject::Get((unsigned long)a[0]);
        break;
    case O::TbButtonGet: return P64(EEButtonObject::Get((unsigned long)a[0]));
    case O::TbSwapButton: ((EEPlayer*)p)->SwapClassButtonMedia((long)a[0], (EEButtonObject*)a[1], (EEButtonObject*)a[2]);
        break;
    case O::TbGlobalTree: return P64(&gTechTree);
    case O::TbEpochInsert: BEPOCH->mNodes.insert(std::map<long, EETechTreeNode*>::value_type(((EETechTreeNode*)a[0])->mData->mID, (EETechTreeNode*)a[0]));
        break;
    case O::TbSetPlayerIndex: BTREE->mPlayerIndex = (long)a[0];
        break;
    case O::TbCopyPlayer: BTREE->mPlayer = ((const EETechTree*)a[0])->mPlayer;
        break;
    case O::TbIsHuman: return ((EEPlayer*)p)->IsHuman();
    case O::TbClassFamily: return ((EEUnitClass*)p)->mFamily;
    case O::TbNodeIsObsolete: return BNODE->GetIsObsolete();
    case O::TbNodeDependsOn: return BNODE->DependsOn((long)a[0]);
    case O::TbNodeGetCost: return BNODE->GetCost((long)a[0], (long)a[1], f[0]);
    case O::TbTreeChecksum: return (int64_t)(uint32_t)BTREE->Walk(EeTechTreeOp::Checksum);
    case O::TbWorldPlayers: return (int64_t)gWorld.GetNumPlayers();
    case O::TbWorldPlayer: return P64(gWorld.GetPlayer((unsigned long)a[0]));
    case O::TbPlayerTree: return P64(((EEPlayer*)p)->GetTechTree());
    // (EECore9OpD7) the C++ calls and members of the nodes' and epochs' bodies in Rust (gamecpp tech.rs; p the node or the epoch, the
    // player for the Pl ops): one statement each
    case O::TkNodeFields: return P64(static_cast<EeTechNodeFields*>(TNODE));
    case O::TkNodeData: return P64(static_cast<EeTechNodeDataFields*>(TNODE->mData));
    case O::TkNodeTree: return P64(TNODE->mTree);
    case O::TkClassCosts: return P64(TNODE->mData->mClassCosts.count((long)a[0]) != 0 ? TNODE->mData->mClassCosts[(long)a[0]] : NULL);
    case O::TkUpgrade: return P64(static_cast<EEUnitStats*>(((EEPlayer*)p)->mUnitClassUpgrades[a[0]]));
    case O::TkInCostSet: return gUnitSetManager.GetSet(TVARIANT->mCostSet) != NULL && gUnitSetManager.GetSet(TVARIANT->mCostSet)->Contains((long)a[0]);
    case O::TkPlayerJit: return ((EEPlayer*)p)->mJITManufacturing;
    case O::TkClassHasRallyPoint: return gUnitClassTable[a[0]] != NULL && gUnitClassTable[a[0]]->HasRallyPoint();
    case O::TkNodeCount: return (int64_t)TEPOCH->mNodes.size();
    case O::TkNodeAt: return P64(std::next(TEPOCH->mNodes.begin(), a[0])->second);
    case O::TkNodeKeyAt: return std::next(TEPOCH->mNodes.begin(), a[0])->first;
    case O::TkNodeUnload: TNODE->Unload();
        break;
    case O::TkNodeDelete: delete TNODE;
        break;
    case O::TkNodesClear: TEPOCH->mNodes.clear();
        break;
    case O::TkEpochDeleteData: delete std::exchange(TEPOCH->mData, nullptr);
        break;
    case O::TkPushId: ((std::vector<long>*)a[0])->push_back((long)a[1]);
        break;
    case O::TkLoadedClassesPush: TEPOCH->mLoadedUnitClasses.push_back((long)a[0]);
        break;
    case O::TkLoadedClassesCount: return (int64_t)TEPOCH->mLoadedUnitClasses.size();
    case O::TkLoadedClassAt: return TEPOCH->mLoadedUnitClasses[a[0]];
    case O::TkLoadedClassesClear: TEPOCH->mLoadedUnitClasses.clear();
        break;
    case O::TkNodeLoadMedia: TEPOCH->EETechTreeNode::LoadMedia((EEPlayer*)a[0]);
        break;
    case O::TkNodeUnloadMedia: TEPOCH->EETechTreeNode::UnloadMedia((EEPlayer*)a[0]);
        break;
    // (EECore11OpC6) a class's run (gamecpp calamity_io.rs class_run): its host made and deleted around it (EEContagionManager.cpp's
    // EECore11OpC5 default); p the class, a0 the calamity, a1 and a2 the sounds
    case O::C11CalHostNew: return P64((new EECalamityUpdateHost{(EECalamityClass*)p, (EECalamity*)a[0], {(EESoundObject*)a[1], (EESoundObject*)a[2]}, {}, NULL, {}, {}}));
    case O::C11CalHostDelete: delete (EECalamityUpdateHost*)p;
        break;
    // (EECore9OpD6) the C++ calls and members of the tech tree's host in Rust (gamecpp tech.rs; p the tree unless noted): one statement
    // each
    case O::TkTreeFields: return P64(static_cast<EeTechTreeFields*>(TREE));
    case O::TkCurrentPlayer: return P64(gWorld.GetCurrentPlayer());
    case O::TkPlayerIndex: return TREE->mPlayerIndex;
    case O::TkPlId: return (int64_t)TPL->mPlayerID;
    case O::TkEpochTitle: return EEEpochTitleID((long)a[0]);
    case O::TkPostShowEpochName: gUserInterface->PostShowEpochName();
        break;
    case O::TkMainGameForm: return P64(gUIManager->GetMainGameForm());
    case O::TkRefreshGroup: gUIManager->GetMainGameForm()->mRefreshGroup = true;
        break;
    case O::TkEpochCount: return TREE->mEpochs.size();
    case O::TkEpochAt: return P64(&TREE->mEpochs[(long)a[0]]);
    case O::TkEpochRef: ((EeTechHostIo*)a[1])->node = TREE->mEpochs[(long)a[0]].Ref();
        break;
    case O::TkEpochNodes: return EETechEpochNodes((EETechTreeEpoch*)p, (EeTechHostIo*)a[0]);
    case O::TkUnitId: return (int64_t)((EEComplexUnit*)p)->mUnitID;
    case O::TkGet: return P64(TREE->Get((long)a[0]));
    case O::TkSetRef: ((EeTechHostIo*)a[0])->node = TNODE->Ref();
        break;
    case O::TkNeedsUnitMedia: return TNODE->NeedsUnitMedia();
    case O::TkTrigger: EEEvent::Trigger((long)a[0], TREE->mPlayerIndex, (long)a[1], TREE->mPlayer, (EEComplexUnit*)a[2]);
        break;
    case O::TkGetPlayer: return P64(TREE->GetPlayer());
    case O::TkTreePlayer: return P64(TREE->mPlayer);
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
    case O::TkEvent: return P64(EEEvent::Get(TNODE->mData->mResearchedEvent));
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
    case O::TkEpochsResize: TREE->mEpochs.Resize((long)a[0]);
        break;
    case O::TkEpochLoad: return TREE->mEpochs[(long)a[0]].Load(*(FSFileSpec*)a[1], TREE);
    case O::TkEpochUnload: TREE->mEpochs[(long)a[0]].Unload();
        break;
    case O::TkNodeUnitClass0: return TNODE->GetUnitClass(0);
    case O::TkNodeIsAvailable: return TNODE->GetIsAvailable();
    case O::TkNodeGetCost: return TNODE->GetCost((long)a[0], (long)a[1], 1.0f);
    default: return EECore11OpE(inOp, p, a, f);
    }
    return 0;
}
