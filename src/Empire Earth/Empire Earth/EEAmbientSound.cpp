// C:\EEX\Empire Earth\Empire Earth\EEAmbientSound.cpp (name from the class EEAmbientSound; no __FILE__). 0x124a88..0x124ee2:
// RTSVisibleMap.cpp's key chains end at 0x124a88, EEAmbientSoundManager.cpp starts at 0x124ee2. The two EE-level
// trigonometry caller logs ("UMCos/UMSin EE-Level"), the ambient sound (ctor, ??_G, dtor, Init, the fades, start,
// play, unload), then the manager's world analyzer counters. Unit flags: the exe module set.
#include <windows.h>
#include <mbstring.h>
#include <algorithm>
#include <string>
#include <list>
#include <map>
#include <vector>
#include <stdlib.h>
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
#include "lle/UIBlinkingButton.h"
#include "lle/NEMessage.h"
#include "lle/NESynchronousCommunications.h"
#include "lle/NESynchronousServer.h"
#include "lle/NESynchronousClient.h"
#include "lle/GEFont.h"
#include "lle/GEText.h"
#include "lle/UIFormManager.h"
#include "lle/UResourceFile.h"
#include "lle/GE2DPoint.h"
#include "lle/GE3DPoint.h"
#include "lle/GETransformation.h"
#include "lle/GETerrainMesh.h"
#include "lle/GE3DLine.h"
#include "lle/GEPhysicalModel.h"
#include "lle/SESoundManager.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEMap.h"
#include "ee/EEMedia.h"
#include "ee/EESoundObject.h"
#include "ee/EEGameSound.h"
#include "ee/EEAmbientSound.h"
#include "ee/EEAmbientSoundManager.h"

#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEUnitGoal.h"
#include "lle/U2DSparseArray.h"
#include "lle/U2DPolygon.h"
#include "ee/EEAIArea.h"
#include "ee/EEAIForest.h"
#include "ee/EETown.h"
#include "ee/EEAIContinentManager.h"

extern EEAIContinentManager gContinentManager;

// 0x524a88..0x524b34: the original's two EE-level trigonometry caller logs ("UMCos EE-Level", "UMSin EE-Level",
// ESCallerLog, never called), deleted with the class (LLE lane round 1)

// FUNCTION: EXE 0x524b34
// Creates a silent ambient sound with no sound object, full probability and normal pitch
EEAmbientSound::EEAmbientSound()
    : mSound(NULL), mSoundID(0), mInstance(NULL), mProbability(1.0f), mVolume(0.0f), mTargetVolume(0.0f), mInterval(0),
      mNextTime(0)
{






    this->mMaximumVolume = 0.0f;
    this->mStartVolume = 0.0f;
    this->mFadeUp = 0.0f;
    this->mFadeDown = 0.0f;
    this->mPan = 0.0f;
    this->mPitch = 1.0f;
    this->mF38 = 0.0f;
    this->mF3C = 1.0f;
}

// FUNCTION: EXE 0x524b85
// Destroys the ambient sound and frees it when asked (scalar deleting destructor)

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
    this->mSoundIndex = inRecord.mSoundID;
    this->mSound = NULL;
    this->mFadeUp = inRecord.mFadeUp;
    this->mFadeDown = inRecord.mFadeDown;
    this->mSoundID = inRecord.mSoundID;
    this->mStartVolume = inRecord.mStartVolume;
    this->mMaximumVolume = inRecord.mMaximumVolume;
    this->mProbability = inRecord.mProbability;
    this->mTargetVolume = 0.0f;
    this->mVolume = 0.0f;
    this->mIsLooping = false;
    this->mNextTime = 0;
    this->mInterval = inRecord.mIntervalSeconds != 0 ? inRecord.mIntervalSeconds * 1000 : 0;
}

// FUNCTION: EXE 0x524c16
// Raises the volume one fade step toward the target (from silence straight to the start volume) and plays it
void EEAmbientSound::FadeIn()
{
    float theVolume = this->mVolume != 0.0f ? this->mFadeUp * this->mVolume : this->mStartVolume;
    if (theVolume > this->mTargetVolume)
        theVolume = this->mTargetVolume;
    this->PlayAtVolume(theVolume);
}

// FUNCTION: EXE 0x524c46
// Lowers the volume one fade step toward the target, silencing the sound at 0.005 or below
// (the 0.005 silence threshold: .rdata 0x43f924)
void EEAmbientSound::FadeOut()
{
    float theVolume = this->mFadeDown * this->mVolume;
    if (theVolume < this->mTargetVolume)
        theVolume = this->mTargetVolume;
    if (theVolume <= 0.005f)
        theVolume = 0.0f;
    this->PlayAtVolume(theVolume);
}

// FUNCTION: EXE 0x524c72
// Loads the sound object and its media on first use; returns whether the ambient sound has one
bool EEAmbientSound::LoadSound()
{
    bool theResult = this->mSound != NULL;
    if (this->mSound != NULL)
        return theResult;
    this->mSound = EESoundObject::Get(this->mSoundIndex);
    if (this->mSound != NULL) {
        theResult = true;
        this->mSound->LoadMedia();
        this->mIsLooping = this->mSound->GetLooping();
    }
    return theResult;
}

// FUNCTION: EXE 0x524ca9
// Plays the sound at a volume (starting it at the start volume, or updating the playing instance); 0 or less stops it
void EEAmbientSound::PlayAtVolume(float inVolume)
{
    if (inVolume > 0.0f) {
        if (this->mInstance != NULL) {
            this->mVolume = inVolume;
            gSoundManager->SetSoundParameters(this->mInstance, inVolume, this->mPan, this->mPitch);
        } else {
            this->mVolume = this->mStartVolume;
            this->mInstance = gSoundManager->PlayAndReturnReferencedNode(this->mSound->GetSampleID(), this->mVolume, this->mPan, this->mPitch,
                                                                   this->mIsLooping, NULL);
        }
    } else {
        if (this->mInstance != NULL) {
            gSoundManager->Stop(this->mInstance, true);
            this->ReleaseInstance();
        }
        this->mTargetVolume = 0.0f;
        this->mVolume = 0.0f;
    }
}

// FUNCTION: EXE 0x524d4e
// Unloads the sound object's media and forgets the sound object
void EEAmbientSound::UnloadSound()
{
    if (this->mSound != NULL)
        this->mSound->UnloadMedia();
    this->mSound = NULL;
}

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
        if (thePlayer->mTowns[i]->mHull.BoundedContains(theTile)) {
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
        this->mForestType = theForest->mResourceKind;
}

// FUNCTION: EXE 0x524ec1
// Resets the world analyzer's counters for the next ambient update
void EEAmbientWorldAnalyzer::Clear()
{
    this->mNumTrees = 0;
    this->mNumCitizens = 0;
    this->mNumAttackers = 0;
    this->mNumAirRaiders = 0;
    this->mNumFarms = 0;
    this->mNumGoats = 0;
    this->mNumEagles = 0;
    this->mL20 = 0;
    this->mL24 = 0;
    this->mL28 = 0;
}
