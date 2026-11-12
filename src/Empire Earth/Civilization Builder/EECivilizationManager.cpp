// C:\EEX\Empire Earth\Civilization Builder\EECivilizationManager.cpp (file name from the class; no __FILE__). The
// civilization categories, premade civilizations and upgrade ids (RTTI EECivilizationManager, vtable 0x44bfd4). Range
// 0x2813f4..0x2830d0: it opens with its own two UString `$E` chains (".civ", "Civilizations"; objects 0x9654d0/0x9654c0)
// and closes before EECivPowersManager.cpp's ctor 0x2830d0. Whether 0x27efca..0x2813f4 (the civilization builder's
// events) belongs to the same TU stays open: this file claims only the range its own chain and class family prove.
// Unit flags: the exe module set.
#include <windows.h>
#include <mbstring.h>
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
#include "ee/EEUnitStore.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEMessage.h"
#include "ee/EECommunications.h"
#include "lle/UITextLabel.h"
#include "lle/UResourceFile.h"
#include "ee/EmpireEarth.h"
#include "ee/EEDatabaseTables.h"
#include "ee/EEUIFMainGame.h"
#include "ee/EEUIFMGMainGroup.h"
#include "ee/EEUIManager.h"
#include "ee/EEButtonObject.h"
#include "ee/EEUnitBehavior.h"
#include "ee/EEFormation.h"
#include "ee/EECivPowersManager.h"
#include "ee/EEUnitSetManager.h"
#include "ee/EEUnitVectorItem.h"
#include "ee/EEPhysics.h"
#include "ee/EEUICUnitIconBar.h"
#include "ee/EEUIFMGStatusBox.h"
#include "ee/EEUIFMGStatusButton.h"
#include "ee/EEUIEMGHotKeyStatusButton.h"
#include "ee/EECivilization.h"
#include "lle/GEFont.h"
#include "lle/GEText.h"
#include "lle/UIScrollBar.h"
#include "lle/UIListBox.h"
#include "lle/UILBString.h"
#include "ee/EECBUIListBox.h"
#include "ee/EECivilizationManager.h"
#include "ee/EEGameVariant.h"
#include "ee/EEStringIDs.h"
#include "ee/EERules.h"
#include "ee/EERulesWorld.h"
#include "ee/EECivPowerIDs.h"

// The premade civilizations' sort order (RTTI EECMPremadeCivSorter, vtable 0x44bfdc: only the deleting dtor; std::sort
// copies it by value; the comparison is inline)
class EECMPremadeCivSorter {
public:
    virtual ~EECMPremadeCivSorter() {}
    bool operator()(const EECivilizationEntry& inA, const EECivilizationEntry& inB) const { return inA.mIndex < inB.mIndex; }
};

// The upgrade enumeration functors (RTTI EECMUpgradeFunctor, vtable 0x44bfec; EECMGetUpgrades, vtable 0x44bfe4; both
// local to this file; no virtual dtor: GetUpgrades' frame object is destroyed without a vptr store)
class EECMUpgradeFunctor {
public:
    virtual bool operator()(EECivUpgrade inUpgrade, unsigned long inCategory, unsigned long inIndex) = 0;
};

class EECMGetUpgrades : public EECMUpgradeFunctor {
public:
    virtual bool operator()(EECivUpgrade inUpgrade, unsigned long inCategory, unsigned long inIndex)
    {
        this->mUpgrades.push_back(inUpgrade);
        return true;
    }

    std::vector<EECivUpgrade> mUpgrades;  // +0x04
};

// An upgrade's effect on every unit class of a unit set (RTTI EECBProcessUpgrade, vtable 0x44bff4; built by 0x281e4d,
// deleted without a destructor call; ctor 0x2823e2 and Execute 0x282409 follow 0x28239e) (provisional member names)
class EECBProcessUpgrade : public EEUnitSetFunctor {
public:
    EECBProcessUpgrade(long inPlayer, unsigned long inIndex, float inValue);
    virtual bool Execute(long inUnitType);

    EEPlayer* mPlayer;                      // +0x04
    unsigned long mIndex;                   // +0x08 the upgrade's index in its category (1..0x3c)
    float mValue;                           // +0x0c
};

// 0x28239e: one armor kind of inUpgrade raised by inValue of its class's base (at least 1) (provisional name)
void EECBUpgradeArmor(EEUnitClassUpgrade* inUpgrade, long inKind, float inValue);

// 0x24f0e9 (EEFOFindNewTargetNearOldTarget.cpp; as ee/EEUnitUtilities.h and EEWorld.cpp declare it)
void EEUpgradeHitPoints(EEUnitClassUpgrade* inUpgrade, long inType, float inFactor, EEPlayer* inPlayer);

// the civilization powers' category index (0x965568; set by Load, read by MakePowerUpgrade)
static unsigned long sPowersCategory;

// chain at 0x6813f4 (wrapper)
//   object 0x9654d0  ctor __imp_??0UString@@QAE@PBD@Z  dtor __imp_??1UString@@UAE@XZ
const UString kCivKey(".civ");
// SYNTHETIC: EXE 0x6813f4   (wrapper)
// SYNTHETIC: EXE 0x6813fe   (ctor)
// SYNTHETIC: EXE 0x68140f   (registrar)
// SYNTHETIC: EXE 0x68141b   (dtor)

// chain at 0x681426 (wrapper)
//   object 0x9654c0  ctor __imp_??0UString@@QAE@PBD@Z  dtor __imp_??1UString@@UAE@XZ
const UString kCivilizationsKey("Civilizations");
// SYNTHETIC: EXE 0x681426   (wrapper)
// SYNTHETIC: EXE 0x681430   (ctor)
// SYNTHETIC: EXE 0x681441   (registrar)
// SYNTHETIC: EXE 0x68144d   (dtor)

// FUNCTION: EXE 0x681458
// Creates the civilization manager with no categories
EECivilizationManager::EECivilizationManager()
{
    this->mCategories.clear();
    this->mCategoryCount = 0;
}

// SYNTHETIC: EXE 0x6814a8
// (the scalar deleting destructor, emitted with the vtable)

// FUNCTION: EXE 0x6814c4
// Destroys the civilization manager
EECivilizationManager::~EECivilizationManager()
{
}

// FUNCTION: EXE 0x6814e5
// Appends a category record to the category table
void EECivilizationManager::AddCategory(const EECivilizationCategoryEntry& inEntry)
{
    this->mCategories.push_back(inEntry);
    this->mLevels.push_back(std::vector<EECivilizationUpgradeValue>(inEntry.mLevels, inEntry.mLevels + 60));
    this->mCategoryIDs.push_back(std::string());
    this->mCategoryCount++;
}

// FUNCTION: EXE 0x681514
// Reads the upgrade categories from the civilization file, then appends the civ powers category (one level per
// power 0x26..0x3c, cost from world table entry 0x56)
// The powers category is civbuilder.yaml's powers_category; its levels are the attributes that apply a power.
long EECivilizationManager::Load(FSFile* inFile)
{
    long theLastU00 = 0;
    long theLastU04 = 0;
    unsigned long theCount = 0;
    inFile->Open(kFileAccessRead);
    long theResult = inFile->Read(theCount);
    if (theResult == kErrorOK) {
        for (unsigned long i = 0; i < theCount; i++) {
            EECivilizationCategoryEntry theEntry;
            unsigned long theRead;
            memset(&theEntry, 0, sizeof(EECivilizationCategoryEntry));
            theResult = inFile->Read(reinterpret_cast<unsigned char*>(&theEntry), sizeof(EECivilizationCategoryEntry), theRead);
            if (!(theEntry.mIndex > 0))
                continue;
            this->AddCategory(theEntry);
            // the text's id and levels (any number of them: the record holds the first 60)
            if (const EECivCategoryText* theText = EEGetRules().mCivBuilder.FindCategory((long)i)) {
                this->mCategoryIDs.back() = theText->mID;
                std::vector<EECivilizationUpgradeValue>& theLevels = this->mLevels.back();
                if (theText->mValues.size() > theLevels.size())
                    theLevels.resize(theText->mValues.size(), EECivilizationUpgradeValue{0.0f, 0.0f});
                for (size_t k = 0; k < theText->mValues.size(); k++)
                    theLevels[k] = EECivilizationUpgradeValue{theText->mValues[k], theText->mBonuses[k]};
            }
            theLastU00 = theEntry.mDatabaseID;
            theLastU04 = theEntry.mIndex;
        }
        // the civilization powers' category: one level per power 0x26..0x3c
        const EECivBuilderRules& theRules = EEGetRules().mCivBuilder;
        EECivilizationCategoryEntry thePowers;
        memset(&thePowers, 0, sizeof(EECivilizationCategoryEntry));
        thePowers.mDatabaseID = theLastU00 + 1;
        thePowers.mIndex = theLastU04 + 1;
        thePowers.mNameID = theRules.mPowersNameID;
        thePowers.mUnitSet = theRules.mPowersUnitSet;
        thePowers.mCost = EERuleLong(theRules.mPowersCost);
        for (const EECivAttribute& theAttribute : theRules.mAttributes) {   // a level per power attribute
            for (const EECivApply& theApply : theAttribute.mApplies) {
                if (theApply.mStat != kCivStatPower || theAttribute.mLevel < 1 || theAttribute.mLevel > 60 ||   // (the record's)
                    (unsigned long)theApply.mPower >= gCivPowersManager.mPowers.size())
                    continue;
                EECivilizationUpgradeValue* theLevel = &thePowers.mLevels[theAttribute.mLevel - 1];
                theLevel->mBonus = gCivPowersManager.mPowers[theApply.mPower].mBonus;
                theLevel->mValue = theRules.mPowersLevelValue;
                break;
            }
        }
        sPowersCategory = this->mCategoryCount;
        this->AddCategory(thePowers);
        std::vector<EECivilizationUpgradeValue>& theLevels = this->mLevels.back();   // powers past level 60
        for (const EECivAttribute& theAttribute : theRules.mAttributes)
            for (const EECivApply& theApply : theAttribute.mApplies) {
                if (theApply.mStat != kCivStatPower || theAttribute.mLevel <= 60 ||
                    (unsigned long)theApply.mPower >= gCivPowersManager.mPowers.size())
                    continue;
                if ((long)theLevels.size() < theAttribute.mLevel)
                    theLevels.resize(theAttribute.mLevel, EECivilizationUpgradeValue{0.0f, 0.0f});
                theLevels[theAttribute.mLevel - 1] =
                    EECivilizationUpgradeValue{theRules.mPowersLevelValue, gCivPowersManager.mPowers[theApply.mPower].mBonus};
            }
        if (!EEGetRules().mCivBuilder.mCategories.empty())
            this->mCategoryIDs.back() = theRules.mPowersID;
    }
    inFile->Close();
    return theResult;
}

// FUNCTION: EXE 0x681672
// Reads the premade civilizations, sorts them by index and records the distinct era name ids in order
long EECivilizationManager::LoadPremadeCivilizations(FSFile* inFile)
{
    unsigned long theRead;
    std::vector<EECivilizationEntry> theCivs;
    unsigned long theCount = 0;
    theCivs.clear();
    this->mEraNameIDs.clear();
    this->mCivilizations.clear();
    inFile->Open(kFileAccessRead);
    long theResult = inFile->Read(theCount);
    if (theResult == kErrorOK) {
        for (unsigned long i = 0; i < theCount; i++) {
            EECivilizationEntry theEntry;
            memset(&theEntry, 0, sizeof(EECivilizationEntry));
            theResult = inFile->Read(reinterpret_cast<unsigned char*>(&theEntry), sizeof(EECivilizationEntry), theRead);
            theCivs.push_back(theEntry);
        }
    }
    inFile->Close();
    if (theCivs.size() <= 0)
        return theResult;
    std::sort(theCivs.begin(), theCivs.end(), EECMPremadeCivSorter());
    for (unsigned long j = 0; j < theCivs.size(); j++) {
        long theType = theCivs[j].mEraNameID;
        if (std::find(this->mEraNameIDs.begin(), this->mEraNameIDs.end(), theType) == this->mEraNameIDs.end())
            this->mEraNameIDs.push_back(theCivs[j].mEraNameID);
        this->mCivilizations.push_back(theCivs[j]);
    }
    return theResult;
}

// FUNCTION: EXE 0x681856
// Destroys a premade civilization sorter (scalar deleting destructor)
// (emitted after its first user)

// FUNCTION: EXE 0x681873
// Returns the extra cost per pick of the upgrade's category
long EECivilizationManager::GetExtraCost(EECivUpgrade inUpgrade)
{
    unsigned long theCategory = 0;
    this->SplitAttribute(inUpgrade, &theCategory, NULL);
    return this->mCategories[theCategory].mCost;
}

// FUNCTION: EXE 0x68189e
// Counts the upgrades in the list that share the upgrade's category
long EECivilizationManager::CountInCategory(EECivUpgrade inUpgrade, std::vector<EECivUpgrade>* inList)
{
    unsigned long theCategory = 0;
    unsigned long theOther = 0;
    long theCount = 0;
    this->SplitAttribute(inUpgrade, &theCategory, NULL);
    for (std::vector<EECivUpgrade>::iterator i = inList->begin(); i != inList->end(); i++) {
        this->SplitAttribute(*i, &theOther, NULL);
        if (theOther == theCategory)
            theCount++;
    }
    return theCount;
}

// FUNCTION: EXE 0x6818f9
// Copies every premade civilization
void EECivilizationManager::GetCivilizations(std::vector<EECivilizationEntry>& outCivs)
{
    outCivs.clear();
    for (unsigned long i = 0; i < this->mCivilizations.size(); i++)
        outCivs.push_back(this->mCivilizations[i]);
}

// FUNCTION: EXE 0x681945
// Copies the premade civilizations of one era name id
void EECivilizationManager::GetCivilizationsOfType(std::vector<EECivilizationEntry>& outCivs, long inType)
{
    outCivs.clear();
    for (unsigned long i = 0; i < this->mCivilizations.size(); i++) {
        if (this->mCivilizations[i].mEraNameID == inType)
            outCivs.push_back(this->mCivilizations[i]);
    }
}

// FUNCTION: EXE 0x68199b
// Fills a list box with one header per era and a line per premade civilization of that era
void EECivilizationManager::FillCivilizationList(EECBUIListBox* inList)
{
    std::vector<EECivilizationEntry> theCivs;
    UWideString theCategory;
    UWideString theName;
    inList->DeleteAllEntries();
    for (unsigned long i = 0; i < this->mEraNameIDs.size(); i++) {
        if (gResource->GetString(this->mEraNameIDs[i], theCategory) != 0)
            continue;
        // a category's header line, then one line per premade civilization of that type
        inList->AddLine(theCategory + EECBUIListBox::sSeparator + "0" + EECBUIListBox::sSeparator +
                        EECBUIListBox::sCategory);
        this->GetCivilizationsOfType(theCivs, this->mEraNameIDs[i]);
        for (unsigned long j = 0; j < theCivs.size(); j++) {
            if (gResource->GetString(theCivs[j].mNameID, theName) != 0)
                continue;
            inList->AddLine(theCategory + EECBUIListBox::sSeparator + theName + UWideString("/ / / / / "));
        }
    }
    if (inList->GetTabable() == true)
        inList->ChangeSelectionIndex(0, false, false);
}

// FUNCTION: EXE 0x681bc5
// Tells whether a file is a premade civilization whose checksum (name characters, upgrade ids, their count)
// matches; reports the premade civilization's name id (0 when the file is not premade)
// ecx unused: it reads gCivilizationManager
bool EECivilizationManager::MatchesPremadeCivilization(const UWideString& inFile, const UWideString& inName,
                                     std::vector<EECivUpgrade>& inUpgrades, long* outNameID)
{
    bool theResult = false;
    long theChecksum = inUpgrades.size();
    *outNameID = 0;
    for (int i = 0; i < inName.GetLength(); i++)
        theChecksum += inName[i];
    for (std::vector<EECivUpgrade>::iterator j = inUpgrades.begin(); j != inUpgrades.end(); j++)
    {   // the premade checksums sum the legacy (category << 6) | level ids
        unsigned long theLegacy = 0;
        EECivilizationManager::ToLegacy(*j, theLegacy);
        theChecksum += theLegacy;
    }
    for (unsigned long k = 0; k < gCivilizationManager.GetNumCivilizations(); k++) {
        EECivilizationEntry theEntry = gCivilizationManager.mCivilizations[k];
        if (!inFile.Equals(theEntry.mFileName))
            continue;
        if (theEntry.mChecksum == theChecksum)
            theResult = true;
        *outNameID = theEntry.mNameID;
        break;
    }
    return theResult;
}

// FUNCTION: EXE 0x681c92
// Reads a category's name string id (categories 0..0x22)
bool EECivilizationManager::GetCategoryNameID(unsigned long inCategory, long* outNameID)
{
    bool theResult = false;
    if (inCategory < this->mCategoryCount) {
        *outNameID = this->mCategories[inCategory].mNameID;
        theResult = true;
    }
    return theResult;
}

// FUNCTION: EXE 0x681cb2
// Runs a functor over every upgrade of every category until it returns false
bool EECivilizationManager::ForEachUpgrade(EECMUpgradeFunctor& inFunctor)
{
    bool theResult = true;
    for (unsigned long i = 0; i < this->mCategoryCount && theResult; i++)
        theResult = this->ForEachUpgradeOfCategory(i, inFunctor);
    return theResult;
}

// FUNCTION: EXE 0x681cd5
// Runs a functor over the 0x3c upgrade levels of a category until it returns false
bool EECivilizationManager::ForEachUpgradeOfCategory(unsigned long inCategory, EECMUpgradeFunctor& inFunctor)
{
    bool theResult = false;
    bool theContinue = true;
    if (inCategory < this->mCategoryCount) {
        for (unsigned long i = 1; i <= this->GetLevelCount(inCategory) && theContinue; i++) {
            EECivUpgrade theUpgrade = (EECivUpgrade)this->MakeUpgrade(inCategory, i);
            theContinue = inFunctor(theUpgrade, inCategory, i);
        }
        theResult = theContinue;
    }
    return theResult;
}

// FUNCTION: EXE 0x681d18
// Reads an upgrade's value and bonus from its category level table
bool EECivilizationManager::LookupLevel(EECivUpgrade inUpgrade, float* outValue, float* outBonus)
{
    return this->GetCategoryLevel(inUpgrade >> kCivUpgradeLevelBits, inUpgrade & kCivUpgradeLevelMask, outValue,
                                  outBonus);
}

// FUNCTION: EXE 0x681d37
// Reads the value and bonus of a category's level (1-based); false for an unknown category
bool EECivilizationManager::GetCategoryLevel(unsigned long inCategory, unsigned long inIndex, float* outValue,
                                     float* outBonus)
{
    EECivilizationCategoryEntry* theEntry = NULL;
    if (this->GetCategory(inCategory, &theEntry) == false)
        return false;
    // a level past the category's (the original read past its 60): zero
    const std::vector<EECivilizationUpgradeValue>& theLevels = this->mLevels[inCategory];
    bool theKnown = inIndex >= 1 && inIndex <= theLevels.size();
    if (outValue)
        *outValue = theKnown ? theLevels[inIndex - 1].mValue : 0.0f;
    if (outBonus)
        *outBonus = theKnown ? theLevels[inIndex - 1].mBonus : 0.0f;
    return true;
}

// FUNCTION: EXE 0x681d79
// Returns a category's record; false past the loaded categories
bool EECivilizationManager::GetCategory(unsigned long inCategory, EECivilizationCategoryEntry** outEntry)
{
    bool theResult = false;
    if (inCategory < this->mCategoryCount) {
        *outEntry = &this->mCategories[inCategory];
        theResult = true;
    }
    return theResult;
}

// FUNCTION: EXE 0x681d9c
// Builds an upgrade id from a category and a level index (category << 6 | index)
unsigned long EECivilizationManager::MakeUpgrade(unsigned long inCategory, unsigned long inIndex)
{
    return (inCategory << kCivUpgradeLevelBits) + (inIndex & kCivUpgradeLevelMask);
}

unsigned long EECivilizationManager::GetLevelCount(unsigned long inCategory) const
{
    return inCategory < this->mLevels.size() ? (unsigned long)this->mLevels[inCategory].size() : 0;
}

long EECivilizationManager::LevelNameID(unsigned long inLevel)
{
    const EECivAttribute* theAttribute = EEGetRules().mCivBuilder.Find((long)inLevel);
    if (theAttribute != NULL && theAttribute->mNameID != 0)
        return theAttribute->mNameID;
    return inLevel <= 0x25 ? inLevel + IDS_CIV_BONUS_BASE : inLevel + IDS_CIV_POWER_BASE;
}

bool EECivilizationManager::IsPowerLevel(unsigned long inLevel)
{
    const EECivAttribute* theAttribute = EEGetRules().mCivBuilder.Find((long)inLevel);
    if (theAttribute == NULL)
        return inLevel >= 0x26;
    for (const EECivApply& theApply : theAttribute->mApplies)
        if (theApply.mStat == kCivStatPower)
            return true;
    return false;
}

unsigned long EECivilizationManager::PowerLevel(unsigned long inPowerID)
{
    for (const EECivAttribute& theAttribute : EEGetRules().mCivBuilder.mAttributes)
        for (const EECivApply& theApply : theAttribute.mApplies)
            if (theApply.mStat == kCivStatPower && (unsigned long)(theApply.mPower + kCivPowerFlamingArrows) == inPowerID)
                return (unsigned long)theAttribute.mLevel;
    return inPowerID;
}

EECivUpgrade EECivilizationManager::FromLegacy(unsigned long inLegacy)
{
    return (EECivUpgrade)(((inLegacy >> 6) << kCivUpgradeLevelBits) | (inLegacy & 0x3f));
}

bool EECivilizationManager::ToLegacy(EECivUpgrade inUpgrade, unsigned long& outLegacy)
{
    unsigned long theCategory = inUpgrade >> kCivUpgradeLevelBits;
    unsigned long theLevel = inUpgrade & kCivUpgradeLevelMask;
    outLegacy = (theCategory << 6) | (theLevel & 0x3f);
    return theCategory < 0x400 && theLevel < 0x40;
}

bool EECivilizationManager::UpgradeKey(EECivUpgrade inUpgrade, std::string& outKey) const
{
    unsigned long theCategory = inUpgrade >> kCivUpgradeLevelBits;
    const EECivAttribute* theAttribute = EEGetRules().mCivBuilder.Find((long)(inUpgrade & kCivUpgradeLevelMask));
    if (theCategory >= this->mCategoryIDs.size() || this->mCategoryIDs[theCategory].empty() || theAttribute == NULL)
        return false;
    outKey = this->mCategoryIDs[theCategory] + "." + theAttribute->mID;
    return true;
}

bool EECivilizationManager::FindUpgradeKey(const std::string& inKey, EECivUpgrade& outUpgrade) const
{
    size_t theDot = inKey.find('.');
    if (theDot == std::string::npos)
        return false;
    const EECivAttribute* theAttribute = EEGetRules().mCivBuilder.FindID(inKey.substr(theDot + 1));
    if (theAttribute == NULL)
        return false;
    for (unsigned long i = 0; i < this->mCategoryIDs.size(); i++)
        if (this->mCategoryIDs[i] == inKey.substr(0, theDot)) {
            outUpgrade = (EECivUpgrade)((i << kCivUpgradeLevelBits) | ((unsigned long)theAttribute->mLevel & kCivUpgradeLevelMask));
            return true;
        }
    return false;
}

// FUNCTION: EXE 0x681db1
// Builds the upgrade id of a civ power from its index
unsigned long EECivilizationManager::MakePowerUpgrade(unsigned long inIndex)
{
    return (inIndex & kCivUpgradeLevelMask) + (sPowersCategory << kCivUpgradeLevelBits);
}

// FUNCTION: EXE 0x681dc7
// Splits an upgrade id into its category and level index
void EECivilizationManager::SplitAttribute(EECivUpgrade inUpgrade, unsigned long* outCategory, unsigned long* outIndex)
{
    if (outCategory)
        *outCategory = inUpgrade >> kCivUpgradeLevelBits;
    if (outIndex)
        *outIndex = inUpgrade & kCivUpgradeLevelMask;
}

// FUNCTION: EXE 0x681ded
// Returns every upgrade id of every category
std::vector<EECivUpgrade> EECivilizationManager::GetUpgrades()
{
    EECMGetUpgrades theFunctor;
    this->ForEachUpgrade(theFunctor);
    return theFunctor.mUpgrades;
}

// FUNCTION: EXE 0x681e3b
// Appends an upgrade id to the collected list and continues the enumeration
// (inline, out of line after its first user)

// FUNCTION: EXE 0x681e4d
// Applies an upgrade for a player to every unit class of its category's unit set; false without a level value
bool EECivilizationManager::ApplyUpgrade(EECivUpgrade inUpgrade, long inPlayer)
{
    float theValue = 0.0f;
    unsigned long theCategory;
    unsigned long theIndex;
    bool theResult = false;
    this->SplitAttribute(inUpgrade, &theCategory, &theIndex);
    long theSet = this->mCategories[theCategory].mUnitSet;
    if (this->LookupLevel(inUpgrade, &theValue, NULL)) {
        EECBProcessUpgrade* theFunctor = new EECBProcessUpgrade(inPlayer, theIndex, theValue);
        gUnitSetManager.ForEach(theSet, theFunctor);
        delete theFunctor;
        theResult = true;
    }
    return theResult;
}

// a float stat by a non-share mode: + the value, or x (1 + the value)
static float EECBModeValue(float inCurrent, long inMode, float inValue)
{
    return inMode == kCivModeAdd ? inCurrent + inValue : inCurrent * (1.0f + inValue);
}

// an integer stat by a non-share mode (rounded)
static long EECBModeValue(long inCurrent, long inMode, float inValue)
{
    if (inMode == kCivModeAdd)
        return inCurrent + (long)(inValue + (inValue < 0.0f ? -0.5f : 0.5f));
    return (long)(inCurrent * (1.0f + inValue) + 0.5f);
}

// One stat of an attribute (civbuilder.yaml applies): the share mode is the original's rule for that stat (the
// switch EECivilizationManager::ApplyUpgradeLevel had: the base value is the class's)
static void EECBApplyStat(const EECivApply& inApply, EEUnitClassUpgrade* inUpgrade, long inUnitType, float inValue,
                          EEPlayer* inPlayer)
{
    EEUnitStatsScope theStatsScope(inUpgrade);   // New (R5b.4, the fourteenth round): the stats table (ee/EEUnitStore.h)
    EEUnitClass* theClass = inUpgrade->mUnitClass;
    long theMode = inApply.mMode;
    bool theShare = theMode == kCivModeShare;
    switch (inApply.mStat) {
    case kCivStatAttack:
        if (!theShare) {
            inUpgrade->mAttack = EECBModeValue(inUpgrade->mAttack, theMode, inValue);
        } else {
            long theBase = theClass->mAttack;
            if (theBase != 0) {
                float theAdd = theBase * inValue;
                if (theAdd < 1.0f)
                    theAdd = 1.0f;
                inUpgrade->mAttack += theAdd;
            }
        }
        break;
    case kCivStatHitPoints:
        EEUpgradeHitPoints(inUpgrade, inUnitType, inValue, inPlayer);
        break;
    case kCivStatReloadTime:
        if (!theShare)
            inUpgrade->mReloadTime = EECBModeValue(inUpgrade->mReloadTime, theMode, inValue);
        else
            inUpgrade->mReloadTime -= (long)(theClass->mReloadTime * inValue + 0.5f);
        break;
    case kCivStatArmor:
        EECBUpgradeArmor(inUpgrade, 0, inValue);
        EECBUpgradeArmor(inUpgrade, 1, inValue);
        EECBUpgradeArmor(inUpgrade, 2, inValue);
        EECBUpgradeArmor(inUpgrade, 3, inValue);
        EECBUpgradeArmor(inUpgrade, 4, inValue);
        EECBUpgradeArmor(inUpgrade, 5, inValue);
        break;
    case kCivStatRange:
        if (!theShare) {
            inUpgrade->SetRange(EECBModeValue(inUpgrade->mRange, theMode, inValue));
        } else {
            float theAdd = theClass->mRange * inValue;
            if (theAdd < 1.0f)
                theAdd = 1.0f;
            inUpgrade->SetRange(inUpgrade->mRange + theAdd);
        }
        break;
    case kCivStatLineOfSight:
        if (!theShare) {
            inUpgrade->mLineOfSight = EECBModeValue(inUpgrade->mLineOfSight, theMode, inValue);
        } else {
            float theOther = theClass->mLineOfSight * inValue;
            if (theOther < 1.0f)
                theOther = 1.0f;
            inUpgrade->mLineOfSight += theOther;
        }
        break;
    case kCivStatSpeed:
        if (!theShare)
            inUpgrade->SetMaxVelocity(EECBModeValue(inUpgrade->mMaxVelocity, theMode, inValue));
        else
            inUpgrade->SetMaxVelocity(theClass->GetMaxVelocity() * inValue + inUpgrade->mMaxVelocity);
        break;
    case kCivStatFlightTime:
        if (!theShare) {
            inUpgrade->mFlightTime = EECBModeValue(inUpgrade->mFlightTime, theMode, inValue);
        } else {
            long theAdd = (long)(theClass->mFlightTime * inValue + 0.5f);
            if (theAdd < 1)
                theAdd = 1;
            inUpgrade->mFlightTime += theAdd;
        }
        break;
    case kCivStatAreaEffect:
        if (!theShare)
            inUpgrade->mAreaEffect = EECBModeValue(inUpgrade->mAreaEffect, theMode, inValue);
        else
            inUpgrade->mAreaEffect += theClass->mAreaEffect * inValue;
        break;
    case kCivStatGatherTime:
        if (!theShare)
            inUpgrade->mGatherTimes[inApply.mSlot] = EECBModeValue(inUpgrade->mGatherTimes[inApply.mSlot], theMode, inValue);
        else
            inUpgrade->mGatherTimes[inApply.mSlot] -= (long)(theClass->mGatherTimes[inApply.mSlot] * inValue + 0.5f);
        break;
    case kCivStatBuildTime:
        inUpgrade->mBuildTimeScale = theShare ? inUpgrade->mBuildTimeScale * (1.0f - inValue)
                                              : EECBModeValue(inUpgrade->mBuildTimeScale, theMode, inValue);
        break;
    case kCivStatConversion:
        inPlayer->mConversionFactor = theShare ? inPlayer->mConversionFactor * inValue
                                               : EECBModeValue(inPlayer->mConversionFactor, theMode, inValue);
        break;
    case kCivStatRepairRate:
        if (!theShare) {
            inUpgrade->mRepairRate = EECBModeValue(inUpgrade->mRepairRate, theMode, inValue);
        } else {
            long theAdd = (long)(theClass->mRepairRate * inValue + 0.5f);
            if (theAdd < 1)
                theAdd = 1;
            inUpgrade->mRepairRate += theAdd;
        }
        break;
    case kCivStatAreaEffectScale:
        if (!theShare) {
            inUpgrade->mAreaEffectScale = EECBModeValue(inUpgrade->mAreaEffectScale, theMode, inValue);
        } else {
            float theAdd = theClass->mAreaEffectScale * inValue;
            if (theAdd < 1.0f)
                theAdd = 1.0f;
            inUpgrade->mAreaEffectScale += theAdd;
        }
        inPlayer->SuggestRecomputeAllAreaEffects();
        inPlayer->mSelectedGroup->ShowSelectionAreas();
        break;
    case kCivStatHighGround:
        inPlayer->mHighGroundBonus = theShare ? inPlayer->mHighGroundBonus + inValue
                                              : EECBModeValue(inPlayer->mHighGroundBonus, theMode, inValue);
        break;
    case kCivStatPopulation:
        inPlayer->GetPopulationLimit(0);
        if (theShare)
            inPlayer->mShareBonus += (long)(inPlayer->GetPopulationLimit(0) * inValue + 0.5f);
        else
            inPlayer->mShareBonus = EECBModeValue(inPlayer->mShareBonus, theMode, inValue);
        inPlayer->GetPopulationLimit(0);
        break;
    case kCivStatCost:
        inUpgrade->mCostScale = theShare ? inUpgrade->mCostScale * (1.0f - inValue)
                                         : EECBModeValue(inUpgrade->mCostScale, theMode, inValue);
        break;
    }
}

// FUNCTION: EXE 0x681ed3
// Applies one upgrade level to a player's unit class (attack, hit points, reload, armor, range and sight,
// speed, gathering, build time, cost ...); levels 0x26..0x3c apply the civ power instead
// upgrade index inIndex (1..0x17; 0x26..0x3c are the civilization powers) applied with inValue to inUpgrade, the
// class inUnitType of inPlayer (provisional name)
// What each level raises is civbuilder.yaml's (the attribute of that level: its applies; the original's switch on the
// level is the vanilla data). An attribute that applies a power applies only its power; one that raises the line of
// sight takes the class's units' sight off around it.
void EECivilizationManager::ApplyUpgradeLevel(EEUnitClassUpgrade* inUpgrade, unsigned long inIndex, long inUnitType, float inValue,
                                     EEPlayer* inPlayer)
{
    // the current turn (a volatile read the original performs; its use is gone from the shipping build)
    unsigned long theTurn = gCommunications->GetNumberOfCTsProcessedInPhysics();
    const EECivAttribute* theAttribute = EEGetRules().mCivBuilder.Find((long)inIndex);
    if (theAttribute == NULL)
        return;
    bool theSight = false;
    for (const EECivApply& theApply : theAttribute->mApplies) {
        if (theApply.mStat == kCivStatPower) {
            gCivPowersManager.ApplyPower(inPlayer, theApply.mPower + kCivPowerFlamingArrows);
            return;
        }
        theSight = theSight || theApply.mStat == kCivStatLineOfSight;
    }
    if (theSight) {
        for (EEUnit* theUnit = inPlayer->mFirstUnit; theUnit != NULL; theUnit = static_cast<EEUnit*>(theUnit->mNext)) {
            if (theUnit->mUnitClass->mUnitType == inUnitType)
                theUnit->SightOffHere();
        }
    }
    for (const EECivApply& theApply : theAttribute->mApplies) {
        long theCategory = inUpgrade->mUnitClass->mCategory;
        if ((theApply.mOnlyCategory >= 0 && theCategory != theApply.mOnlyCategory) ||
            (theApply.mExceptCategory >= 0 && theCategory == theApply.mExceptCategory))
            continue;
        EECBApplyStat(theApply, inUpgrade, inUnitType, inValue, inPlayer);
    }
    if (!theSight)
        return;
    for (EEUnit* theUnit = inPlayer->mFirstUnit; theUnit != NULL; theUnit = static_cast<EEUnit*>(theUnit->mNext)) {
        if (theUnit->mUnitClass->mUnitType == inUnitType)
            theUnit->SightOnHere();
    }
}

// FUNCTION: EXE 0x68239e
// Raises one armor kind of a class upgrade by a share of the class's base armor (at least 1)
// NEAR: the final add is fld [mem]; faddp in the original, fadd [mem] here (+=, = x + y and a float& all give
// fadd [mem])
void EECBUpgradeArmor(EEUnitClassUpgrade* inUpgrade, long inKind, float inValue)
{
    EEUnitStatsScope theStatsScope(inUpgrade);   // New (R5b.4, the fourteenth round): the stats table (ee/EEUnitStore.h)
    long theAdd = (long)(inUpgrade->mUnitClass->mArmor[inKind] * inValue + 0.5f);
    if (theAdd < 1)
        theAdd = 1;
    inUpgrade->mArmor[inKind] += theAdd;
}

// FUNCTION: EXE 0x6823e2
// Creates the per-class upgrade functor for a player, level index and value
EECBProcessUpgrade::EECBProcessUpgrade(long inPlayer, unsigned long inIndex, float inValue)
{
    this->mPlayer = ::gWorld.mPlayers[inPlayer];
    this->mIndex = inIndex;
    this->mValue = inValue;
}

// FUNCTION: EXE 0x682409
// Applies the upgrade level to the player's upgrade of one unit class (hit points scaled by the game variant)
bool EECBProcessUpgrade::Execute(long inUnitType)
{
    if (gUnitClassTable[inUnitType] != NULL) {
        EEUnitClassUpgrade* theUpgrade = this->mPlayer->mUnitClassUpgrades[inUnitType];
        float theValue;
        if (this->mIndex == 2)
            theValue = (::gWorld.GetGameVariant()->GetFactor(inUnitType) + 1.0f) * this->mValue;
        else
            theValue = this->mValue;
        gCivilizationManager.ApplyUpgradeLevel(theUpgrade, this->mIndex, inUnitType, theValue, this->mPlayer);
    }
    return true;
}


// FUNCTION: EXE 0x68246c
// Standard library code for the premade civilization list (vector or sort template COMDAT)

// FUNCTION: EXE 0x682498
// Standard library code for the premade civilization list (vector or sort template COMDAT)

// FUNCTION: EXE 0x6824d3
// Standard library code for the premade civilization list (vector or sort template COMDAT)

// FUNCTION: EXE 0x68252a
// Standard library code for the premade civilization list (vector or sort template COMDAT)

// FUNCTION: EXE 0x68256e
// Standard library code for the premade civilization list (vector or sort template COMDAT)

// FUNCTION: EXE 0x682829
// Standard library code for the premade civilization list (vector or sort template COMDAT)

// FUNCTION: EXE 0x68283d
// Standard library code for the premade civilization list (vector or sort template COMDAT)

// FUNCTION: EXE 0x682920
// Standard library code for the premade civilization list (vector or sort template COMDAT)

// FUNCTION: EXE 0x682931
// Standard library code for the premade civilization list (vector or sort template COMDAT)

// FUNCTION: EXE 0x68295b
// Standard library code for the premade civilization list (vector or sort template COMDAT)

// FUNCTION: EXE 0x68298d
// Standard library code for the premade civilization list (vector or sort template COMDAT)

// FUNCTION: EXE 0x6829b0
// Standard library code for the premade civilization list (vector or sort template COMDAT)

// FUNCTION: EXE 0x6829df
// Standard library code for the premade civilization list (vector or sort template COMDAT)

// FUNCTION: EXE 0x6829f5
// Standard library code for the premade civilization list (vector or sort template COMDAT)

// FUNCTION: EXE 0x682a00
// Standard library code for the premade civilization list (vector or sort template COMDAT)

// FUNCTION: EXE 0x682a32
// Standard library code for the premade civilization list (vector or sort template COMDAT)

// FUNCTION: EXE 0x682b22
// Standard library code for the premade civilization list (vector or sort template COMDAT)

// FUNCTION: EXE 0x682b6f
// Standard library code for the premade civilization list (vector or sort template COMDAT)

// FUNCTION: EXE 0x682c3b
// Standard library code for the premade civilization list (vector or sort template COMDAT)

// FUNCTION: EXE 0x682c41
// Standard library code for the premade civilization list (vector or sort template COMDAT)

// FUNCTION: EXE 0x682c87
// Standard library code for the premade civilization list (vector or sort template COMDAT)

// FUNCTION: EXE 0x682ccc
// Standard library code for the premade civilization list (vector or sort template COMDAT)

// FUNCTION: EXE 0x682dc7
// Standard library code for the premade civilization list (vector or sort template COMDAT)

// FUNCTION: EXE 0x682df0
// Standard library code for the premade civilization list (vector or sort template COMDAT)

// FUNCTION: EXE 0x682e5d
// Standard library code for the premade civilization list (vector or sort template COMDAT)

// FUNCTION: EXE 0x682e8f
// Standard library code for the premade civilization list (vector or sort template COMDAT)

// FUNCTION: EXE 0x682eb4
// Standard library code for the premade civilization list (vector or sort template COMDAT)

// FUNCTION: EXE 0x682f4b
// Standard library code for the premade civilization list (vector or sort template COMDAT)

// FUNCTION: EXE 0x682fdf
// Standard library code for the premade civilization list (vector or sort template COMDAT)

// FUNCTION: EXE 0x68300c
// Standard library code for the premade civilization list (vector or sort template COMDAT)

// FUNCTION: EXE 0x68305e
// Standard library code for the premade civilization list (vector or sort template COMDAT)

// FUNCTION: EXE 0x6830ae
// Runs a functor on every unit type of a unit set (EEUnitSetManager::ForEach, emitted after ApplyUpgrade)
