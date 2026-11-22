// C:\EEX\Empire Earth\Civilization Builder\EECivilizationManager.cpp (file name from the class; no __FILE__). The
// civilization categories, premade civilizations and upgrade ids (RTTI EECivilizationManager, vtable 0x44bfd4). Range
// 0x2813f4..0x2830d0; EECivPowersManager.cpp's 0x2830d0..0x283b2e folded in (civilization builder lane, round 2).
// Whole in Rust (ee_tech civ.rs: the tables and queries; civ_apply.rs: ApplyUpgrade, ApplyUpgradeLevel with
// EECBProcessUpgrade and EECBUpgradeArmor, the powers' ApplyPower): the two classes keep their names and methods as
// forwarders. UI lane round 12: their remaining bodies (the db files' reads, the premade copies, ApplyUpgrade with
// its world host, the out-parameter members) are ee_game screens civ_manager_obj.rs over the shims of this unit; the
// world calls are civ_manager_calls.rs'. Unit flags: the exe module set.
#include <windows.h>
#include <mbstring.h>
#include <algorithm>
#include "lle/LowLevelEngine.h"
#include "lle/GERhombus.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "lle/UResourceFile.h"
#include "ee/EmpireEarth.h"
#include "ee/EEDatabaseTables.h"
#include "ee/EECivPowersManager.h"
#include "ee/EEUnitSetManager.h"
#include "lle/GEText.h"
#include "lle/UIListBox.h"
#include "lle/UILBString.h"
#include "ee/EECBUIListBox.h"
#include "ee/EECivilizationManager.h"
#include "ee/EEGameVariant.h"
#include "ee/EEStringIDs.h"
#include "ee/EERulesWorld.h"
#include "ee/EECivCapture.h"
#include "ee/EEMedia.h"
#include "ee/EESoundObject.h"
#include "ee/EEGraphicEffect.h"
#include "rust/sim/tech.h"
#include "rust/game/screens.h"   // the host and the premade list (civ_manager_calls.rs)
#include "ee/EEScreensShims.h"

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

// Creates the civilization manager (the tables are Rust's: it binds the game's rules for them)
EECivilizationManager::EECivilizationManager()
{
    ee_tech_civ_bind_rules(EEGetRules().RustRules());
}

EECivilizationManager::~EECivilizationManager()
{
}

// Reads the categories, then appends the civ powers' category (civbuilder.yaml's powers_category)
long EECivilizationManager::Load(FSFile* inFile) { return ee_screens_civm_load((uintptr_t)inFile, 0); }

// Reads the premade civilizations, sorted by index, and their eras
long EECivilizationManager::LoadPremadeCivilizations(FSFile* inFile) { return ee_screens_civm_load((uintptr_t)inFile, 1); }

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
    ee_screens_civm_civilizations((uintptr_t)&outCivs, 0, true);
}

void EECivilizationManager::GetCivilizationsOfType(std::vector<EECivilizationEntry>& outCivs, long inType)
{
    ee_screens_civm_civilizations((uintptr_t)&outCivs, (int32_t)inType, false);
}

// Fills a list box with one header per era and a line per premade civilization of that era (civ_manager_calls.rs)
void EECivilizationManager::FillCivilizationList(EECBUIListBox* inList)
{
    ee_screens_civ_fill_list(inList);
}

bool EECivilizationManager::MatchesPremadeCivilization(const UWideString& inFile, const UWideString& inName,
                                                       std::vector<EECivUpgrade>& inUpgrades, long* outNameID)
{
    return ee_screens_civm_matches((const char*)inFile, (const char*)inName, (uint32_t)inName.GetLength(), inUpgrades.data(),
                                   inUpgrades.size(), outNameID);
}

bool EECivilizationManager::GetCategoryNameID(unsigned long inCategory, long* outNameID)
{
    return ee_screens_civm_category_name_id((uint32_t)inCategory, outNameID);
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
    return ee_screens_civm_category((uint32_t)inCategory, (void**)outEntry);
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
    return ee_screens_civm_to_legacy(inUpgrade, &outLegacy);
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
    ee_screens_civm_split(inUpgrade, outCategory, outIndex);
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

#define CIVH_P(p) (::gWorld.mPlayers[p])
#define CIVH_U(u) ((EEUnitClassUpgrade*)(u))
#define CIVH_K(k) ((EEUnitClass*)(k))
extern "C" bool ee_screenscpp_civh_set_exists(int64_t inSet)
{ return gUnitSetManager.GetSet((unsigned long)inSet) != NULL; }
extern "C" int64_t ee_screenscpp_civh_class_count() { return EEUnitClassCount(); }
extern "C" bool ee_screenscpp_civh_set_contains(int64_t inSet, int64_t inKind)
{ return gUnitSetManager.GetSet((unsigned long)inSet)->Contains((unsigned long)inKind); }
extern "C" bool ee_screenscpp_civh_class_listed(int64_t inKind) { return gUnitClassTable[(long)inKind] != NULL; }
extern "C" uintptr_t ee_screenscpp_civh_upgrade(int32_t inPlayer, int64_t inKind)
{ return (uintptr_t)CIVH_P(inPlayer)->mUnitClassUpgrades[(long)inKind]; }
extern "C" uintptr_t ee_screenscpp_civh_upgrade_class(uintptr_t inUp) { return (uintptr_t)CIVH_U(inUp)->mUnitClass; }
extern "C" float ee_screenscpp_civh_up_attack(uintptr_t inUp) { return CIVH_U(inUp)->mAttack; }
extern "C" int64_t ee_screenscpp_civh_up_reload_time(uintptr_t inUp) { return CIVH_U(inUp)->mReloadTime; }
extern "C" float ee_screenscpp_civh_up_armor(uintptr_t inUp, uint32_t inI) { return CIVH_U(inUp)->mArmor[inI]; }
extern "C" float ee_screenscpp_civh_up_range(uintptr_t inUp) { return CIVH_U(inUp)->mRange; }
extern "C" float ee_screenscpp_civh_up_line_of_sight(uintptr_t inUp) { return CIVH_U(inUp)->mLineOfSight; }
extern "C" float ee_screenscpp_civh_up_max_velocity(uintptr_t inUp) { return CIVH_U(inUp)->mMaxVelocity; }
extern "C" int64_t ee_screenscpp_civh_up_flight_time(uintptr_t inUp) { return CIVH_U(inUp)->mFlightTime; }
extern "C" float ee_screenscpp_civh_up_area_effect(uintptr_t inUp) { return CIVH_U(inUp)->mAreaEffect; }
extern "C" int64_t ee_screenscpp_civh_up_gather_time(uintptr_t inUp, uint32_t inI)
{ return CIVH_U(inUp)->mGatherTimes[inI]; }
extern "C" float ee_screenscpp_civh_up_build_time_scale(uintptr_t inUp) { return CIVH_U(inUp)->mBuildTimeScale; }
extern "C" int64_t ee_screenscpp_civh_up_repair_rate(uintptr_t inUp) { return CIVH_U(inUp)->mRepairRate; }
extern "C" float ee_screenscpp_civh_up_area_effect_scale(uintptr_t inUp) { return CIVH_U(inUp)->mAreaEffectScale; }
extern "C" float ee_screenscpp_civh_up_cost_scale(uintptr_t inUp) { return CIVH_U(inUp)->mCostScale; }
extern "C" int64_t ee_screenscpp_civh_class_attack(uintptr_t inK) { return CIVH_K(inK)->mAttack; }
extern "C" int64_t ee_screenscpp_civh_class_reload_time(uintptr_t inK) { return CIVH_K(inK)->mReloadTime; }
extern "C" int64_t ee_screenscpp_civh_class_armor(uintptr_t inK, uint32_t inI) { return CIVH_K(inK)->mArmor[inI]; }
extern "C" float ee_screenscpp_civh_class_range(uintptr_t inK) { return CIVH_K(inK)->mRange; }
extern "C" float ee_screenscpp_civh_class_line_of_sight(uintptr_t inK) { return CIVH_K(inK)->mLineOfSight; }
extern "C" float ee_screenscpp_civh_class_max_velocity(uintptr_t inK) { return CIVH_K(inK)->GetMaxVelocity(); }
extern "C" int64_t ee_screenscpp_civh_class_flight_time(uintptr_t inK) { return CIVH_K(inK)->mFlightTime; }
extern "C" float ee_screenscpp_civh_class_area_effect(uintptr_t inK) { return CIVH_K(inK)->mAreaEffect; }
extern "C" int64_t ee_screenscpp_civh_class_gather_time(uintptr_t inK, uint32_t inI)
{ return CIVH_K(inK)->mGatherTimes[inI]; }
extern "C" int64_t ee_screenscpp_civh_class_repair_rate(uintptr_t inK) { return CIVH_K(inK)->mRepairRate; }
extern "C" float ee_screenscpp_civh_class_area_effect_scale(uintptr_t inK) { return CIVH_K(inK)->mAreaEffectScale; }
extern "C" int64_t ee_screenscpp_civh_class_category(uintptr_t inK) { return (long)CIVH_K(inK)->mCategory; }
extern "C" void ee_screenscpp_civh_put_attack(uintptr_t inUp, float inV) { CIVH_U(inUp)->mAttack = inV; }
extern "C" void ee_screenscpp_civh_put_reload_time(uintptr_t inUp, int64_t inV)
{ CIVH_U(inUp)->mReloadTime = (long)inV; }
extern "C" void ee_screenscpp_civh_put_armor(uintptr_t inUp, uint32_t inI, float inV)
{ CIVH_U(inUp)->mArmor[inI] = inV; }
extern "C" void ee_screenscpp_civh_put_range(uintptr_t inUp, float inV) { CIVH_U(inUp)->SetRange(inV); }
extern "C" void ee_screenscpp_civh_put_line_of_sight(uintptr_t inUp, float inV) { CIVH_U(inUp)->mLineOfSight = inV; }
extern "C" void ee_screenscpp_civh_put_max_velocity(uintptr_t inUp, float inV) { CIVH_U(inUp)->SetMaxVelocity(inV); }
extern "C" void ee_screenscpp_civh_put_flight_time(uintptr_t inUp, int64_t inV)
{ CIVH_U(inUp)->mFlightTime = (long)inV; }
extern "C" void ee_screenscpp_civh_put_area_effect(uintptr_t inUp, float inV) { CIVH_U(inUp)->mAreaEffect = inV; }
extern "C" void ee_screenscpp_civh_put_gather_time(uintptr_t inUp, uint32_t inI, int64_t inV)
{ CIVH_U(inUp)->mGatherTimes[inI] = (long)inV; }
extern "C" void ee_screenscpp_civh_put_build_time_scale(uintptr_t inUp, float inV)
{ CIVH_U(inUp)->mBuildTimeScale = inV; }
extern "C" void ee_screenscpp_civh_put_repair_rate(uintptr_t inUp, int64_t inV)
{ CIVH_U(inUp)->mRepairRate = (long)inV; }
extern "C" void ee_screenscpp_civh_put_area_effect_scale(uintptr_t inUp, float inV)
{ CIVH_U(inUp)->mAreaEffectScale = inV; }
extern "C" void ee_screenscpp_civh_put_cost_scale(uintptr_t inUp, float inV) { CIVH_U(inUp)->mCostScale = inV; }
extern "C" float ee_screenscpp_civh_variant_factor(int64_t inKind)
{ return ::gWorld.GetGameVariant()->GetFactor((long)inKind); }
extern "C" void ee_screenscpp_civh_hit_points(int32_t inPlayer, int64_t inKind, float inFactor)
{ EEUpgradeHitPoints(CIVH_P(inPlayer)->mUnitClassUpgrades[(long)inKind], (long)inKind, inFactor, CIVH_P(inPlayer)); }
extern "C" uintptr_t ee_screenscpp_civh_first_unit(int32_t inPlayer) { return (uintptr_t)CIVH_P(inPlayer)->mFirstUnit; }
extern "C" uintptr_t ee_screenscpp_civh_next_unit(uintptr_t inUnit)
{ return (uintptr_t)static_cast<EEUnit*>(((EEUnit*)inUnit)->mNext); }
extern "C" uint64_t ee_screenscpp_civh_unit_type(uintptr_t inUnit)
{ return (uint64_t)((EEUnit*)inUnit)->mUnitClass->mUnitType; }
extern "C" void ee_screenscpp_civh_sight_on(uintptr_t inUnit) { ((EEUnit*)inUnit)->SightOnHere(); }
extern "C" void ee_screenscpp_civh_sight_off(uintptr_t inUnit) { ((EEUnit*)inUnit)->SightOffHere(); }
extern "C" float ee_screenscpp_civh_conversion(int32_t inPlayer) { return CIVH_P(inPlayer)->mConversionFactor; }
extern "C" float ee_screenscpp_civh_high_ground(int32_t inPlayer) { return CIVH_P(inPlayer)->mHighGroundBonus; }
extern "C" int64_t ee_screenscpp_civh_share_bonus(int32_t inPlayer) { return CIVH_P(inPlayer)->mShareBonus; }
extern "C" void ee_screenscpp_civh_set_conversion(int32_t inPlayer, float inV)
{ CIVH_P(inPlayer)->mConversionFactor = inV; }
extern "C" void ee_screenscpp_civh_set_high_ground(int32_t inPlayer, float inV)
{ CIVH_P(inPlayer)->mHighGroundBonus = inV; }
extern "C" void ee_screenscpp_civh_set_share_bonus(int32_t inPlayer, int64_t inV)
{ CIVH_P(inPlayer)->mShareBonus = (long)inV; }
extern "C" int64_t ee_screenscpp_civh_population_limit(int32_t inPlayer)
{ return CIVH_P(inPlayer)->GetPopulationLimit(0); }
extern "C" void ee_screenscpp_civh_recompute_area_effects(int32_t inPlayer)
{ CIVH_P(inPlayer)->SuggestRecomputeAllAreaEffects(); }
extern "C" void ee_screenscpp_civh_show_selection_areas(int32_t inPlayer)
{ CIVH_P(inPlayer)->mSelectedGroup->ShowSelectionAreas(); }
extern "C" void ee_screenscpp_civh_research(int32_t inPlayer, int64_t inKind)
{ CIVH_P(inPlayer)->GetTechTree()->Research((long)inKind, NULL); }
extern "C" uintptr_t ee_screenscpp_civh_node(int32_t inPlayer, int64_t inKind)
{ return (uintptr_t)CIVH_P(inPlayer)->GetTechTree()->Get((long)inKind); }
extern "C" bool ee_screenscpp_civh_node_researched(uintptr_t inNode)
{ return ((EETechTreeNode*)inNode)->mIsResearched; }
extern "C" void ee_screenscpp_civh_node_research(uintptr_t inNode) { ((EETechTreeNode*)inNode)->ResearchNode(NULL); }
extern "C" void ee_screenscpp_civh_node_set_allowed(uintptr_t inNode, bool inV)
{ ((EETechTreeNode*)inNode)->mIsAllowed = inV; }
extern "C" void ee_screenscpp_civh_node_set_obsolete(uintptr_t inNode)
{ ((EETechTreeNode*)inNode)->SetObsolete(true, false); }
extern "C" uintptr_t ee_screenscpp_civh_sound(int64_t inKind) { return (uintptr_t)EESoundObject::Get((long)inKind); }
extern "C" void ee_screenscpp_civh_sound_load(uintptr_t inSound) { ((EESoundObject*)inSound)->LoadMedia(); }
extern "C" void ee_screenscpp_civh_add_sound(int32_t inPlayer, uintptr_t inSound)
{ CIVH_P(inPlayer)->AddLoadedMedia((EESoundObject*)inSound); }
extern "C" uintptr_t ee_screenscpp_civh_effect(int64_t inKind) { return (uintptr_t)EEGraphicEffect::Get((long)inKind); }
extern "C" void ee_screenscpp_civh_effect_load(uintptr_t inEffect) { ((EEGraphicEffect*)inEffect)->LoadMedia(); }
extern "C" void ee_screenscpp_civh_add_effect(int32_t inPlayer, uintptr_t inEffect)
{ CIVH_P(inPlayer)->AddLoadedMedia((EEGraphicEffect*)inEffect); }
extern "C" void ee_screenscpp_civh_pulse_sight(int32_t inPlayer, float inRadius, int64_t inKind)
{ CIVH_P(inPlayer)->PulseSight(false, inRadius, (long)inKind); }
extern "C" void ee_screenscpp_civh_mining_limit(int32_t inPlayer, int64_t inKind)
{ CIVH_P(inPlayer)->mMiningLimit = (long)inKind; }
extern "C" void ee_screenscpp_civh_camouflage(int32_t inPlayer) { CIVH_P(inPlayer)->EnableCamouflagePower(); }
extern "C" bool ee_screenscpp_cbl_tabable(uintptr_t inList) { return ((EECBUIListBox*)inList)->GetTabable(); }

// Applies an upgrade for a player to every unit class of its category's unit set; false without a level value
bool EECivilizationManager::ApplyUpgrade(EECivUpgrade inUpgrade, long inPlayer)
{
    return ee_screens_civm_apply_upgrade(inUpgrade, inPlayer);
}

// ------------------------------------------------------------------------------------------------ the powers

EECivPowersManager::EECivPowersManager()
{
}

EECivPowersManager::~EECivPowersManager()
{
}

// Reads the civilization power records (appended)
long EECivPowersManager::Load(FSFile* inFile) { return ee_screens_civm_load((uintptr_t)inFile, 2); }

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
bool EECivPowersManager::HasPower(EEPlayer* inPlayer, long inID) { return ee_screens_civm_has_power((uintptr_t)inPlayer, inID); }

// the db files' reads, the premade copies, ApplyUpgrade's capture halves and the powers' test: civ_manager_obj.rs over
// these (UI lane round 12)
static unsigned char sCivmChunk[0x1000];   // a chunk read (the original's local)
static unsigned long sCivmRead = 0;
static unsigned char sCivmByte = 0;
extern "C" uintptr_t ee_screenscpp_civm_capture_begin(uint32_t inUpgrade, int64_t inPlayer)
{ return (uintptr_t)EECivCaptureApplyBegin(inUpgrade, (long)inPlayer); }
extern "C" void ee_screenscpp_civm_capture_end(uintptr_t inState, bool inResult)
{ EECivCaptureApplyEnd((EECivCaptureApply*)inState, inResult); }
extern "C" int32_t ee_screenscpp_civm_file_open(uintptr_t inFile) { return ((FSFile*)inFile)->Open(kFileAccessRead); }
extern "C" int32_t ee_screenscpp_civm_read_chunk(uintptr_t inFile)
{ return ((FSFile*)inFile)->Read(sCivmChunk, sizeof(sCivmChunk), sCivmRead); }
extern "C" void ee_screenscpp_civm_chunk_bytes(void* inOut)
{ ee_screens_shim_out(inOut, sCivmChunk, sizeof(sCivmChunk)); }
extern "C" int32_t ee_screenscpp_civm_read_byte(uintptr_t inFile) { return ((FSFile*)inFile)->Read(sCivmByte); }
extern "C" uint8_t ee_screenscpp_civm_byte() { return sCivmByte; }
extern "C" void ee_screenscpp_civm_file_close(uintptr_t inFile) { ((FSFile*)inFile)->Close(); }
extern "C" int64_t ee_screenscpp_civm_powers_cost()
{ return (int64_t)EERuleLong(EEGetRules().mCivBuilder.mPowersCost); }
extern "C" void ee_screenscpp_civm_civs_clear(uintptr_t inList)
{ ((std::vector<EECivilizationEntry>*)inList)->clear(); }
extern "C" void ee_screenscpp_civm_civs_push(uintptr_t inList, uintptr_t inEntry)
{ ((std::vector<EECivilizationEntry>*)inList)->push_back(*(const EECivilizationEntry*)inEntry); }
extern "C" uintptr_t ee_screenscpp_civm_purchased_count(uintptr_t inPlayer)
{ return ((EEPlayer*)inPlayer)->mCivilization.GetPurchasedUpgrades().size(); }
extern "C" uintptr_t ee_screenscpp_civm_purchased_data(uintptr_t inPlayer)
{ return (uintptr_t)((EEPlayer*)inPlayer)->mCivilization.GetPurchasedUpgrades().data(); }
