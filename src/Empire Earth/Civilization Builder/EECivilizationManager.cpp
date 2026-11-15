// C:\EEX\Empire Earth\Civilization Builder\EECivilizationManager.cpp (file name from the class; no __FILE__). The
// civilization categories, premade civilizations and upgrade ids (RTTI EECivilizationManager, vtable 0x44bfd4). Range
// 0x2813f4..0x2830d0; EECivPowersManager.cpp's 0x2830d0..0x283b2e folded in (civilization builder lane, round 2).
// Whole in Rust (ee_tech civ.rs: the tables and queries; civ_apply.rs: ApplyUpgrade, ApplyUpgradeLevel with
// EECBProcessUpgrade and EECBUpgradeArmor, the powers' ApplyPower): the two classes keep their names and methods as
// forwarders, this unit reads the db tables' bytes and answers the simulation's world calls (EECivHostCall). The
// premade list's filling (a UI list) stays here. Unit flags: the exe module set.
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
#include "ee/EERules.h"
#include "ee/EERulesWorld.h"
#include "ee/EECivPowerIDs.h"
#include "ee/EECivCapture.h"
#include "ee/EEMedia.h"
#include "ee/EESoundObject.h"
#include "ee/EEGraphicEffect.h"
#include "lle/FSMemoryFile.h"
#include "rust/ee_tech.h"

// 0x24f0e9 (EEFOFindNewTargetNearOldTarget.cpp; as ee/EEUnitUtilities.h and EEWorld.cpp declare it)
void EEUpgradeHitPoints(EEUnitClassUpgrade* inUpgrade, long inType, float inFactor, EEPlayer* inPlayer);

// the C++ records are the Rust ones (ee_tech civ.rs), field for field
static_assert(sizeof(EECivilizationCategoryEntry) == sizeof(EeTechCivCategory) &&
              sizeof(EECivilizationEntry) == sizeof(EeTechCivEntry) && sizeof(EECivPower) == sizeof(EeTechCivPower) &&
              sizeof(EECivilizationUpgradeValue) == sizeof(EeTechCivLevel), "the civilization records");

// chain at 0x6813f4 (wrapper)
//   object 0x9654d0  ctor __imp_??0UString@@QAE@PBD@Z  dtor __imp_??1UString@@UAE@XZ
const UString kCivKey(".civ");

// chain at 0x681426 (wrapper)
//   object 0x9654c0  ctor __imp_??0UString@@QAE@PBD@Z  dtor __imp_??1UString@@UAE@XZ
const UString kCivilizationsKey("Civilizations");

// A db table's whole bytes (the loads read them as their records; a memory file reads nothing past its end)
static long EECivReadAll(FSFile* inFile, std::vector<unsigned char>& outBytes)
{
    long theResult = inFile->Open(kFileAccessRead);
    unsigned char theChunk[0x1000];
    unsigned long theRead = 0;
    while (theResult == kErrorOK && inFile->Read(theChunk, sizeof(theChunk), theRead) == kErrorOK)
        outBytes.insert(outBytes.end(), theChunk, theChunk + sizeof(theChunk));
    unsigned char theByte;
    while (theResult == kErrorOK && inFile->Read(theByte) == kErrorOK)
        outBytes.push_back(theByte);
    inFile->Close();
    return theResult;
}

// Creates the civilization manager (the tables are Rust's: it binds the game's rules for them)
EECivilizationManager::EECivilizationManager()
{
    ee_tech_civ_bind_rules(EEGetRules().RustRules());
}

EECivilizationManager::~EECivilizationManager()
{
}

// Reads the categories, then appends the civ powers' category (civbuilder.yaml's powers_category)
long EECivilizationManager::Load(FSFile* inFile)
{
    std::vector<unsigned char> theBytes;
    if (EECivReadAll(inFile, theBytes) != kErrorOK)
        return kErrorFailed;
    return ee_tech_civ_load(theBytes.data(), theBytes.size(), EERuleLong(EEGetRules().mCivBuilder.mPowersCost));
}

// Reads the premade civilizations, sorted by index, and their eras
long EECivilizationManager::LoadPremadeCivilizations(FSFile* inFile)
{
    std::vector<unsigned char> theBytes;
    if (EECivReadAll(inFile, theBytes) != kErrorOK)
        return kErrorFailed;
    return ee_tech_civ_load_premade(theBytes.data(), theBytes.size());
}

long EECivilizationManager::GetExtraCost(EECivUpgrade inUpgrade) { return ee_tech_civ_extra_cost(inUpgrade); }

long EECivilizationManager::CountInCategory(EECivUpgrade inUpgrade, std::vector<EECivUpgrade>* inList)
{
    return ee_tech_civ_count_in_category(inUpgrade, inList->data(), inList->size());
}

unsigned long EECivilizationManager::GetNumCivilizations() { return ee_tech_civ_civilization_count(); }

const EECivilizationEntry& EECivilizationManager::GetCivilization(unsigned long inIndex)
{
    return *reinterpret_cast<const EECivilizationEntry*>(ee_tech_civ_civilization((uint32_t)inIndex));
}

unsigned long EECivilizationManager::GetEraCount() { return ee_tech_civ_era_count(); }
long EECivilizationManager::GetEra(unsigned long inIndex) { return ee_tech_civ_era((uint32_t)inIndex); }

void EECivilizationManager::GetCivilizations(std::vector<EECivilizationEntry>& outCivs)
{
    outCivs.clear();
    for (unsigned long i = 0; i < this->GetNumCivilizations(); i++)
        outCivs.push_back(this->GetCivilization(i));
}

void EECivilizationManager::GetCivilizationsOfType(std::vector<EECivilizationEntry>& outCivs, long inType)
{
    outCivs.clear();
    for (unsigned long i = 0; i < this->GetNumCivilizations(); i++)
        if (this->GetCivilization(i).mEraNameID == inType)
            outCivs.push_back(this->GetCivilization(i));
}

// Fills a list box with one header per era and a line per premade civilization of that era
void EECivilizationManager::FillCivilizationList(EECBUIListBox* inList)
{
    std::vector<EECivilizationEntry> theCivs;
    UWideString theCategory;
    UWideString theName;
    inList->DeleteAllEntries();
    for (unsigned long i = 0; i < this->GetEraCount(); i++) {
        if (gResource->GetString(this->GetEra(i), theCategory) != 0)
            continue;
        // a category's header line, then one line per premade civilization of that type
        inList->AddLine(theCategory + EECBUIListBox::sSeparator + "0" + EECBUIListBox::sSeparator +
                        EECBUIListBox::sCategory);
        this->GetCivilizationsOfType(theCivs, this->GetEra(i));
        for (unsigned long j = 0; j < theCivs.size(); j++) {
            if (gResource->GetString(theCivs[j].mNameID, theName) != 0)
                continue;
            inList->AddLine(theCategory + EECBUIListBox::sSeparator + theName + UWideString("/ / / / / "));
        }
    }
    if (inList->GetTabable() == true)
        inList->ChangeSelectionIndex(0, false, false);
}

bool EECivilizationManager::MatchesPremadeCivilization(const UWideString& inFile, const UWideString& inName,
                                                       std::vector<EECivUpgrade>& inUpgrades, long* outNameID)
{
    int32_t theNameID = 0;
    bool theResult = ee_tech_civ_matches_premade((const char*)inFile, (const char*)inName, (uint32_t)inName.GetLength(),
                                                 inUpgrades.data(), inUpgrades.size(), &theNameID);
    *outNameID = theNameID;
    return theResult;
}

bool EECivilizationManager::GetCategoryNameID(unsigned long inCategory, long* outNameID)
{
    EECivilizationCategoryEntry* theEntry = NULL;
    if (!this->GetCategory(inCategory, &theEntry))
        return false;
    *outNameID = theEntry->mNameID;
    return true;
}

bool EECivilizationManager::LookupLevel(EECivUpgrade inUpgrade, float* outValue, float* outBonus)
{
    return this->GetCategoryLevel(inUpgrade >> kCivUpgradeLevelBits, inUpgrade & kCivUpgradeLevelMask, outValue, outBonus);
}

bool EECivilizationManager::GetCategoryLevel(unsigned long inCategory, unsigned long inIndex, float* outValue,
                                             float* outBonus)
{
    return ee_tech_civ_category_level((uint32_t)inCategory, (uint32_t)inIndex, outValue, outBonus);
}

bool EECivilizationManager::GetCategory(unsigned long inCategory, EECivilizationCategoryEntry** outEntry)
{
    EeTechCivCategory* theEntry = ee_tech_civ_category((uint32_t)inCategory);
    if (theEntry != NULL)
        *outEntry = reinterpret_cast<EECivilizationCategoryEntry*>(theEntry);
    return theEntry != NULL;
}

unsigned long EECivilizationManager::GetCategoryCount() { return ee_tech_civ_category_count(); }

std::string EECivilizationManager::GetCategoryID(unsigned long inCategory)
{
    struct Sink {
        static void Put(void* inCtx, const uint8_t* inBytes, size_t inLength)
        {
            static_cast<std::string*>(inCtx)->assign(reinterpret_cast<const char*>(inBytes), inLength);
        }
    };
    std::string theID;
    ee_tech_civ_category_id((uint32_t)inCategory, &theID, &Sink::Put);
    return theID;
}

unsigned long EECivilizationManager::MakeUpgrade(unsigned long inCategory, unsigned long inIndex)
{
    return (inCategory << kCivUpgradeLevelBits) + (inIndex & kCivUpgradeLevelMask);
}

unsigned long EECivilizationManager::GetLevelCount(unsigned long inCategory) const
{
    return ee_tech_civ_level_count((uint32_t)inCategory);
}

long EECivilizationManager::LevelNameID(unsigned long inLevel) { return ee_tech_civ_level_name_id((uint32_t)inLevel); }
bool EECivilizationManager::IsPowerLevel(unsigned long inLevel) { return ee_tech_civ_is_power_level((uint32_t)inLevel); }
unsigned long EECivilizationManager::PowerLevel(unsigned long inPowerID) { return ee_tech_civ_power_level((uint32_t)inPowerID); }
EECivUpgrade EECivilizationManager::FromLegacy(unsigned long inLegacy) { return ee_tech_civ_from_legacy((uint32_t)inLegacy); }

bool EECivilizationManager::ToLegacy(EECivUpgrade inUpgrade, unsigned long& outLegacy)
{
    uint32_t theLegacy = 0;
    bool theResult = ee_tech_civ_to_legacy(inUpgrade, &theLegacy);
    outLegacy = theLegacy;
    return theResult;
}

bool EECivilizationManager::UpgradeKey(EECivUpgrade inUpgrade, std::string& outKey) const
{
    struct Sink {
        static void Put(void* inCtx, const uint8_t* inBytes, size_t inLength)
        {
            static_cast<std::string*>(inCtx)->assign(reinterpret_cast<const char*>(inBytes), inLength);
        }
    };
    return ee_tech_civ_upgrade_key(inUpgrade, &outKey, &Sink::Put);
}

bool EECivilizationManager::FindUpgradeKey(const std::string& inKey, EECivUpgrade& outUpgrade) const
{
    return ee_tech_civ_find_upgrade_key(reinterpret_cast<const uint8_t*>(inKey.data()), inKey.size(), &outUpgrade);
}

unsigned long EECivilizationManager::MakePowerUpgrade(unsigned long inIndex) { return ee_tech_civ_power_upgrade((uint32_t)inIndex); }

void EECivilizationManager::SplitAttribute(EECivUpgrade inUpgrade, unsigned long* outCategory, unsigned long* outIndex)
{
    if (outCategory)
        *outCategory = inUpgrade >> kCivUpgradeLevelBits;
    if (outIndex)
        *outIndex = inUpgrade & kCivUpgradeLevelMask;
}

std::vector<EECivUpgrade> EECivilizationManager::GetUpgrades()
{
    struct Sink {
        static void Push(void* inCtx, uint32_t inUpgrade) { static_cast<std::vector<EECivUpgrade>*>(inCtx)->push_back(inUpgrade); }
    };
    std::vector<EECivUpgrade> theUpgrades;
    ee_tech_civ_upgrades(&theUpgrades, &Sink::Push);
    return theUpgrades;
}

// ------------------------------------------------------------------------------------------------ the simulation's host

// The world for ee_tech civ_apply.rs: the unit sets, the player's class upgrades and fields, the units' sight, the
// powers' research, media, sight pulse, camouflage and tech nodes
static void EECivHostCall(void* inCtx, const EeTechCivCall* c, EeTechCivReply* r)
{
    EEPlayer* thePlayer = ::gWorld.mPlayers[c->player];
    long theType = (long)c->a;
    EETechTreeNode* theNode;
    switch (c->op) {
    case EE_TECH_CIV_OP_SET_EXISTS:
        r->value = gUnitSetManager.GetSet((unsigned long)c->a) != NULL;
        break;
    case EE_TECH_CIV_OP_CLASS_COUNT:
        r->value = EEUnitClassCount();
        break;
    case EE_TECH_CIV_OP_SET_CONTAINS:
        r->value = gUnitSetManager.GetSet((unsigned long)c->a)->Contains((unsigned long)c->b);
        break;
    case EE_TECH_CIV_OP_CLASS: {
        EEUnitClassUpgrade* u = gUnitClassTable[theType] != NULL ? thePlayer->mUnitClassUpgrades[theType] : NULL;
        if (u == NULL)
            break;
        EEUnitClass* k = u->mUnitClass;   // the upgrade's class: the base values and the category
        EeTechCivStats& s = r->stats;
        s.up_attack = u->mAttack;
        s.up_reload_time = u->mReloadTime;
        s.up_range = u->mRange;
        s.up_line_of_sight = u->mLineOfSight;
        s.up_max_velocity = u->mMaxVelocity;
        s.up_flight_time = u->mFlightTime;
        s.up_area_effect = u->mAreaEffect;
        s.up_build_time_scale = u->mBuildTimeScale;
        s.up_repair_rate = u->mRepairRate;
        s.up_area_effect_scale = u->mAreaEffectScale;
        s.up_cost_scale = u->mCostScale;
        s.class_attack = k->mAttack;
        s.class_reload_time = k->mReloadTime;
        s.class_range = k->mRange;
        s.class_line_of_sight = k->mLineOfSight;
        s.class_max_velocity = k->GetMaxVelocity();
        s.class_flight_time = k->mFlightTime;
        s.class_area_effect = k->mAreaEffect;
        s.class_repair_rate = k->mRepairRate;
        s.class_area_effect_scale = k->mAreaEffectScale;
        s.class_category = (long)k->mCategory;
        for (int i = 0; i < 6; i++) {
            s.up_armor[i] = u->mArmor[i];
            s.class_armor[i] = k->mArmor[i];
        }
        for (int i = 0; i < 7; i++) {
            s.up_gather_times[i] = u->mGatherTimes[i];
            s.class_gather_times[i] = k->mGatherTimes[i];
        }
        r->value = 1;
        break;
    }
    case EE_TECH_CIV_OP_PUT: {
        EEUnitClassUpgrade* u = thePlayer->mUnitClassUpgrades[theType];
        const EeTechCivStats& s = c->stats;
        switch (c->b) {
        case EE_TECH_CIV_STAT_ATTACK: u->mAttack = s.up_attack; break;
        case EE_TECH_CIV_STAT_RELOAD_TIME: u->mReloadTime = (long)s.up_reload_time; break;
        case EE_TECH_CIV_STAT_ARMOR: for (int i = 0; i < 6; i++) u->mArmor[i] = s.up_armor[i]; break;
        case EE_TECH_CIV_STAT_RANGE: u->SetRange(s.up_range); break;
        case EE_TECH_CIV_STAT_LINE_OF_SIGHT: u->mLineOfSight = s.up_line_of_sight; break;
        case EE_TECH_CIV_STAT_SPEED: u->SetMaxVelocity(s.up_max_velocity); break;
        case EE_TECH_CIV_STAT_FLIGHT_TIME: u->mFlightTime = (long)s.up_flight_time; break;
        case EE_TECH_CIV_STAT_AREA_EFFECT: u->mAreaEffect = s.up_area_effect; break;
        case EE_TECH_CIV_STAT_GATHER_TIMES: for (int i = 0; i < 7; i++) u->mGatherTimes[i] = (long)s.up_gather_times[i]; break;
        case EE_TECH_CIV_STAT_BUILD_TIME: u->mBuildTimeScale = s.up_build_time_scale; break;
        case EE_TECH_CIV_STAT_REPAIR_RATE: u->mRepairRate = (long)s.up_repair_rate; break;
        case EE_TECH_CIV_STAT_AREA_EFFECT_SCALE: u->mAreaEffectScale = s.up_area_effect_scale; break;
        case EE_TECH_CIV_STAT_COST: u->mCostScale = s.up_cost_scale; break;
        }
        break;
    }
    case EE_TECH_CIV_OP_VARIANT_FACTOR:
        r->real = ::gWorld.GetGameVariant()->GetFactor(theType);
        break;
    case EE_TECH_CIV_OP_HIT_POINTS:
        EEUpgradeHitPoints(thePlayer->mUnitClassUpgrades[theType], theType, c->real, thePlayer);
        break;
    case EE_TECH_CIV_OP_SIGHT:
        for (EEUnit* theUnit = thePlayer->mFirstUnit; theUnit != NULL; theUnit = static_cast<EEUnit*>(theUnit->mNext))
            if (theUnit->mUnitClass->mUnitType == (unsigned long)theType) {
                if (c->b != 0)
                    theUnit->SightOnHere();
                else
                    theUnit->SightOffHere();
            }
        break;
    case EE_TECH_CIV_OP_GET_FIELD:
        if (c->a == EE_TECH_CIV_FIELD_CONVERSION)
            r->real = thePlayer->mConversionFactor;
        else if (c->a == EE_TECH_CIV_FIELD_HIGH_GROUND)
            r->real = thePlayer->mHighGroundBonus;
        else
            r->value = thePlayer->mShareBonus;
        break;
    case EE_TECH_CIV_OP_SET_FIELD:
        if (c->a == EE_TECH_CIV_FIELD_CONVERSION)
            thePlayer->mConversionFactor = c->real;
        else if (c->a == EE_TECH_CIV_FIELD_HIGH_GROUND)
            thePlayer->mHighGroundBonus = c->real;
        else
            thePlayer->mShareBonus = (long)c->b;
        break;
    case EE_TECH_CIV_OP_POPULATION_LIMIT:
        r->value = thePlayer->GetPopulationLimit(0);
        break;
    case EE_TECH_CIV_OP_AREA_EFFECTS:
        thePlayer->SuggestRecomputeAllAreaEffects();
        thePlayer->mSelectedGroup->ShowSelectionAreas();
        break;
    case EE_TECH_CIV_OP_RESEARCH:
        thePlayer->GetTechTree()->Research(theType, NULL);
        break;
    case EE_TECH_CIV_OP_NODE_RESEARCHED:
        theNode = thePlayer->GetTechTree()->Get(theType);
        r->value = theNode == NULL ? -1 : theNode->mIsResearched ? 1 : 0;
        break;
    case EE_TECH_CIV_OP_RESEARCH_NODE:
        theNode = thePlayer->GetTechTree()->Get(theType);
        if (theNode)
            theNode->ResearchNode(NULL);
        break;
    case EE_TECH_CIV_OP_LOAD_SOUND:
        if (EESoundObject* theSound = EESoundObject::Get(theType)) {
            theSound->LoadMedia();
            thePlayer->AddLoadedMedia(theSound);
        }
        break;
    case EE_TECH_CIV_OP_LOAD_EFFECT:
        if (EEGraphicEffect* theEffect = EEGraphicEffect::Get(theType)) {
            theEffect->LoadMedia();
            thePlayer->AddLoadedMedia(theEffect);
        }
        break;
    case EE_TECH_CIV_OP_PULSE_SIGHT:
        thePlayer->PulseSight(false, c->real, theType);
        break;
    case EE_TECH_CIV_OP_MINING_LIMIT:
        thePlayer->mMiningLimit = theType;
        break;
    case EE_TECH_CIV_OP_CAMOUFLAGE:
        thePlayer->EnableCamouflagePower();
        break;
    case EE_TECH_CIV_OP_ALLOW_TECH:
        theNode = thePlayer->GetTechTree()->Get(theType);
        if (theNode == NULL)
            break;
        theNode->mIsAllowed = c->b != 0;
        if (c->b == 0)
            theNode->SetObsolete(true, false);
        break;
    }
}

// Applies an upgrade for a player to every unit class of its category's unit set; false without a level value
bool EECivilizationManager::ApplyUpgrade(EECivUpgrade inUpgrade, long inPlayer)
{
    EECivCaptureApply* theCapture = EECivCaptureApplyBegin(inUpgrade, inPlayer);   // TEST ONLY: CVAP
    EeTechCivHost theHost{NULL, &EECivHostCall};
    bool theResult = ee_tech_civ_apply_upgrade(&theHost, inUpgrade, (int32_t)inPlayer);
    EECivCaptureApplyEnd(theCapture, theResult);
    return theResult;
}

// ------------------------------------------------------------------------------------------------ the powers

EECivPowersManager::EECivPowersManager()
{
}

EECivPowersManager::~EECivPowersManager()
{
}

// Reads the civilization power records (appended)
long EECivPowersManager::Load(FSFile* inFile)
{
    std::vector<unsigned char> theBytes;
    if (EECivReadAll(inFile, theBytes) != kErrorOK)
        return kErrorFailed;
    return ee_tech_civ_load_powers(theBytes.data(), theBytes.size());
}

// Reads a civilization power's name from the string table
long EECivPowersManager::GetName(long inID, UWideString& outName)
{
    return gResource->GetString(inID + IDS_CIV_POWER_BASE, outName);
}

bool EECivPowersManager::GetPower(long inID, EECivPower& outPower)
{
    return ee_tech_civ_power((int32_t)inID, reinterpret_cast<EeTechCivPower*>(&outPower));
}

EECivPower* EECivPowersManager::GetPowerRecord(long inID)
{
    return reinterpret_cast<EECivPower*>(ee_tech_civ_power_record((int32_t)inID));
}

unsigned long EECivPowersManager::GetNumPowers() { return ee_tech_civ_power_count(); }

// (an upgrade whose attribute applies the power: civbuilder.yaml; the original compared the level with the id)
bool EECivPowersManager::HasPower(EEPlayer* inPlayer, long inID)
{
    std::vector<EECivUpgrade>& theUpgrades = inPlayer->mCivilization.GetPurchasedUpgrades();
    return ee_tech_civ_has_power(theUpgrades.data(), theUpgrades.size(), (int32_t)inID);
}
