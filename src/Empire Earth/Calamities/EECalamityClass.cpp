// C:\EEX\Empire Earth\Calamities\EECalamityClass.cpp (file name inferred: no __FILE__; the class's members in vtable
// order). The base of every calamity class: 0xadcd9..0xadf97. Unit flags: the exe's module set.
// Data (.rdata, in order): -10000.0f 0x43c6bc (not referenced by the unit's code), the vtable 0x43c6c4 (COL 0x43c6c0).
#include <windows.h>

#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GETerrainMesh.h"
#include "lle/SESoundManager.h"

#include "ee/EEEffectManager.h"
#include "ee/EECalamityUpdate.h"
#include "ee/EECalamity.h"
#include "ee/EECalamityClass.h"
#include "rust/game/gamecpp.h"
#include "ee/EEContagionManager.h"
#include "ee/EEGameSound.h"
#include "ee/EEMedia.h"
#include "ee/EEMap.h"
#include "ee/EEPlayer.h"
#include "ee/EESoundObject.h"
#include "ee/EEAreaUtilities.h"
#include "ee/EEUnit.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EETechCapture.h"
#include "ee/EEGameVariant.h"
#include "lle/USingleton.h"
#include "ee/EECalamityManager.h"
#include "ee/EEGraphicEffect.h"
#include "ee/EEContagionFunctor.h"
#include "ee/EEMouse.h"
#include "ee/EEPhysics.h"
#include "ee/EEUnitPicker.h"
#include "rust/sim/units.h"

static_assert(sizeof(EECalamityData) == sizeof(EeCalamityData), "the db record as ee_tech reads it");

// Creates a calamity class with no numbers, effects, sounds or functor
EECalamityClass::EECalamityClass()
    : EeCalamityClassFields(), mGraphicEffect1(NULL), mGraphicEffect2(NULL), mGraphicEffect3(NULL),
      mGraphicEffect4(NULL), mSound(NULL), mSecondarySound(NULL), mFunctor(NULL)
{
}

// Returns the power cost of casting the calamity
long EECalamityClass::GetCost()
{
    return this->mCost;
}

// Returns the id of the unit under the mouse for a unit class, -1 if none (and for the other classes)
long EECalamityClass::GetDefaultTarget(GE3DPoint&, unsigned long, unsigned long, bool)
{
    return ee_gamecpp_calamity_class_default_target(this);   // (ee_game gamecpp calamity_objects.rs)
}

// Returns the unit set the calamity affects
unsigned long EECalamityClass::GetUnitSet()
{
    return this->mUnitSet;
}

// Destroys the calamity class: its class's releases in its order (effects dereferenced, the functor deleted, the
// volcano's sample unloaded; ee_tech calamity_class.rs media_step)
EECalamityClass::~EECalamityClass()
{
    ee_gamecpp_calamity_class_media_list(this, EE_CALAMITY_MEDIA_DESTROY, NULL, false);
}

// Reads the calamity data: the common fields (class, lifetime, unit set, cost, range, immunity and
// cast-by-all-selected flags, name and id), then the class's own: its effects and sounds fetched as it fetched them,
// its numbers, its contagion functor (ee_tech calamity_class.rs)
void EECalamityClass::Initialize(EECalamityData* inData)
{
    ee_gamecpp_calamity_class_initialize(this, reinterpret_cast<const EeCalamityData*>(inData));
}

// New (port): the class's effects (whether set, duration, size, delay) and sounds for the Rust tables
void EECalamityClass::GetMedia(EeCalamityClassMedia& outMedia)
{
    ee_gamecpp_calamity_class_get_media(this, &outMedia);
}

// The class's Start: its parameters and sounds (ee_tech calamity_class.rs run), the decisions in Rust (ee_tech
// calamity_start.rs; ee/EECalamityUpdate.h), the world steps the class's (EECalamityUpdate.cpp)
bool EECalamityClass::Start(EECalamity* inCalamity)
{
    return ee_gamecpp_calamity_class_run(this, inCalamity, EE_CALAMITY_RUN_START, 0.0f) != 0;
}

// The class's Stop (none for the classes whose Stop was empty), as Start
void EECalamityClass::Stop(EECalamity* inCalamity)
{
    ee_gamecpp_calamity_class_run(this, inCalamity, EE_CALAMITY_RUN_STOP, 0.0f);
}

// The class's Update: its parameters (ee_tech calamity_class.rs run; the game variant's earthquake damage factor),
// the update in Rust (ee_tech calamity_update.rs; ee/EECalamityUpdate.h), the class's world steps; 7 once
// the calamity is over
long EECalamityClass::Update(EECalamity* inCalamity, float inElapsedTime)
{
    return ee_gamecpp_calamity_class_run(this, inCalamity, EE_CALAMITY_RUN_UPDATE, inElapsedTime);
}

// The class's Reset of a new calamity: its placeholders (ee_tech calamity_class.rs reset), the time warp's target
// tile, the fire's and the plague's contagion helper on the target
void EECalamityClass::Reset(EECalamity* inCalamity)
{
    ee_gamecpp_calamity_class_reset(this, inCalamity);
}

// Updates the calamity's main or secondary sound for the target location with the given falloff; a volume other than
// -1 replaces the computed one (clamped to 0..1)
void EECalamityClass::UpdateSound(EECalamity* inCalamity, EESoundObject* inSound, float inVolume, float inFalloff,
                                  bool inPrimary)
{
    ee_gamecpp_calamity_class_update_sound(inCalamity, inSound, inVolume, inFalloff, inPrimary);
}

// Returns 0 if the caster (or nature) is alive with enough power and sees the tile, 2 if immunity blocks it there,
// else 1; always 1 without the check flag
long EECalamityClass::CanCastAt(EEComplexUnit* inUnit, GE3DPoint& inLocation, unsigned long inX, unsigned long inY,
                                bool inCheck, EEUnit* inTargetUnit)
{
    return ee_gamecpp_calamity_class_can_cast(this, inUnit, (const float(*)[3])&inLocation.mX, inX, inY, inCheck, inTargetUnit);
}

// A location class's IsValidLocation: the shared rule over the class's radius, water flag, map area, space terrain
// and placed unit class (ee_tech calamity_cast.rs, calamity_class.rs cast_fields)
bool EECalamityClass::IsValidLocation(EEComplexUnit* inUnit, unsigned long inX, unsigned long inY)
{
    return ee_gamecpp_calamity_class_valid_location(this, inUnit, inX, inY);
}

// A unit class's IsValidTarget: the fire's and the plague's functor decides (CanInfect), the other classes' the shared
// rule (ee_tech calamity_cast.rs; ee/EECalamityUpdate.h)
bool EECalamityClass::IsValidTarget(EEComplexUnit* inUnit, EEUnit* inTargetUnit)
{
    return ee_gamecpp_calamity_class_valid_target(this, inUnit, inTargetUnit);
}

// Picks a random point on the map between the minimum and maximum radius around the centre, at the tile's height
void EECalamityClass::GetRandomLocation(const GE3DPoint& inCenter, GE3DPoint& outLocation)
{
    ee_gamecpp_calamity_class_random_location(this, (const float(*)[3])&inCenter.mX, (float(*)[3])&outLocation.mX);
}

// (the base's: every class's own, by tables keyed by its enum)
// Returns the class's contagion functor (none but for the fire, plague, leprosy and area effects)
class EEContagionFunctor * EECalamityClass::GetContagionFunctor()
{
    return this->mFunctor;
}


// (the base's: every class's own, by tables keyed by its enum)
// Loads the class's effects and sounds (and the unit class it places) in its order
void EECalamityClass::LoadMedia(EEPlayer* inPlayer, bool inIsOnDemand)
{
    ee_gamecpp_calamity_class_media_list(this, EE_CALAMITY_MEDIA_LOAD, inPlayer, inIsOnDemand);
}

// The volcano makes units step aside from its site at the tile; nothing for the other classes
void EECalamityClass::Place(EEUnit* inUnit, unsigned long inX, unsigned long inY)
{
    ee_gamecpp_calamity_class_place(this, inUnit, inX, inY);
}

// (the base's: every class's own, by tables keyed by its enum)
// Starts the calamity's sound: the earthquake's, hurricane's and volcano's as its looping main sound, the meteor
// storm's once at the target (ee_tech calamity_class.rs sound_mode); nothing for the others
void EECalamityClass::PlaySound(EECalamity* inCalamity)
{
    ee_gamecpp_calamity_class_play_sound(this, inCalamity);
}

// Unloads the class's effects and sounds (and the unit class it places) in its order
void EECalamityClass::UnloadMedia(EEPlayer* inPlayer)
{
    ee_gamecpp_calamity_class_media_list(this, EE_CALAMITY_MEDIA_UNLOAD, inPlayer, false);
}
