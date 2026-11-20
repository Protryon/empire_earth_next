// New (port), TEST ONLY: the civilization builder lane's capture records (ee/EECivCapture.h; tools/sim_capture.py).
// Reads the manager, the powers, the players and their class upgrades; changes nothing but the capture file (CVMG
// loads the premade civilizations' files through EELoadCivilization, as the game's choice form does).
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
#include "ee/EETechCapture.h"
#include "ee/EECivCapture.h"
#include "rust/sim/tech.h"   // ee_tech_capture_civ_manager

namespace {

void Text(EESimCaptureRecord& ioRecord, const char* inText)
{
    unsigned int theSize = inText != NULL ? (unsigned int)strlen(inText) : 0;
    ioRecord.U32(theSize);
    ioRecord.Bytes(inText, theSize);
}

void Upgrades(EESimCaptureRecord& ioRecord, const std::vector<EECivUpgrade>& inUpgrades)
{
    ioRecord.U32((unsigned int)inUpgrades.size());
    for (EECivUpgrade theUpgrade : inUpgrades)
        ioRecord.U32(theUpgrade);
}

// the player's fields the upgrade levels and powers change
void PlayerFields(EESimCaptureRecord& ioRecord, EEPlayer* inPlayer)
{
    EETechCapturePlayer(ioRecord, inPlayer);
    if (inPlayer == NULL)
        return;
    ioRecord.F32(inPlayer->mConversionFactor);
    ioRecord.F32(inPlayer->mHighGroundBonus);
    ioRecord.I32(inPlayer->mShareBonus);
    ioRecord.I32(inPlayer->mMiningLimit);
}

// the player's civilization
void Civilization(EESimCaptureRecord& ioRecord, EECivilization& inCivilization)
{
    Upgrades(ioRecord, inCivilization.mPurchasedUpgrades);
    Upgrades(ioRecord, inCivilization.mDeferredUpgrades);
    Text(ioRecord, (const char*)inCivilization.mName);
    ioRecord.U32(inCivilization.mAvailablePoints);
    ioRecord.U32(inCivilization.mMaxPoints);
    ioRecord.U32((inCivilization.mNoPointsAdded ? 1u : 0u) | (inCivilization.mUnchanged ? 2u : 0u));
}

// the unit types of a set, in its order
class Collect : public EEUnitSetFunctor {
public:
    virtual bool Execute(long inUnitType)
    {
        this->mTypes.push_back(inUnitType);
        return true;
    }
    std::vector<long> mTypes;
};

// CVMG: the manager's and the powers' data and every query over it (ee_sim tech capture_rec.rs), then the premade
// civilizations' files, loaded and matched
void CaptureManager()
{
    EECivilizationManager& m = gCivilizationManager;
    EESimCaptureRecord r(EECaptureTag("CVMG"));
    uint32_t theSize = 0;
    unsigned char* theHead = ee_tech_capture_civ_manager(&theSize);
    r.Bytes(theHead, theSize);
    ee_tech_capture_free(theHead, theSize);
    std::vector<EECivilizationEntry> theCivs;
    m.GetCivilizations(theCivs);
    // the premade civilizations' files: loaded and matched
    for (const EECivilizationEntry& theCiv : theCivs) {
        UWideString theName;
        std::vector<EECivUpgrade> theLoaded;
        UString theFile(theCiv.mFileName);
        long theResult = EELoadCivilization(theFile, EEGetCivilizationDirectory(false), theName, theLoaded);
        r.I32(theResult);
        long theNameID = -7;
        r.U32(m.MatchesPremadeCivilization(UWideString(theCiv.mFileName), theName, theLoaded, &theNameID) ? 1 : 0);
        r.I32(theNameID);
        theLoaded.push_back(1);
        r.U32(m.MatchesPremadeCivilization(UWideString(theCiv.mFileName), theName, theLoaded, &theNameID) ? 1 : 0);
        r.I32(theNameID);
    }
    long theNameID = -7;
    std::vector<EECivUpgrade> theNone;
    r.U32(m.MatchesPremadeCivilization(UWideString("no such file"), UWideString("x"), theNone, &theNameID) ? 1 : 0);
    r.I32(theNameID);
    r.Commit();
}

}  // namespace

struct EECivCaptureApply {
    EECivUpgrade mUpgrade;
    long mPlayer;
    std::vector<long> mTypes;
    EESimCaptureRecord mRecord{EECaptureTag("CVAP")};
};

static EECivCaptureApply* EECivCaptureApplyBeginImpl(EECivUpgrade inUpgrade, long inPlayer)
{
    if (!EESimCaptureRecord::Active())
        return NULL;
    static bool sManagerDone = false;
    if (!sManagerDone) {
        sManagerDone = true;
        CaptureManager();
    }
    EECivCaptureApply* c = new EECivCaptureApply;
    c->mUpgrade = inUpgrade;
    c->mPlayer = inPlayer;
    c->mRecord.U32(inUpgrade);
    c->mRecord.I32(inPlayer);
    EEPlayer* thePlayer = inPlayer >= 0 && inPlayer < 17 ? ::gWorld.mPlayers[inPlayer] : NULL;
    PlayerFields(c->mRecord, thePlayer);
    unsigned long theCategory = 0;
    gCivilizationManager.SplitAttribute(inUpgrade, &theCategory, NULL);
    EECivilizationCategoryEntry* theEntry = NULL;
    if (thePlayer != NULL && gCivilizationManager.GetCategory(theCategory, &theEntry) &&
        gUnitSetManager.GetSet(theEntry->mUnitSet) != NULL) {
        Collect theCollect;
        gUnitSetManager.ForEach(theEntry->mUnitSet, &theCollect);
        c->mTypes = theCollect.mTypes;
    }
    c->mRecord.U32((unsigned int)c->mTypes.size());
    for (long theType : c->mTypes) {
        c->mRecord.I32(theType);
        EETechCaptureUpgrade(c->mRecord, gUnitClassTable[theType] != NULL ? thePlayer->mUnitClassUpgrades[theType] : NULL);
    }
    return c;
}

static void EECivCaptureApplyEndImpl(EECivCaptureApply* inCapture, bool inResult)
{
    if (inCapture == NULL)
        return;
    EESimCaptureRecord& r = inCapture->mRecord;
    r.U32(inResult ? 1 : 0);
    EEPlayer* thePlayer = inCapture->mPlayer >= 0 && inCapture->mPlayer < 17
                              ? ::gWorld.mPlayers[inCapture->mPlayer] : NULL;
    PlayerFields(r, thePlayer);
    for (long theType : inCapture->mTypes)
        EETechCaptureUpgrade(r, gUnitClassTable[theType] != NULL ? thePlayer->mUnitClassUpgrades[theType] : NULL);
    unsigned long theLevel = 0;
    gCivilizationManager.SplitAttribute(inCapture->mUpgrade, NULL, &theLevel);
    if (thePlayer != NULL && EECivilizationManager::IsPowerLevel(theLevel)) {
        r.U32((unsigned int)thePlayer->mLoadedMedia.size());
        for (long theID = kCivPowerFlamingArrows; theID <= kCivPowerJustInTimeManufacturing + 2; theID++)
            r.U32(gCivPowersManager.HasPower(thePlayer, theID) ? 1 : 0);
        EETechTree* theTree = thePlayer->GetTechTree();
        if (theTree != NULL) {
            EETechCaptureTree(r, theTree);
            std::vector<long> theIDs;
            theTree->GetNodeIDs(theIDs);
            r.U32((unsigned int)theIDs.size());
            for (long theID : theIDs) {
                EETechTreeNode* theNode = theTree->Get(theID);
                r.I32(theID);
                if (theNode != NULL)
                    r.U32((theNode->mIsResearched ? 1u : 0u) | (theNode->mIsObsolete ? 2u : 0u) |
                          (theNode->mIsAvailable ? 4u : 0u) | (theNode->mIsAllowed ? 8u : 0u) | 16u);
                else
                    r.U32(0);
            }
        }
    }
    r.Commit();
    delete inCapture;
}

static void EECivCaptureLoadImpl(const UString& inName, const UString& inDirectory, long inResult,
                                 const UWideString& inCivName, const std::vector<EECivUpgrade>& inUpgrades)
{
    if (!EESimCaptureRecord::Active())
        return;
    EESimCaptureRecord r(EECaptureTag("CVLD"));
    Text(r, (const char*)inName);
    // the directory without the startup folder (the user's directory is under the run's own folder)
    UString theUser = EEGetCivilizationDirectory(true);
    r.U32(inDirectory.Equals(theUser) ? 1 : 0);
    if (!inDirectory.Equals(theUser))
        Text(r, (const char*)inDirectory);
    r.I32(inResult);
    Text(r, (const char*)inCivName);
    Upgrades(r, inUpgrades);
    r.Commit();
}

static void EECivCaptureApplyFileImpl(const UString& inName, EEPlayer* inPlayer, long inResult)
{
    if (!EESimCaptureRecord::Active())
        return;
    EESimCaptureRecord r(EECaptureTag("CVAF"));
    Text(r, (const char*)inName);
    PlayerFields(r, inPlayer);
    r.I32(inResult);
    if (inPlayer != NULL) {
        Civilization(r, inPlayer->mCivilization);
        r.U32(inPlayer->mCivUpgradesPurchased ? 1 : 0);
    }
    r.Commit();
}

// The game calls these through the header's hook pointers, set here before main: the game's units name no
// symbol of this TEST ONLY unit (tools/cpp_deps.py cuts; PORT_LEARNINGS "Cycle cuts")
static const bool sCaptureHooksSet = (EECivCaptureApplyBegin = EECivCaptureApplyBeginImpl,
                                      EECivCaptureApplyEnd = EECivCaptureApplyEndImpl,
                                      EECivCaptureLoad = EECivCaptureLoadImpl,
                                      EECivCaptureApplyFile = EECivCaptureApplyFileImpl, true);
