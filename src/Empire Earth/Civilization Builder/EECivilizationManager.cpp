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
    virtual bool operator()(unsigned short inUpgrade, unsigned long inCategory, unsigned long inIndex) = 0;
};

class EECMGetUpgrades : public EECMUpgradeFunctor {
public:
    virtual bool operator()(unsigned short inUpgrade, unsigned long inCategory, unsigned long inIndex)
    {
        this->mUpgrades.push_back(inUpgrade);
        return true;
    }

    std::vector<unsigned short> mUpgrades;  // +0x04
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
// the civilization powers' level value (0x44bfcc; Load) (provisional name)
static const float kPowerLevelValue = 0.01f;

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
    memset(this->mCategories, 0, sizeof(this->mCategories));
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
    memcpy(&this->mCategories[this->mCategoryCount], &inEntry, sizeof(EECivilizationCategoryEntry));
    this->mCategoryCount++;
}

// FUNCTION: EXE 0x681514
// Reads the upgrade categories from the civilization file, then appends the civ powers category (one level per
// power 0x26..0x3c, cost from world table entry 0x56)
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
            theLastU00 = theEntry.mDatabaseID;
            theLastU04 = theEntry.mIndex;
        }
        // the civilization powers' category: one level per power 0x26..0x3c
        EECivilizationCategoryEntry thePowers;
        memset(&thePowers, 0, sizeof(EECivilizationCategoryEntry));
        thePowers.mDatabaseID = theLastU00 + 1;
        thePowers.mIndex = theLastU04 + 1;
        thePowers.mNameID = IDS_SPECIAL_POWERS;
        thePowers.mUnitSet = 0x66;
        thePowers.mCost = gWorldTable.GetLongValue(kWorldSpecialPowersCategoryCost);
        EECivilizationUpgradeValue* theLevel = &thePowers.mLevels[0x25];
        unsigned long j = 0;
        while (j < 0x17) {
            EECivPower thePower = *gCivPowersManager.GetPowerRecord(j + 0x26);
            j++;
            theLevel->mBonus = thePower.mBonus;
            theLevel->mValue = kPowerLevelValue;
            theLevel++;
        }
        sPowersCategory = this->mCategoryCount;
        this->AddCategory(thePowers);
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
long EECivilizationManager::GetExtraCost(unsigned short inUpgrade)
{
    unsigned long theCategory = 0;
    this->SplitAttribute(inUpgrade, &theCategory, NULL);
    return this->mCategories[theCategory].mCost;
}

// FUNCTION: EXE 0x68189e
// Counts the upgrades in the list that share the upgrade's category
long EECivilizationManager::CountInCategory(unsigned short inUpgrade, std::vector<unsigned short>* inList)
{
    unsigned long theCategory = 0;
    unsigned long theOther = 0;
    long theCount = 0;
    this->SplitAttribute(inUpgrade, &theCategory, NULL);
    for (std::vector<unsigned short>::iterator i = inList->begin(); i != inList->end(); i++) {
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
                                     std::vector<unsigned short>& inUpgrades, long* outNameID)
{
    bool theResult = false;
    long theChecksum = inUpgrades.size();
    *outNameID = 0;
    for (int i = 0; i < inName.GetLength(); i++)
        theChecksum += inName[i];
    for (std::vector<unsigned short>::iterator j = inUpgrades.begin(); j != inUpgrades.end(); j++)
        theChecksum += *j;
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
    if (inCategory < 0x23) {
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
    for (unsigned long i = 0; i < 0x23 && theResult; i++)
        theResult = this->ForEachUpgradeOfCategory(i, inFunctor);
    return theResult;
}

// FUNCTION: EXE 0x681cd5
// Runs a functor over the 0x3c upgrade levels of a category until it returns false
bool EECivilizationManager::ForEachUpgradeOfCategory(unsigned long inCategory, EECMUpgradeFunctor& inFunctor)
{
    bool theResult = false;
    bool theContinue = true;
    if (inCategory < 0x23) {
        for (unsigned long i = 1; i <= 0x3c && theContinue; i++) {
            unsigned short theUpgrade = (unsigned short)this->MakeUpgrade(inCategory, (unsigned char)i);
            theContinue = inFunctor(theUpgrade, inCategory, i);
        }
        theResult = theContinue;
    }
    return theResult;
}

// FUNCTION: EXE 0x681d18
// Reads an upgrade's value and bonus from its category level table
bool EECivilizationManager::LookupLevel(unsigned short inUpgrade, float* outValue, float* outBonus)
{
    return this->GetCategoryLevel(inUpgrade >> 6, inUpgrade & 0x3f, outValue, outBonus);
}

// FUNCTION: EXE 0x681d37
// Reads the value and bonus of a category's level (1-based); false for an unknown category
bool EECivilizationManager::GetCategoryLevel(unsigned long inCategory, unsigned long inIndex, float* outValue,
                                     float* outBonus)
{
    EECivilizationCategoryEntry* theEntry = NULL;
    if (this->GetCategory(inCategory, &theEntry) == false)
        return false;
    if (outValue)
        *outValue = theEntry->mLevels[inIndex - 1].mValue;
    if (outBonus)
        *outBonus = theEntry->mLevels[inIndex - 1].mBonus;
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
unsigned long EECivilizationManager::MakeUpgrade(unsigned long inCategory, unsigned char inIndex)
{
    return (inCategory << 6) + (inIndex & 0x3f);
}

// FUNCTION: EXE 0x681db1
// Builds the upgrade id of a civ power from its index
unsigned long EECivilizationManager::MakePowerUpgrade(unsigned char inIndex)
{
    return (inIndex & 0x3f) + (sPowersCategory << 6);
}

// FUNCTION: EXE 0x681dc7
// Splits an upgrade id into its category and level index
void EECivilizationManager::SplitAttribute(unsigned short inUpgrade, unsigned long* outCategory, unsigned long* outIndex)
{
    if (outCategory)
        *outCategory = inUpgrade >> 6;
    if (outIndex)
        *outIndex = inUpgrade & 0x3f;
}

// FUNCTION: EXE 0x681ded
// Returns every upgrade id of every category
std::vector<unsigned short> EECivilizationManager::GetUpgrades()
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
bool EECivilizationManager::ApplyUpgrade(unsigned short inUpgrade, long inPlayer)
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

// FUNCTION: EXE 0x681ed3
// Applies one upgrade level to a player's unit class (attack, hit points, reload, armor, range and sight,
// speed, gathering, build time, cost ...); levels 0x26..0x3c apply the civ power instead
// upgrade index inIndex (1..0x17; 0x26..0x3c are the civilization powers) applied with inValue to inUpgrade, the
// class inUnitType of inPlayer (provisional name)
void EECivilizationManager::ApplyUpgradeLevel(EEUnitClassUpgrade* inUpgrade, unsigned long inIndex, long inUnitType, float inValue,
                                     EEPlayer* inPlayer)
{
    // the current turn (a volatile read the original performs; its use is gone from the shipping build)
    unsigned long theTurn = gCommunications->GetNumberOfCTsProcessedInPhysics();
    if (inIndex > 0x25 && inIndex <= 0x3c) {
        gCivPowersManager.ApplyPower(inPlayer, inIndex);
        return;
    }
    if (inIndex == 5) {
        for (EEUnit* theUnit = inPlayer->mFirstUnit; theUnit != NULL; theUnit = static_cast<EEUnit*>(theUnit->mNext)) {
            if (theUnit->mUnitClass->mUnitType == inUnitType)
                theUnit->SightOffHere();
        }
    }
    switch (inIndex) {
    case 1: {
        long theBase = inUpgrade->mUnitClass->mAttack;
        if (theBase != 0) {
            float theAdd = theBase * inValue;
            if (theAdd < 1.0f)
                theAdd = 1.0f;
            inUpgrade->mAttack += theAdd;
        }
        break;
    }
    case 2:
        EEUpgradeHitPoints(inUpgrade, inUnitType, inValue, inPlayer);
        break;
    case 3:
        inUpgrade->mReloadTime -= (long)(inUpgrade->mUnitClass->mReloadTime * inValue + 0.5f);
        break;
    case 4:
        EECBUpgradeArmor(inUpgrade, 0, inValue);
        EECBUpgradeArmor(inUpgrade, 1, inValue);
        EECBUpgradeArmor(inUpgrade, 2, inValue);
        EECBUpgradeArmor(inUpgrade, 3, inValue);
        EECBUpgradeArmor(inUpgrade, 4, inValue);
        EECBUpgradeArmor(inUpgrade, 5, inValue);
        break;
    case 5: {
        float theAdd = inUpgrade->mUnitClass->mRange * inValue;
        if (theAdd < 1.0f)
            theAdd = 1.0f;
        inUpgrade->SetRange(inUpgrade->mRange + theAdd);
        float theOther = inUpgrade->mUnitClass->mLineOfSight * inValue;
        if (theOther < 1.0f)
            theOther = 1.0f;
        inUpgrade->mLineOfSight += theOther;
        break;
    }
    case 6:
        inUpgrade->SetMaxVelocity(inUpgrade->mUnitClass->GetMaxVelocity() * inValue + inUpgrade->mMaxVelocity);
        break;
    case 7: {
        long theAdd = (long)(inUpgrade->mUnitClass->mFlightTime * inValue + 0.5f);
        if (theAdd < 1)
            theAdd = 1;
        inUpgrade->mFlightTime += theAdd;
        break;
    }
    case 8:
        inUpgrade->mAreaEffect += inUpgrade->mUnitClass->mAreaEffect * inValue;
        break;
    case 9:
        if (inUpgrade->mUnitClass->mCategory != kUnitCategoryNavalCitizen) {
            inUpgrade->mGatherTimes[5] -= (long)(inUpgrade->mUnitClass->mGatherTimes[5] * inValue + 0.5f);
            inUpgrade->mGatherTimes[4] -= (long)(inUpgrade->mUnitClass->mGatherTimes[4] * inValue + 0.5f);
        }
        break;
    case 10:
        inUpgrade->mGatherTimes[0] -= (long)(inUpgrade->mUnitClass->mGatherTimes[0] * inValue + 0.5f);
        break;
    case 11:
        inUpgrade->mGatherTimes[2] -= (long)(inUpgrade->mUnitClass->mGatherTimes[2] * inValue + 0.5f);
        break;
    case 12:
        inUpgrade->mGatherTimes[6] -= (long)(inUpgrade->mUnitClass->mGatherTimes[6] * inValue + 0.5f);
        break;
    case 13:
        inUpgrade->mGatherTimes[1] -= (long)(inUpgrade->mUnitClass->mGatherTimes[1] * inValue + 0.5f);
        break;
    case 14:
        inUpgrade->mGatherTimes[3] -= (long)(inUpgrade->mUnitClass->mGatherTimes[3] * inValue + 0.5f);
        break;
    case 15:
        inUpgrade->mBuildTimeScale *= 1.0f - inValue;
        break;
    case 16:
        inPlayer->mConversionFactor *= inValue;
        break;
    case 17:
        if (inUpgrade->mUnitClass->mCategory == kUnitCategoryNavalCitizen)
            inUpgrade->mGatherTimes[4] -= (long)(inUpgrade->mUnitClass->mGatherTimes[4] * inValue + 0.5f);
        break;
    case 18: {
        long theAdd = (long)(inUpgrade->mUnitClass->mRepairRate * inValue + 0.5f);
        if (theAdd < 1)
            theAdd = 1;
        inUpgrade->mRepairRate += theAdd;
        break;
    }
    case 19: {
        float theAdd = inUpgrade->mUnitClass->mAreaEffectScale * inValue;
        if (theAdd < 1.0f)
            theAdd = 1.0f;
        inUpgrade->mAreaEffectScale += theAdd;
        inPlayer->SuggestRecomputeAllAreaEffects();
        inPlayer->mSelectedGroup->ShowSelectionAreas();
        break;
    }
    case 20:
        inPlayer->mHighGroundBonus += inValue;
        break;
    case 21:
        inPlayer->GetPopulationLimit(0);
        inPlayer->mShareBonus += (long)(inPlayer->GetPopulationLimit(0) * inValue + 0.5f);
        inPlayer->GetPopulationLimit(0);
        break;
    case 23:
        inUpgrade->mCostScale *= 1.0f - inValue;
        break;
    }
    if (inIndex != 5)
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
