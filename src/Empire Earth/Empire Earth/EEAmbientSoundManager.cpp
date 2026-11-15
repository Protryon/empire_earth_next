// C:\EEX\Empire Earth\Empire Earth\EEAmbientSoundManager.cpp (name from RTTI EEAmbientSoundManager; no __FILE__).
// 0x124ee2..0x1273d7: the ambient sounds (ctor, ??_G, dtor, the day-phase handlers, Load, the per-frame update and
// its animal/water/terrain parts), then the COMDATs: vector<EEAmbientSound>, EEAmbientSound's implicit copy members
// and inline members, the world analyzer's ??_G, the manager's inline Random/CheckState. The sound slots are indices
// into mSounds (7 wind, 9/10/14/18 animals, 11/15/19 terrain, 20..27 water, forest and space).
// Also EEAmbientSound.cpp's 0x124a88..0x124ee2 (the ambient sound and the world analyzer's counters; the unit is
// folded in here).
//
// The ambient sounds are whole in Rust (rust/crates/ee_app ambient.rs, app and world lane round 1): the manager's, the
// sounds' and the analyzer's plain members are records the classes derive from, every decision is Rust over the host
// below (the sound objects and instances, the random generator, the world's reads). This unit keeps the host, Load
// (the file), the analyzer's counting of the rendered units and the forwarders.
#include <windows.h>
#include <vector>

#include "lle/LowLevelEngine.h"
#include "lle/TMPoll.h"
#include "lle/SESMInstanceNode.h"
#include "lle/SESoundManager.h"
#include "lle/FSFile.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GE3DPoint.h"
#include "lle/GETransformation.h"
#include "lle/GETerrainMesh.h"
#include "lle/GE3DLine.h"
#include "lle/GEPhysicalModel.h"
#include "lle/U2DSparseArray.h"
#include "lle/U2DPolygon.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEMap.h"
#include "ee/EEMedia.h"
#include "ee/EESoundObject.h"
#include "ee/EEGameSound.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEUnitGoal.h"
#include "ee/EEAIArea.h"
#include "ee/EEAIForest.h"
#include "ee/EETown.h"
#include "ee/EEAIContinentManager.h"
#include "ee/EEAmbientSound.h"
#include "ee/EEAmbientSoundManager.h"

extern EEAIContinentManager gContinentManager;

// 0x524a88..0x524b34: the original's two EE-level trigonometry caller logs ("UMCos EE-Level", "UMSin EE-Level",
// ESCallerLog, never called), deleted with the class (LLE lane round 1)

// FUNCTION: EXE 0x524b34
// Creates a silent ambient sound with no sound object, full probability and normal pitch
EEAmbientSound::EEAmbientSound() : mSound(NULL), mInstance(NULL)
{
    ee_app_ambient_sound_default(this);
}

// FUNCTION: EXE 0x524b85
// (the scalar deleting destructor)

// FUNCTION: EXE 0x524ba1
// Destroys the ambient sound, unloading its sound object's media
EEAmbientSound::~EEAmbientSound()
{
    if (this->mSound != NULL)
        this->mSound->UnloadMedia();
}

// FUNCTION: EXE 0x524bb4
// Sets up the ambient sound from its database record (sound, fades, volumes, probability, interval in ms), silent
void EEAmbientSound::Init(const EEAmbientSoundRecord& inRecord)
{
    EeAppAmbientRecord theRecord = {inRecord.mSoundID, inRecord.mMaximumVolume, inRecord.mStartVolume, inRecord.mFadeUp,
                                    inRecord.mFadeDown, inRecord.mProbability, inRecord.mIntervalSeconds};
    this->mSound = NULL;
    ee_app_ambient_sound_init(this, &theRecord);
}

// FUNCTION: EXE 0x524c16
// FUNCTION: EXE 0x524c46
// FUNCTION: EXE 0x524c72
// FUNCTION: EXE 0x524ca9
// FUNCTION: EXE 0x524d4e
// (FadeIn, FadeOut, LoadSound, PlayAtVolume, UnloadSound: ee_app ambient.rs, over the host's sound operations)

// FUNCTION: EXE 0x524d63
// Counts a visible unit for the ambient sounds: citizens, farms, goats, eagles, attackers, and enemy bombers over
// the local player's towns (from the atomic epochs on)
void EEAmbientWorldAnalyzer::CountUnit(EEComplexUnit* inUnit)
{
    U2DPoint<long> theTile(0, 0);
    EEComplexUnit* theUnit = inUnit;
    EEUnitClass* theClass = theUnit->mUnitClass;
    if (theClass->mCategory == kUnitCategoryLandCitizen)
        this->mNumCitizens++;
    switch (theClass->mUnitType) {
    case kUnitTypeFarm:
        this->mNumFarms++;
        break;
    case kUnitTypeResGoat:
        this->mNumGoats++;
        break;
    case kUnitTypeResEagle:
        this->mNumEagles++;
        break;
    }
    if (theUnit->mGoalState != NULL && theUnit->mGoalState->mKind == kGoalAttackUnit && (theClass->mIsHuman || theClass->mIsHorse)
        && theClass->mEpoch >= kTechEpochStone && theClass->mEpoch <= kTechEpochIndustrial)
        this->mNumAttackers++;
    if (theClass->mFamily != kUnitFamilyBomber || theClass->mIsParatrooperBomber)
        return;
    EEPlayer* thePlayer = gWorld.mPlayers[gWorld.mCurrentPlayer];
    if (theUnit->mPlayer == thePlayer || thePlayer->mEnemies[theUnit->mPlayer->mPlayerID] == 0)
        return;
    theTile.mX = theUnit->mTileX;
    theTile.mY = theUnit->mTileY;
    if (!(thePlayer->mTechnologies->mCurrentEpoch >= kTechEpochAtomicWW1))
        return;
    for (long i = thePlayer->mTowns.size() - 1; i >= 0; i--) {
        if (thePlayer->mTowns[i]->HullContains(theTile)) {
            this->mNumAirRaiders++;
            break;
        }
    }
}

// FUNCTION: EXE 0x524e80
// Counts a visible tree when the unit is a wood resource
void EEAmbientWorldAnalyzer::CountTree(EEUnit* inUnit)
{
    if (inUnit->mUnitClass->mResourceType == kResourceWood)
        this->AddTree(inUnit);
}

// FUNCTION: EXE 0x524e99
// Counts a visible tree and, if no forest type is known yet, takes it from the AI forest at the unit
void EEAmbientWorldAnalyzer::AddTree(EEUnit* inUnit)
{
    this->mNumTrees++;
    if (this->mForestType != 0)
        return;
    EEAIForest* theForest = gContinentManager.GetForestAt(inUnit);
    if (theForest != NULL)
        this->mForestType = theForest->GetResourceKind();
}

// FUNCTION: EXE 0x524ec1
// Resets the world analyzer's counters for the next ambient update
void EEAmbientWorldAnalyzer::Clear()
{
    ee_app_ambient_clear(this);
}

// The ambient sounds' host (ee_app ambient_ffi.rs): the context is the manager
static void EEAmbientHost(void* inContext, int32_t inOp, EeAppAmbientIo* io)
{
    EEAmbientSoundManager* theManager = static_cast<EEAmbientSoundManager*>(inContext);
    EEAmbientSound* theSound = inOp >= EE_APP_AMBIENT_OP_SOUND && inOp <= EE_APP_AMBIENT_OP_UNLOAD_SOUND && inOp != EE_APP_AMBIENT_OP_RANDOM
                                   ? &theManager->mSounds[io->slot] : NULL;
    switch (inOp) {
    case EE_APP_AMBIENT_OP_SOUNDS: io->n = (uint32_t)theManager->mSounds.size(); break;
    case EE_APP_AMBIENT_OP_SOUND: io->sound = theSound; break;
    case EE_APP_AMBIENT_OP_RANDOM: io->volume = theManager->Random(); break;
    case EE_APP_AMBIENT_OP_PLAY:
        gSoundManager->Play(theSound->mSound->GetSampleID(), io->volume, io->pan, io->pitch, false, NULL);
        break;
    case EE_APP_AMBIENT_OP_HAS_INSTANCE: io->flag = theSound->mInstance != NULL; break;
    case EE_APP_AMBIENT_OP_START:
        theSound->mInstance = gSoundManager->PlayAndReturnReferencedNode(theSound->mSound->GetSampleID(), io->volume, io->pan,
                                                                          io->pitch, io->flag, NULL);
        break;
    case EE_APP_AMBIENT_OP_SET_PARAMETERS: gSoundManager->SetSoundParameters(theSound->mInstance, io->volume, io->pan, io->pitch); break;
    case EE_APP_AMBIENT_OP_STOP_INSTANCE:
        gSoundManager->Stop(theSound->mInstance, true);
        theSound->ReleaseInstance();
        break;
    case EE_APP_AMBIENT_OP_LOAD_SOUND:
        io->flag = theSound->mSound != NULL;
        if (theSound->mSound != NULL)
            break;
        theSound->mSound = EESoundObject::Get(theSound->mSoundIndex);
        if (theSound->mSound != NULL) {
            io->flag = true;
            theSound->mSound->LoadMedia();
            io->has = true;
            io->n = theSound->mSound->GetLooping() ? 1 : 0;
        }
        break;
    case EE_APP_AMBIENT_OP_UNLOAD_SOUND:
        if (theSound->mSound != NULL)
            theSound->mSound->UnloadMedia();
        theSound->mSound = NULL;
        break;
    case EE_APP_AMBIENT_OP_TIME_OF_DAY: io->volume = gWorld.GetTimeOfDay(); break;
    case EE_APP_AMBIENT_OP_TIME: io->n = (uint32_t)TMGetTime(); break;
    case EE_APP_AMBIENT_OP_VISIBLE_TILES: io->n = (uint32_t)gWorld.mMap->mTerrainMesh.GetNumVisibleTiles(); break;
    case EE_APP_AMBIENT_OP_VISIBLE_WATER: io->volume = gWorld.mMap->mTerrainMesh.GetVisibleWaterTilePercentage(); break;
    case EE_APP_AMBIENT_OP_VISIBLE_SPACE: io->volume = gWorld.mMap->mTerrainMesh.GetVisibleSpaceTilePercentage(); break;
    }
}
#define EE_AMBIENT_HOST this, &this->mAnalyzer, this, EEAmbientHost

// FUNCTION: EXE 0x524ee2
// Creates the ambient sound manager, disabled and unpaused, updating every 10 frames
EEAmbientSoundManager::EEAmbientSoundManager() : mRandom(1)
{
    ee_app_ambient_default(this);
}

// SYNTHETIC: EXE 0x524f41
// (the scalar deleting destructor)

// FUNCTION: EXE 0x524f5d
// Destroys the manager, stopping and discarding every ambient sound
EEAmbientSoundManager::~EEAmbientSoundManager()
{
    this->StopSounds();
    this->ClearSounds();
}

// FUNCTION: EXE 0x524f83
// FUNCTION: EXE 0x5250ae
// FUNCTION: EXE 0x525231
// FUNCTION: EXE 0x525267
// FUNCTION: EXE 0x525281
// FUNCTION: EXE 0x5252c2
// FUNCTION: EXE 0x5254c8
// FUNCTION: EXE 0x52580b
// FUNCTION: EXE 0x525d0d
// FUNCTION: EXE 0x525fcc
// (UpdateState, the day, dawn, night and dusk handlers, RandomRange, RandomInteger, UpdateAnimals, UpdateWater,
// UpdateTerrain: ee_app ambient.rs)

// FUNCTION: EXE 0x525012
// Loads the ambient sound records from the file into their slots and seeds the random generator; returns the last
// read status
long EEAmbientSoundManager::Load(FSFile& inFile)
{
    unsigned long theCount = 0;
    unsigned long theRead = 0;
    EEAmbientSoundRecord theRecord;
    long theResult;
    inFile.Open(kFileAccessRead);
    this->mRandom.SetSeed(TMGetTime());
    theResult = inFile.Read(theCount);
    if (theResult == 0) {
        this->mSounds.resize(theCount);
        for (unsigned long i = 0; i < theCount; i++) {
            theResult = inFile.Read(reinterpret_cast<unsigned char*>(&theRecord), sizeof(theRecord), theRead);
            if (theRecord.mIndex >= 0)
                this->mSounds[theRecord.mIndex].Init(theRecord);
        }
    }
    inFile.Close();
    return theResult;
}

// FUNCTION: EXE 0x5252b4
// Stops every ambient sound and pauses the manager
void EEAmbientSoundManager::Pause()
{
    ee_app_ambient_stop_sounds(EE_AMBIENT_HOST, true);
}

// FUNCTION: EXE 0x525685
// Runs one ambient update: day phase, terrain and animal sounds, the fades, then resets the world counters
void EEAmbientSoundManager::Update()
{
    ee_app_ambient_update(EE_AMBIENT_HOST);
}

// FUNCTION: EXE 0x52572e
// Enables or disables the ambient sounds: loads every sound object, or stops and unloads them (slot 0 is skipped)
void EEAmbientSoundManager::SetEnabled(bool inEnabled)
{
    ee_app_ambient_set_enabled(EE_AMBIENT_HOST, inEnabled);
}

// FUNCTION: EXE 0x5257bc
// Stops every ambient sound
void EEAmbientSoundManager::StopSounds()
{
    ee_app_ambient_stop_sounds(EE_AMBIENT_HOST, false);
}

// FUNCTION: EXE 0x5257f8
// Discards every ambient sound
void EEAmbientSoundManager::ClearSounds()
{
    this->mSounds.clear();
}

// FUNCTION: EXE 0x525800
// Unpauses the manager, running the next update straight away
void EEAmbientSoundManager::Restart()
{
    this->mIsPaused = false;
    this->mFrameCount = this->mNextUpdateFrame;
}

// Template and inline COMDATs (vector<EEAmbientSound>, EEAmbientSound copy/inline members, the analyzer's ??_G,
// CheckState):
// TEMPLATE: EXE 0x526c09
// TEMPLATE: EXE 0x526c25
// TEMPLATE: EXE 0x526c32
// TEMPLATE: EXE 0x526c6d
// TEMPLATE: EXE 0x526d29
// TEMPLATE: EXE 0x526d94
// TEMPLATE: EXE 0x526dcb
// TEMPLATE: EXE 0x526e04
// TEMPLATE: EXE 0x526e16
// TEMPLATE: EXE 0x5270ec
// TEMPLATE: EXE 0x527167
// TEMPLATE: EXE 0x527192
// TEMPLATE: EXE 0x5271a6
// TEMPLATE: EXE 0x5271ca
// TEMPLATE: EXE 0x5271f6
// TEMPLATE: EXE 0x527213
// TEMPLATE: EXE 0x52723f
// TEMPLATE: EXE 0x527251
// TEMPLATE: EXE 0x5272c6
// TEMPLATE: EXE 0x5272ea
// TEMPLATE: EXE 0x527312
// TEMPLATE: EXE 0x52733e
// TEMPLATE: EXE 0x52735f
// TEMPLATE: EXE 0x527385
