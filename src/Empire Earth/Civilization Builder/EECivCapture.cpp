// New (port), TEST ONLY: the civilization builder lane's capture records (ee/EECivCapture.h; tools/sim_capture.py):
// CVMG, CVAP, CVLD, CVAF. port (round 10, lane C): written by gamecpp civ_capture.rs; this unit sets the hooks the
// civilization manager calls and answers the records' C++ reads (one statement each). Reads the manager, the powers,
// the players and their class upgrades; changes nothing but the capture file (CVMG loads the premade civilizations'
// files through EELoadCivilization, as the game's choice form does).
#include <windows.h>
#include <algorithm>
#include <string.h>
#include <string>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEUnitSetManager.h"
#include "ee/EECivilizationManager.h"
#include "ee/EECivPowersManager.h"
#include "ee/EECivPowerIDs.h"
#include "ee/EETechCapture.h"
#include "ee/EECivCapture.h"
#include "rust/sim/units.h"
#include "rust/game/gamecpp.h"

static EECivCaptureApply* EECivCaptureApplyBeginImpl(EECivUpgrade inUpgrade, long inPlayer)
{
    return (EECivCaptureApply*)ee_gamecpp_civcap_apply_begin(inUpgrade, inPlayer);
}

static void EECivCaptureApplyEndImpl(EECivCaptureApply* inCapture, bool inResult)
{
    ee_gamecpp_civcap_apply_end(inCapture, inResult);
}

static void EECivCaptureLoadImpl(const UString& inName, const UString& inDirectory, long inResult,
                                 const UWideString& inCivName, const std::vector<EECivUpgrade>& inUpgrades)
{
    ee_gamecpp_civcap_load(&inName, &inDirectory, inResult, &inCivName, &inUpgrades);
}

static void EECivCaptureApplyFileImpl(const UString& inName, EEPlayer* inPlayer, long inResult)
{
    ee_gamecpp_civcap_apply_file(&inName, inPlayer, inResult);
}

// Hooks set during static initialisation; the original code is unchanged by these TEST ONLY records
static const bool sCaptureHooksSet = (EECivCaptureApplyBegin = EECivCaptureApplyBeginImpl,
                                      EECivCaptureApplyEnd = EECivCaptureApplyEndImpl,
                                      EECivCaptureLoad = EECivCaptureLoadImpl,
                                      EECivCaptureApplyFile = EECivCaptureApplyFileImpl, true);

// ------------------------------------------------------------------------- round 10, lane C: the records' C++ reads

int64_t EECore10OpC3(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f);   // EEMessageCapture.cpp

static_assert(kCivPowerFlamingArrows == 0x26 && kCivPowerJustInTimeManufacturing == 0x3c && EE_UNITS_LISTS_MEDIA == 6,
              "gamecpp civ_capture.rs CC_POWER_FIRST, CC_POWER_LAST, CC_LISTS_MEDIA");

#define P64(x) ((int64_t)(intptr_t)(x))
#define CCREC ((EESimCaptureRecord*)p)
#define CCPL ((EEPlayer*)p)
#define CCCIVS (*(std::vector<EECivilizationEntry>*)p)
#define CCCIV ((EECivilization*)p)
#define CCNODE ((EETechTreeNode*)p)
#define CCUPG (*(std::vector<EECivUpgrade>*)p)
#define CCLONGS (*(std::vector<long>*)p)
#define CCWIDE(i) (*(UWideString*)a[i])
#define CCUPGS(i) (*(std::vector<EECivUpgrade>*)a[i])

// the C++ calls and members of gamecpp civ_capture.rs (and of the other lane C captures): one statement each
int64_t EECore10OpC2(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f)
{
    typedef EeGameCpp O;
    switch (inOp) {
    // a scratch record the C++ capture writers append to (p the record)
    case O::LcRecNew: return P64(new EESimCaptureRecord(0));
    case O::LcRecDelete: delete CCREC;
        break;
    case O::LcRecSize: return (int64_t)CCREC->Data().size();
    case O::LcRecData: return P64(CCREC->Data().data());
    case O::LcRecTechPlayer: EETechCapturePlayer(*CCREC, (EEPlayer*)a[0]);
        break;
    case O::LcRecTechUpgrade: EETechCaptureUpgrade(*CCREC, (EEUnitClassUpgrade*)a[0]);
        break;
    case O::LcRecTechTree: EETechCaptureTree(*CCREC, (EETechTree*)a[0]);
        break;
    // the texts and lists (p the object)
    case O::LcUStringText: return P64((const char*)*(const UString*)p);
    case O::LcWideText: return P64((const char*)*(const UWideString*)p);
    case O::LcWideNew: return P64(new UWideString);
    case O::LcWideDelete: delete (UWideString*)p;
        break;
    case O::LcUpgVecNew: return P64(new std::vector<EECivUpgrade>);
    case O::LcUpgVecDelete: delete &CCUPG;
        break;
    case O::LcUpgVecSize: return (int64_t)CCUPG.size();
    case O::LcUpgVecAt: return CCUPG[a[0]];
    case O::LcUpgVecPush: CCUPG.push_back((EECivUpgrade)a[0]);
        break;
    case O::LcLongVecNew: return P64(new std::vector<long>);
    case O::LcLongVecDelete: delete &CCLONGS;
        break;
    case O::LcLongVecSize: return (int64_t)CCLONGS.size();
    case O::LcLongVecAt: return CCLONGS[a[0]];
    // the civilization manager (p a vector of its entries for the ops on one)
    case O::LcCivVecNew: return P64(new std::vector<EECivilizationEntry>);
    case O::LcCivVecDelete: delete &CCCIVS;
        break;
    case O::LcCivVecSize: return (int64_t)CCCIVS.size();
    case O::LcCivGetAll: gCivilizationManager.GetCivilizations(CCCIVS);
        break;
    case O::LcCivLoad: return EELoadCivilization(UString(CCCIVS[a[0]].mFileName), EEGetCivilizationDirectory(false), CCWIDE(1), CCUPGS(2));
    case O::LcCivMatch: return gCivilizationManager.MatchesPremadeCivilization(UWideString(CCCIVS[a[0]].mFileName), CCWIDE(1), CCUPGS(2), (long*)a[3]);
    case O::LcCivMatchNone: return gCivilizationManager.MatchesPremadeCivilization(UWideString("no such file"), UWideString("x"), CCUPG, (long*)a[0]);
    case O::LcCivSplit: gCivilizationManager.SplitAttribute((EECivUpgrade)a[0], (unsigned long*)a[1], (unsigned long*)a[2]);
        break;
    case O::LcCivCategory: return gCivilizationManager.GetCategory((unsigned long)a[0], (EECivilizationCategoryEntry**)a[1]);
    case O::LcCivCategorySet: return ((EECivilizationCategoryEntry*)p)->mUnitSet;
    case O::LcCivIsPowerLevel: return EECivilizationManager::IsPowerLevel((unsigned long)a[0]);
    case O::LcCivDirIsUser: return ((const UString*)p)->Equals(EEGetCivilizationDirectory(true));
    // a civilization (p an EECivilization*)
    case O::LcCivPurchased: return P64(&CCCIV->mPurchasedUpgrades);
    case O::LcCivDeferred: return P64(&CCCIV->mDeferredUpgrades);
    case O::LcCivName: return P64((const char*)CCCIV->mName);
    case O::LcCivPoints: return (int64_t)CCCIV->mAvailablePoints;
    case O::LcCivMaxPoints: return (int64_t)CCCIV->mMaxPoints;
    case O::LcCivFlags: return (CCCIV->mNoPointsAdded ? 1 : 0) | (CCCIV->mUnchanged ? 2 : 0);
    // the players (p an EEPlayer*), their tech trees and nodes
    case O::LcPlCivilization: return P64(&CCPL->mCivilization);
    case O::LcPlUpgradeOf: return P64(gUnitClassTable[a[0]] != NULL ? CCPL->mUnitClassUpgrades[a[0]] : NULL);
    case O::LcPlLists: return P64(CCPL->mLists);
    case O::LcPlHasPower: return gCivPowersManager.HasPower(CCPL, (long)a[0]);
    case O::LcPlTechTree: return P64(CCPL->GetTechTree());
    case O::LcTreeNodeIDs: ((EETechTree*)p)->GetNodeIDs(*(std::vector<long>*)a[0]);
        break;
    case O::LcTreeGet: return P64(((EETechTree*)p)->Get((long)a[0]));
    case O::LcNodeFlags: return (CCNODE->mIsResearched ? 1u : 0u) | (CCNODE->mIsObsolete ? 2u : 0u) | (CCNODE->mIsAvailable ? 4u : 0u) | (CCNODE->mIsAllowed ? 8u : 0u) | 16u;
    default: return EECore10OpC3(inOp, p, a, f);
    }
    return 0;
}
