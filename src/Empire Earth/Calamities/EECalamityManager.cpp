// C:\EEX\Empire Earth\Calamities\EECalamityManager.cpp (__FILE__ 0x4b9850). The calamity classes and the active
// calamities: 0xadf97..0xaebb7. Unit flags: the exe's module set.
// The unit closes with its template COMDATs: vector<EECalamity*>::push_back (0xaea04), USingleton's ??_G (0xaea30),
// DeleteVectorContents<EECalamity> (0xaea54), GetCalamityClass<EEGlobalCalamityClass> (0xaea85),
// <EELocationCalamityClass> (0xaeaec), DeleteVectorContents<EECalamityClass> (0xaeb53). The three bases' units
// (EEGlobalCalamityClass.cpp, EELocationCalamityClass.cpp 0xb6f21, EEUnitCalamityClass.cpp 0xb70cc) are folded into
// EECalamityClass and the creations below (calamities lane, round 4).
#include <windows.h>
#include <stdio.h>

#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GETerrainMesh.h"
#include "lle/USingleton.h"

#include "ee/EEEffectManager.h"
#include "ee/EECalamity.h"
#include "ee/EECalamityClass.h"
#include "ee/EECalamityManager.h"
#include "rust/game/gamecpp.h"
#include "ee/EEMap.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
// Creates the calamity manager with no classes or active calamities
EECalamityManager::EECalamityManager()
{
}

// Stops and deletes every active calamity (asserts that all were deleted)
void EECalamityManager::DestroyCalamities()
{
    ee_gamecpp_calamity_destroy_all(this);   // (ee_game gamecpp calamity_objects.rs)
}


// Reads the calamity class records from the file and creates and initializes the class each one names (NULL for
// unknown kinds); returns the record-count read result
long EECalamityManager::LoadCalamityClasses(FSFileSpec* inFile)
{
    return ee_gamecpp_calamity_load_classes(this, inFile);
}

// Starts a global calamity of the class for the caster unit and player; false if the class or caster does not exist
bool EECalamityManager::CreateGlobalCalamity(unsigned long inClassEnum, unsigned long inCasterUnitID,
                                             unsigned long inPlayerNumber)
{
    return ee_gamecpp_calamity_create(this, 0, inClassEnum, inCasterUnitID, 0, 0, 0, inPlayerNumber);
}

// Starts a location calamity at the centre of the tile for the caster (or none, id -1) and player; false if the class,
// tile or caster does not exist
bool EECalamityManager::CreateLocationCalamity(unsigned long inClassEnum, unsigned long inCasterUnitID,
                                               long inX, long inY, unsigned long inPlayerNumber)
{
    return ee_gamecpp_calamity_create(this, 1, inClassEnum, inCasterUnitID, 0, inX, inY, inPlayerNumber);
}

// Starts a unit calamity on the target unit for the caster (or none, id -1) and player; false if the class, target or
// caster does not exist
bool EECalamityManager::CreateUnitCalamity(unsigned long inClassEnum, unsigned long inCasterUnitID,
                                           unsigned long inTargetUnitID, unsigned long inPlayerNumber)
{
    return ee_gamecpp_calamity_create(this, 2, inClassEnum, inCasterUnitID, inTargetUnitID, 0, 0, inPlayerNumber);
}

// Writes every active calamity to the out-of-sync dump
void EECalamityManager::DumpOutOfSynch(FILE* inFile)
{
    ee_gamecpp_calamity_dump_all(this, inFile);
}

// Returns the list of active calamities
void EECalamityManager::GetCalamities(std::vector<EECalamity*>*& outCalamities)
{
    outCalamities = &this->mCalamities;
}

// Returns the calamity class of the given kind, NULL if out of range
EECalamityClass* EECalamityManager::GetCalamityClass(const EECalamityEnum& inClassEnum) const
{
    return (EECalamityClass*)ee_gamecpp_calamity_class_of(const_cast<EECalamityManager*>(this), inClassEnum);
}

// Finds the calamity's index among the active calamities; false if it is not active
bool EECalamityManager::GetCalamityIndex(EECalamity* inCalamity, long& outIndex)
{
    return ee_gamecpp_calamity_index(this, inCalamity, false, &outIndex);
}

// Returns the calamity class of the given kind, NULL if out of range
EECalamityClass* EECalamityManager::GetCalamityClass(unsigned long inClassEnum) const
{
    return (EECalamityClass*)ee_gamecpp_calamity_class_of(const_cast<EECalamityManager*>(this), inClassEnum);
}

// Finds the class's index among the calamity classes; false if it is not there
bool EECalamityManager::GetCalamityClassIndex(EECalamityClass* inClass, long& outIndex)
{
    return ee_gamecpp_calamity_index(this, inClass, true, &outIndex);
}

// Returns the class's target kind (global, location or unit); false if the class does not exist
bool EECalamityManager::GetCalamityType(EECalamityEnum inClassEnum, long& outType)
{
    return ee_gamecpp_calamity_type(this, inClassEnum, &outType);
}

// Returns the sum of the active calamities' sync checksums
long EECalamityManager::GetOutOfSynchChecksum()
{
    return ee_gamecpp_calamity_checksum_all(this);
}

// Tells whether a calamity of the given kind is active
bool EECalamityManager::IsCalamityActive(long inClassEnum)
{
    return ee_gamecpp_calamity_is_active(this, inClassEnum);
}

// Loads the media of the calamity class of the given kind for the player
void EECalamityManager::LoadMedia(EEPlayer* inPlayer, const EECalamityEnum& inClassEnum)
{
    ee_gamecpp_calamity_class_media(this, inPlayer, inClassEnum, true);
}

// Replaces the active calamities with the loaded ones; saves before 0.30 drop kinds 7, 8 and 10 (not deleted)
void EECalamityManager::Restore(unsigned long inMajorVersion, unsigned long inMinorVersion,
                                std::vector<EECalamity*>& inCalamities)
{
    ee_gamecpp_calamity_restore(this, inMajorVersion, inMinorVersion, &inCalamities);
}

// Starts the calamity and keeps it active if it started; one that fails to start is deleted
void EECalamityManager::AddCalamity(EECalamity* inCalamity)
{
    ee_gamecpp_calamity_add(this, inCalamity);
}

// Destroys the active calamities and deletes every calamity class (asserts that all were deleted)
void EECalamityManager::DestroyCalamityClasses(bool inShutdown)
{
    ee_gamecpp_calamity_destroy_classes(this);
}

// Unloads the media of the calamity class of the given kind for the player
void EECalamityManager::UnloadMedia(EEPlayer* inPlayer, const EECalamityEnum& inClassEnum)
{
    ee_gamecpp_calamity_class_media(this, inPlayer, inClassEnum, false);
}

// Clears every player's earthquake-shaken flag, updates each active calamity and destroys those that report 7 (done)
void EECalamityManager::Update(float inElapsedTime)
{
    ee_gamecpp_calamity_update_all(this, inElapsedTime);
}

// Creates a unit of the class at the transformation for the player (-1: the calamity's player); NULL if the class
// does not exist
// cdecl; the hurricane and volcano calamities call it (provisional name). It sits after the unit's template COMDATs
// in the original.
EEUnit* EECreateCalamityUnit(EECalamity* inCalamity, long inUnitClass, GETransformation& inTransformation,
                             long inPlayerNumber)
{
    return (EEUnit*)ee_gamecpp_calamity_create_unit(inCalamity, inUnitClass, &inTransformation, inPlayerNumber);
}
