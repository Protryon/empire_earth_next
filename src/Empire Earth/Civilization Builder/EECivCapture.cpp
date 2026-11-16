// New (port), TEST ONLY: the civilization builder lane's capture records (ee/EECivCapture.h; tools/sim_capture.py).
// Reads the manager, the powers, the players and their class upgrades; changes nothing but the capture file (CVMG
// loads the premade civilizations' files through EELoadCivilization, as the game's choice form does).
#include <windows.h>
#include <algorithm>
#include <string.h>
#include <string>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/GE3DPlane.h"
#include "lle/GETerrainMesh.h"
#include "lle/UResourceFile.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEUnitSetManager.h"
#include "ee/EECivilization.h"
#include "ee/EECivilizationManager.h"
#include "ee/EECivPowersManager.h"
#include "ee/EETechCapture.h"
#include "ee/EECivCapture.h"

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

// CVMG: the manager's and the powers' data and every query over it
void CaptureManager()
{
    EECivilizationManager& m = gCivilizationManager;
    EESimCaptureRecord r(EECaptureTag("CVMG"));
    r.U32(m.GetCategoryCount());
    for (unsigned long i = 0; i <= m.GetCategoryCount(); i++) {   // one past: GetCategory's false
        EECivilizationCategoryEntry* theEntry = NULL;
        bool theKnown = m.GetCategory(i, &theEntry);
        r.U32(theKnown ? 1 : 0);
        if (theKnown)
            r.Bytes(theEntry, sizeof(EECivilizationCategoryEntry));
        long theNameID = -7;
        r.U32(m.GetCategoryNameID(i, &theNameID) ? 1 : 0);
        r.I32(theNameID);
        unsigned long theLevels = m.GetLevelCount(i);
        r.U32(theLevels);
        for (unsigned long k = 0; k <= theLevels + 1; k++) {
            float theValue = -7.0f, theBonus = -7.0f;
            r.U32(m.GetCategoryLevel(i, k, &theValue, &theBonus) ? 1 : 0);
            r.F32(theValue);
            r.F32(theBonus);
        }
        Text(r, m.GetCategoryID(i).c_str());
    }
    std::vector<EECivilizationEntry> theCivs;
    m.GetCivilizations(theCivs);
    r.U32((unsigned int)theCivs.size());
    for (const EECivilizationEntry& theCiv : theCivs)
        r.Bytes(&theCiv, sizeof(EECivilizationEntry));
    r.U32((unsigned int)m.GetEraCount());
    for (unsigned long e = 0; e < m.GetEraCount(); e++) {
        long theEra = m.GetEra(e);
        r.I32(theEra);
        std::vector<EECivilizationEntry> theOfType;
        m.GetCivilizationsOfType(theOfType, theEra);
        r.U32((unsigned int)theOfType.size());
        for (const EECivilizationEntry& theCiv : theOfType)
            r.I32(theCiv.mIndex);
    }
    // the upgrades: values, costs, keys both ways, legacy ids, counts
    std::vector<EECivUpgrade> theUpgrades = m.GetUpgrades();
    Upgrades(r, theUpgrades);
    std::vector<EECivUpgrade> theSome;
    for (EECivUpgrade theUpgrade : theUpgrades) {
        float theValue = -7.0f, theBonus = -7.0f;
        r.U32(m.LookupLevel(theUpgrade, &theValue, &theBonus) ? 1 : 0);
        r.F32(theValue);
        r.F32(theBonus);
        r.I32(m.GetExtraCost(theUpgrade));
        r.I32(m.CountInCategory(theUpgrade, &theSome));
        theSome.push_back(theUpgrade);
        std::string theKey;
        bool theKeyed = m.UpgradeKey(theUpgrade, theKey);
        r.U32(theKeyed ? 1 : 0);
        Text(r, theKey.c_str());
        EECivUpgrade theBack = 0xdeadbeef;
        r.U32(m.FindUpgradeKey(theKey, theBack) ? 1 : 0);
        r.U32(theBack);
        unsigned long theLegacy = 0;
        r.U32(EECivilizationManager::ToLegacy(theUpgrade, theLegacy) ? 1 : 0);
        r.U32(theLegacy);
        r.U32(EECivilizationManager::FromLegacy(theLegacy));
    }
    // keys that are not upgrades
    const char* theBadKeys[] = {"", ".", "nokey", "special_powers.", ".attack", "x.attack", "special_powers.x"};
    for (const char* theKey : theBadKeys) {
        EECivUpgrade theBack = 0xdeadbeef;
        r.U32(m.FindUpgradeKey(theKey, theBack) ? 1 : 0);
        r.U32(theBack);
    }
    // the levels: names, powers
    for (unsigned long theLevel = 0; theLevel <= 0x50; theLevel++) {
        r.I32(EECivilizationManager::LevelNameID(theLevel));
        r.U32(EECivilizationManager::IsPowerLevel(theLevel) ? 1 : 0);
        r.U32(EECivilizationManager::PowerLevel(theLevel));
        r.U32(m.MakePowerUpgrade(theLevel));
        r.U32(m.MakeUpgrade(theLevel & 7, theLevel));
    }
    // the powers
    r.U32((unsigned int)gCivPowersManager.GetNumPowers());
    for (long theID = 0x20; theID <= 0x42; theID++) {
        EECivPower thePower;
        r.U32(gCivPowersManager.GetPower(theID, thePower) ? 1 : 0);
        r.Bytes(&thePower, sizeof(EECivPower));
    }
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

EECivCaptureApply* EECivCaptureApplyBegin(EECivUpgrade inUpgrade, long inPlayer)
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

void EECivCaptureApplyEnd(EECivCaptureApply* inCapture, bool inResult)
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

void EECivCaptureLoad(const UString& inName, const UString& inDirectory, long inResult, const UWideString& inCivName,
                      const std::vector<EECivUpgrade>& inUpgrades)
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

void EECivCaptureApplyFile(const UString& inName, EEPlayer* inPlayer, long inResult)
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
