// C:\EEX\Empire Earth\Empire Earth\EEAmbientSoundManager.cpp (name from RTTI EEAmbientSoundManager; no __FILE__).
// 0x124ee2..0x1273d7: the ambient sounds (ctor, ??_G, dtor, the day-phase handlers, Load, the per-frame update and
// its animal/water/terrain parts), then the COMDATs: vector<EEAmbientSound>, EEAmbientSound's implicit copy members
// and inline members, the world analyzer's ??_G, the manager's inline Random/CheckState. The sound slots are indices
// into mSounds (7 wind?, 9/10/14/18 animals, 11/15/19 terrain, 20..27 water...) (provisional names; the big update
// functions are approximate).
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
#include "ee/EEGlobalConstants.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEMap.h"
#include "ee/EEMedia.h"
#include "ee/EESoundObject.h"
#include "ee/EEGameSound.h"
#include "ee/EEAmbientSound.h"
#include "ee/EEAmbientSoundManager.h"

// the unit's .rdata floats (0x441e14..0x441e3c)
static const float kAmbientNightTime = 0.07f;
static const float kAmbientBirdPitchHigh = 1.062f;
static const float kAmbientCricketPitchLow = 0.795f;
static const float kAmbientCricketPitchHigh = 1.125f;
static const float kAmbientAnimalPitchHigh = 1.065f;
static const float kAmbientTerrainPitchLow1 = 0.825f;
static const float kAmbientTerrainPitchHigh1 = 1.11f;
static const float kAmbientTerrainPitchLow2 = 0.805f;
static const float kAmbientTerrainPitchHigh2 = 1.096f;
static const float kAmbientTerrainPitchLow3 = 0.865f;
static const float kAmbientTerrainPitchHigh3 = 1.066f;

// FUNCTION: EXE 0x524ee2
// Creates the ambient sound manager, disabled and unpaused, updating every 10 frames
EEAmbientSoundManager::EEAmbientSoundManager()
    : mRandom(1), mFrameCount(0), mNextUpdateFrame(0), mUpdateInterval(10)
{
    this->mIsPaused = false;
    this->mIsEnabled = false;
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
// Picks the day phase (day, dusk, night, dawn) from the time of day and runs that phase's ambient update
void EEAmbientSoundManager::UpdateState(float inTimeOfDay, long inTime)
{
    long theState;
    if (inTimeOfDay < 0.5f) {
        theState = 3;
        if (inTimeOfDay >= kEEAmbientDuskTime)
            theState = 2;
    } else {
        theState = 4;
        if (inTimeOfDay >= kEEAmbientDawnTime)
            theState = 1;
    }
    this->mState = theState;
    switch (this->mState) {
    case 1:
        this->UpdateNight(inTimeOfDay, inTime);
        break;
    case 2:
        this->UpdateDusk(inTimeOfDay, inTime);
        break;
    case 3:
        this->UpdateDay(inTimeOfDay, inTime);
        break;
    case 4:
        this->UpdateDawn(inTimeOfDay, inTime);
        break;
    }
}

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

// FUNCTION: EXE 0x5250ae
// Silences the wind and, early in the day when due and the chance allows, plays a bird call (full and centred when
// farms are visible, else random volume and pan)
void EEAmbientSoundManager::UpdateDay(float inTimeOfDay, long inTime)
{
    this->mSounds[7].SetTargetVolume(0.0f);
    if (!(inTimeOfDay <= kAmbientNightTime) || !(this->mSounds[9].mNextTime <= inTime))
        return;
    float theRandom = this->Random();
    if (!(this->mSounds[9].mProbability >= theRandom))
        return;
    float theVolume;
    float thePan;
    if (this->mAnalyzer.mNumFarms == 0) {
        theVolume = this->RandomRange(0.35f, 0.6f);
        thePan = this->RandomRange(-1.0f, 1.0f);
    } else {
        theVolume = 1.0f;
        thePan = 0.0f;
    }
    float theMaxVolume = this->mSounds[9].mMaximumVolume;
    EEAmbientSound* theSound = &this->mSounds[9];
    float thePitch = this->RandomRange(0.9f, kAmbientBirdPitchHigh);
    gSoundManager->Play(theSound->mSound->GetSampleID(), theMaxVolume * theVolume, thePan, thePitch, false, NULL);
    this->mSounds[9].mNextTime = this->mSounds[9].mInterval + inTime;
}

// FUNCTION: EXE 0x525231
// Fades the wind up to its maximum volume at dawn
void EEAmbientSoundManager::UpdateDawn(float inTimeOfDay, long inTime)
{
    this->mSounds[7].SetTargetVolume(this->mSounds[7].mMaximumVolume);
}

// FUNCTION: EXE 0x525267
// Returns a random float between two bounds
float EEAmbientSoundManager::RandomRange(float inLow, float inHigh)
{
    return this->mRandom.Generate() * (inHigh - inLow) + inLow;
}

// FUNCTION: EXE 0x525281
// Returns a random integer between two bounds, inclusive
long EEAmbientSoundManager::RandomInteger(long inLow, long inHigh)
{
    long theLimit = inHigh + 1;
    long theResult = this->mRandom.Generate() * (theLimit - inLow) + inLow;
    if (theResult == theLimit)
        theResult = inHigh;
    return theResult;
}

// FUNCTION: EXE 0x5252b4
// Stops every ambient sound and pauses the manager
void EEAmbientSoundManager::Pause()
{
    this->StopSounds();
    this->mIsPaused = true;
}

// FUNCTION: EXE 0x5252c2
// Fades the wind with the time of night and, when due and the chance allows, plays a bird call; the next call comes
// after a random part of its interval
void EEAmbientSoundManager::UpdateNight(float inTimeOfDay, long inTime)
{
    float theVolume = inTimeOfDay * this->mSounds[7].mMaximumVolume;
    theVolume = max(0.0f, theVolume);
    if (this->mSounds[7].mTargetVolume != theVolume)
        this->mSounds[7].SetTargetVolume(theVolume);
    if (!(this->mSounds[9].mNextTime <= inTime))
        return;
    float theRandom = this->Random();
    if (!(theRandom <= this->mSounds[9].mProbability))
        return;
    float theScale;
    float thePan;
    if (this->mAnalyzer.mNumFarms == 0) {
        theScale = this->RandomRange(0.35f, 0.6f);
        thePan = this->RandomRange(-1.0f, 1.0f);
    } else {
        theScale = 1.0f;
        thePan = 0.0f;
    }
    gSoundManager->Play(this->mSounds[9].mSound->GetSampleID(),
                        this->mSounds[9].mMaximumVolume * theScale, thePan,
                        this->RandomRange(0.9f, kAmbientBirdPitchHigh), false, NULL);
    this->mSounds[9].mNextTime =
        (long)(this->mRandom.Generate() * this->mSounds[9].mInterval) + inTime;
}

// FUNCTION: EXE 0x5254c8
// Sets the wind's target volume for dusk, rising with the time of day between its start and maximum volume
void EEAmbientSoundManager::UpdateDusk(float inTimeOfDay, long inTime)
{
    float theFactor = ((inTimeOfDay + inTimeOfDay) - 0.9f) * 10.0f;
    // the CRT's double-evaluating min/max macros: the inner max is evaluated again for the result
    float theVolume = min(this->mSounds[7].mMaximumVolume, max(this->mSounds[7].mStartVolume, this->mSounds[7].mMaximumVolume * theFactor));
    if (this->mSounds[7].mTargetVolume != theVolume)
        this->mSounds[7].SetTargetVolume(theVolume);
}

// FUNCTION: EXE 0x525685
// Runs one ambient update: day phase, terrain and animal sounds, the fades, then resets the world counters
void EEAmbientSoundManager::Update()
{
    float theTimeOfDay = gWorld.GetTimeOfDay();
    unsigned long theTime = TMGetTime();
    this->CheckState(theTimeOfDay, theTime);
    this->UpdateTerrain(theTimeOfDay, theTime);
    this->UpdateAnimals(theTimeOfDay, theTime);
    for (long i = this->mSounds.size() - 1; i > 0; i--) {
        if (this->mSounds[i].mVolume != this->mSounds[i].mTargetVolume)
            this->mSounds[i].UpdateFade();
    }
    this->mAnalyzer.Clear();
    this->mAnalyzer.mForestType = 0;
    if (this->mState == 2)
        this->mState = 0;
    this->mNextUpdateFrame = this->mUpdateInterval + this->mFrameCount;
}

// FUNCTION: EXE 0x52572e
// Enables or disables the ambient sounds: loads every sound object, or stops and unloads them (slot 0 is skipped)
void EEAmbientSoundManager::SetEnabled(bool inEnabled)
{
    long i;
    if (this->mIsEnabled == inEnabled)
        return;
    this->mIsEnabled = inEnabled;
    if (!inEnabled) {
        for (i = this->mSounds.size() - 1; i > 0; i--) {
            this->mSounds[i].PlayAtVolume(0.0f);
            this->mSounds[i].UnloadSound();
        }
    } else {
        for (i = this->mSounds.size() - 1; i > 0; i--)
            this->mSounds[i].LoadSound();
    }
}

// FUNCTION: EXE 0x5257bc
// Stops every ambient sound
void EEAmbientSoundManager::StopSounds()
{
    for (long i = this->mSounds.size() - 1; i >= 0; i--)
        this->mSounds[i].Stop();
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

// FUNCTION: EXE 0x52580b
// Plays the dawn/dusk animal calls and the citizen chatter (more than 3 citizens visible) when due; these play when
// the random draw is above the sound's probability, unlike the other ambient sounds
void EEAmbientSoundManager::UpdateAnimals(float inTimeOfDay, long inTime)
{
    float theRatio;
    if (gWorld.mMap->mTerrainMesh.GetNumVisibleTiles() != 0)
        theRatio = (float)this->mAnalyzer.mNumTrees / (float)gWorld.mMap->mTerrainMesh.GetNumVisibleTiles();
    else
        theRatio = 0.0f;
    if (this->mState == 4 || this->mState == 2) {
        if (this->mSounds[10].mNextTime <= inTime) {
            float theRandom = this->Random();
            // play with chance p, as every other ambient sound (the test was reversed: chance 1 - p)
            if (theRandom <= this->mSounds[10].mProbability) {
                float theVolume;
                float thePan;
                if (theRatio >= 0.11f) {
                    theVolume = this->mSounds[10].mMaximumVolume * this->RandomRange(1.0f, 1.3f);
                    thePan = 0.0f;
                } else {
                    theVolume = this->mSounds[10].mMaximumVolume * this->RandomRange(0.25f, 0.65f);
                    thePan = this->RandomRange(-1.0f, 1.0f);
                }
                EEAmbientSound* theSound = &this->mSounds[10];
                float thePitch = this->RandomRange(0.9f, kAmbientAnimalPitchHigh);
                gSoundManager->Play(theSound->mSound->GetSampleID(), theVolume, thePan, thePitch, false, NULL);
                this->mSounds[10].mNextTime = this->mSounds[10].mInterval + inTime;
            }
        }
    }
    if (this->mState == 4 || this->mState == 2) {
        if (this->mSounds[14].mNextTime <= inTime) {
            float theRandom = this->Random();
            // play with chance p, as every other ambient sound (the test was reversed: chance 1 - p)
            if (theRandom <= this->mSounds[14].mProbability) {
                float thePan = this->RandomRange(-1.0f, 1.0f);
                gSoundManager->Play(this->mSounds[14].mSound->GetSampleID(),
                                    this->mSounds[14].mMaximumVolume, thePan,
                                    this->RandomRange(kAmbientCricketPitchLow, kAmbientCricketPitchHigh), false, NULL);
                this->mSounds[14].mNextTime = this->mSounds[14].mInterval + inTime;
            }
        }
    }
    if (!(this->mAnalyzer.mNumCitizens > 3))
        return;
    if (!(this->mSounds[18].mNextTime <= inTime))
        return;
    if (this->mSounds[18].mInterval + 12000 < inTime - this->mSounds[18].mNextTime) {
        this->mSounds[18].mNextTime = this->RandomInteger(100, this->mSounds[18].mInterval) + inTime;
    } else {
        float theRandom = this->Random();
        // play with chance p, as every other ambient sound (the test was reversed: chance 1 - p)
        if (theRandom <= this->mSounds[18].mProbability) {
            float thePan = this->RandomRange(-1.0f, 1.0f);
            gSoundManager->Play(this->mSounds[18].mSound->GetSampleID(),
                                this->mSounds[18].mMaximumVolume, thePan,
                                this->RandomRange(0.6f, 1.2f), false, NULL);
            this->mSounds[18].mNextTime = this->mSounds[18].mInterval + inTime;
        }
    }
}

// FUNCTION: EXE 0x525d0d
// Fades in one forest sound (picked by the forest type) when trees fill the view by day or night, fading out the
// others; returns whether a forest sound plays
bool EEAmbientSoundManager::UpdateWater(float inTimeOfDay, long inTime, bool inForce, float inRatio)
{
    bool theResult = false;
    if (!inForce && (this->mState == 3 || this->mState == 1) && (inRatio >= 0.11f || this->mAnalyzer.mNumTrees >= 160)) {
        EEAmbientSound* theSound;
        EEAmbientSound* theOther1;
        EEAmbientSound* theOther2;
        switch (this->mAnalyzer.mForestType) {
        default:
            theSound = &this->mSounds[20];
            theOther1 = &this->mSounds[21];
            theOther2 = &this->mSounds[22];
            break;
        case 3:
            theSound = &this->mSounds[22];
            theOther1 = &this->mSounds[21];
            theOther2 = &this->mSounds[20];
            break;
        case 2:
            theSound = &this->mSounds[21];
            theOther1 = &this->mSounds[20];
            theOther2 = &this->mSounds[22];
            break;
        }
        float theVolume = theSound->mTargetVolume;
        if (theVolume == 0.0f || theSound->mNextTime < inTime) {
            theSound->mNextTime = theSound->mInterval + inTime;
            theVolume = this->RandomRange(0.05f, theSound->mMaximumVolume + kHeightEpsilon);
        }
        theResult = true;
        theSound->SetTargetVolume(theVolume);
        if (theOther1->mInstance != NULL)
            theOther1->SetTargetVolume(0.0f);
        if (theOther2->mInstance != NULL)
            theOther2->SetTargetVolume(0.0f);
    } else {
        if (this->mSounds[20].mInstance != NULL)
            this->mSounds[20].SetTargetVolume(0.0f);
        if (this->mSounds[21].mInstance != NULL)
            this->mSounds[21].SetTargetVolume(0.0f);
        if (this->mSounds[22].mInstance != NULL)
            this->mSounds[22].SetTargetVolume(0.0f);
    }
    return theResult;
}

// FUNCTION: EXE 0x525fcc
// Sets the terrain ambience from the visible units and tiles: crowd and battle noise, farm/goat/eagle calls, air
// raid sirens, wind over water, space sounds, forest sounds and the quiet background
void EEAmbientSoundManager::UpdateTerrain(float inTimeOfDay, long inTime)
{
    float theWater = gWorld.mMap->mTerrainMesh.GetVisibleWaterTilePercentage();
    float theSpace = gWorld.mMap->mTerrainMesh.GetVisibleSpaceTilePercentage();
    float theRatio;
    float theBase;
    if (gWorld.mMap->mTerrainMesh.GetNumVisibleTiles() != 0)
        theRatio = (float)this->mAnalyzer.mNumTrees / (float)gWorld.mMap->mTerrainMesh.GetNumVisibleTiles();
    else
        theRatio = 0.0f;
    if (this->mAnalyzer.mNumCitizens >= 4) {
        theBase = this->mSounds[1].mStartVolume;
        this->mSounds[1].SetTargetVolume((float)this->mAnalyzer.mNumCitizens * 0.02f + theBase);
    } else {
        if (this->mSounds[1].mInstance != NULL)
            this->mSounds[1].SetTargetVolume(0.0f);
    }
    if (this->mAnalyzer.mNumAttackers >= 5) {
        this->mSounds[1].SetTargetVolume(0.0f);
        theBase = this->mSounds[2].mStartVolume;
        this->mSounds[2].SetTargetVolume((float)this->mAnalyzer.mNumAttackers * 0.02f + theBase);
    } else {
        if (this->mSounds[2].mInstance != NULL)
            this->mSounds[2].SetTargetVolume(0.0f);
    }
    if (this->mAnalyzer.mNumFarms != 0 && (this->mState == 3 || this->mState == 1)) {
        if (this->mSounds[11].mNextTime <= inTime) {
            float theRandom = this->Random();
            if (theRandom <= this->mSounds[11].mProbability) {
                this->mSounds[11].mNextTime = this->mSounds[11].mInterval + inTime;
                float theVolume = this->mSounds[11].mMaximumVolume;
                EEAmbientSound* theSound = &this->mSounds[11];
                float thePitch = this->RandomRange(kAmbientTerrainPitchLow3, kAmbientTerrainPitchHigh3);
                float thePan = this->RandomRange(-1.0f, 1.0f);
                gSoundManager->Play(theSound->mSound->GetSampleID(), this->Random() * theVolume, thePan, thePitch, false, NULL);
            }
        }
    }
    if (this->mAnalyzer.mNumGoats != 0 && (this->mState == 3 || this->mState == 1)) {
        if (this->mSounds[19].mNextTime <= inTime) {
            float theRandom = this->Random();
            if (theRandom <= this->mSounds[19].mProbability) {
                this->mSounds[19].mNextTime = this->mSounds[19].mInterval + inTime;
                float theVolume = this->mSounds[19].mMaximumVolume;
                EEAmbientSound* theSound = &this->mSounds[19];
                float thePitch = this->RandomRange(kAmbientTerrainPitchLow2, kAmbientTerrainPitchHigh2);
                float thePan = this->RandomRange(-1.0f, 1.0f);
                gSoundManager->Play(theSound->mSound->GetSampleID(), this->Random() * theVolume, thePan, thePitch, false, NULL);
            }
        }
    }
    if (this->mAnalyzer.mNumEagles != 0 && (this->mState == 3 || this->mState == 1)) {
        if (this->mSounds[15].mNextTime <= inTime) {
            float theRandom = this->Random();
            if (theRandom <= this->mSounds[15].mProbability) {
                this->mSounds[15].mNextTime = this->mSounds[15].mInterval + inTime;
                float theVolume = this->mSounds[15].mMaximumVolume;
                EEAmbientSound* theSound = &this->mSounds[15];
                float thePitch = this->RandomRange(kAmbientTerrainPitchLow1, kAmbientTerrainPitchHigh1);
                float thePan = this->RandomRange(-1.0f, 1.0f);
                gSoundManager->Play(theSound->mSound->GetSampleID(), this->Random() * theVolume, thePan, thePitch, false, NULL);
            }
        }
    }
    if (this->mAnalyzer.mNumAirRaiders == 0) {
        if (this->mSounds[8].mTargetVolume != 0.0f)
            this->mSounds[8].SetTargetVolume(0.0f);
    } else {
        this->mSounds[8].SetTargetVolume(this->mSounds[8].mMaximumVolume);
    }
    bool theWindy = theWater > 0.7f || (theWater > 0.2f && theWater > theRatio * 1.5f);
    if (theWindy) {
        this->mSounds[3].SetTargetVolume(this->mSounds[3].mMaximumVolume);
    } else {
        if (this->mSounds[3].mInstance != NULL)
            this->mSounds[3].SetTargetVolume(0.0f);
    }
    bool theSpaceSounds = theSpace > 0.7f || (theSpace > 0.2f && theSpace > theRatio * 1.5f);
    if (theSpaceSounds) {
        if (this->mSounds[26].mTargetVolume == 0.0f && this->mSounds[27].mTargetVolume == 0.0f) {
            float theRandom = this->Random();
            if (theRandom <= this->mSounds[26].mProbability)
                this->mSounds[26].SetTargetVolume(this->mSounds[26].mMaximumVolume);
            else
                this->mSounds[27].SetTargetVolume(this->mSounds[27].mMaximumVolume);
        }
        for (long i = 23; i <= 25; i++) {
            if (!(this->mSounds[i].mNextTime <= inTime))
                continue;
            float theRandom = this->Random();
            if (theRandom <= this->mSounds[i].mProbability) {
                this->mSounds[i].mNextTime = this->mSounds[i].mInterval + inTime;
                float theVolume = this->mSounds[i].mMaximumVolume;
                EEAmbientSound* theSound = &this->mSounds[i];
                float thePitch = this->RandomRange(kAmbientTerrainPitchLow1, kAmbientTerrainPitchHigh1);
                float thePan = this->RandomRange(-1.0f, 1.0f);
                gSoundManager->Play(theSound->mSound->GetSampleID(), this->Random() * theVolume, thePan, thePitch, false, NULL);
            }
        }
    } else {
        if (this->mState == 3 || this->mState == 1) {
            if (this->mSounds[12].mNextTime <= inTime) {
                float theRandom = this->Random();
                // play with chance p, as every other ambient sound (the test was reversed: chance 1 - p)
                if (theRandom <= this->mSounds[12].mProbability) {
                    float theVolume = this->mSounds[12].mMaximumVolume;
                    EEAmbientSound* theSound = &this->mSounds[12];
                    float thePitch = this->RandomRange(0.8f, kAmbientAnimalPitchHigh);
                    float thePan = this->RandomRange(-1.0f, 1.0f);
                    gSoundManager->Play(theSound->mSound->GetSampleID(), this->Random() * theVolume, thePan, thePitch, false, NULL);
                    this->mSounds[12].mNextTime = this->mSounds[12].mInterval + inTime;
                }
            }
        }
    }
    bool theQuiet = this->UpdateWater(inTimeOfDay, inTime, false, theRatio) || theWindy || theSpaceSounds || this->mState == 4 ||
                    this->mState == 2;
    this->mSounds[13].SetTargetVolume(theQuiet ? 0.0f : this->mSounds[13].mMaximumVolume);
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
