// C:\EEX\Empire Earth\Calamities\EECalamity.cpp (file name inferred: no __FILE__; the class's save keys, ctors, members
// and the vector load/save templates it instantiates first). One active calamity: 0xacb75..0xadcd9 (init chains of
// the 21 save keys 0xacb75..0xad0f4). Unit flags: the exe's module set.
// Data (.rdata, in order): -10000.0f 0x43c690 (not referenced by the unit's code), the vtable 0x43c698 (COL
// 0x43c694). (.data): the key strings 0x4b90d8.., type descriptors, the dump formats 0x4b9324..; (.bss): the keys
// 0x4e5f58..0x4e5fa0 and their guard bytes.
#include <windows.h>
#include <stdio.h>

#include "lle/LowLevelEngine.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GE3DPlane.h"
#include "lle/GETerrainMesh.h"
#include "lle/FSRestorable.h"
#include "lle/SESMInstanceNode.h"
#include "lle/SESoundManager.h"
#include "lle/USingleton.h"

#include "ee/EEEffectManager.h"
#include "ee/EECalamity.h"
#include "ee/EECalamityClass.h"
#include "ee/EECalamityManager.h"
#include "ee/EEGameSound.h"
#include "ee/EEPlayer.h"
#include "ee/EEAreaEffect.h"
#include "ee/EEUnit.h"
#include "ee/EEVectorIO.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"

const UString EECalamity::mEffectGenerationStringID("EffectGenerationStringID");
const UString EECalamity::mTargetUnitStringID("TargetUnitStringID");
const UString EECalamity::mClassEnumStringID("ClassEnumStringID");
const UString EECalamity::mEpicenterFactorStringID("EpicenterFactorStringID");
const UString EECalamity::mFloatPlaceHolder1StringID("FloatPlaceHolder1StringID");
const UString EECalamity::mFloatPlaceHolder2StringID("FloatPlaceHolder2StringID");
const UString EECalamity::mHeightStringID("HeightStringID");
const UString EECalamity::mIntPlaceHolder1StringID("IntPlaceHolder1StringID");
const UString EECalamity::mIntPlaceHolder2StringID("IntPlaceHolder2StringID");
const UString EECalamity::mTimeAliveStringID("TimeAliveStringID");
const UString EECalamity::mOriginatingUnitIDStringID("OriginatingUnitIDStringID");
const UString EECalamity::mPlayerNumberStringID("PlayerNumberStringID");
const UString EECalamity::mDiscreteUpdateTimeStringID("DiscreteUpdateTimeStringID");
const UString EECalamity::mTimeElapsedSinceLastUpdateStringID("TimeElapsedSinceLastUpdateStringID");
const UString EECalamity::mBoolPlaceHolder1StringID("BoolPlaceHolder1StringID");
const UString EECalamity::mBoolPlaceHolder2StringID("BoolPlaceHolder2StringID");
const UString EECalamity::mInitializedStringID("InitializedStringID");
const UString EECalamity::mHasTargetLocationStringID("HasTargetLocationStringID");
const UString EECalamity::mHasTargetUnitStringID("HasTargetUnitStringID");
const UString EECalamity::mTilePlaceHolder1StringID("TilePlaceHolder1StringID");
const UString EECalamity::mEffectHandleStringID("EffectHandleStringID");

// the 21 key init chains (registrar, ctor, atexit registrar, guarded dtor each)
// SYNTHETIC: EXE 0x4acb75   _$E91
// SYNTHETIC: EXE 0x4acb7f   _$E87
// SYNTHETIC: EXE 0x4acb90   _$E90
// SYNTHETIC: EXE 0x4acb9c   _$E88
// SYNTHETIC: EXE 0x4acbb8   _$E96
// SYNTHETIC: EXE 0x4acbc2   _$E93
// SYNTHETIC: EXE 0x4acbd3   _$E95
// SYNTHETIC: EXE 0x4acbdf   _$E94
// SYNTHETIC: EXE 0x4acbfb   _$E101
// SYNTHETIC: EXE 0x4acc05   _$E98
// SYNTHETIC: EXE 0x4acc16   _$E100
// SYNTHETIC: EXE 0x4acc22   _$E99
// SYNTHETIC: EXE 0x4acc3e   _$E106
// SYNTHETIC: EXE 0x4acc48   _$E103
// SYNTHETIC: EXE 0x4acc59   _$E105
// SYNTHETIC: EXE 0x4acc65   _$E104
// SYNTHETIC: EXE 0x4acc81   _$E111
// SYNTHETIC: EXE 0x4acc8b   _$E108
// SYNTHETIC: EXE 0x4acc9c   _$E110
// SYNTHETIC: EXE 0x4acca8   _$E109
// SYNTHETIC: EXE 0x4accc4   _$E116
// SYNTHETIC: EXE 0x4accce   _$E113
// SYNTHETIC: EXE 0x4accdf   _$E115
// SYNTHETIC: EXE 0x4acceb   _$E114
// SYNTHETIC: EXE 0x4acd07   _$E121
// SYNTHETIC: EXE 0x4acd11   _$E118
// SYNTHETIC: EXE 0x4acd22   _$E120
// SYNTHETIC: EXE 0x4acd2e   _$E119
// SYNTHETIC: EXE 0x4acd4a   _$E126
// SYNTHETIC: EXE 0x4acd54   _$E123
// SYNTHETIC: EXE 0x4acd65   _$E125
// SYNTHETIC: EXE 0x4acd71   _$E124
// SYNTHETIC: EXE 0x4acd8d   _$E132
// SYNTHETIC: EXE 0x4acd97   _$E128
// SYNTHETIC: EXE 0x4acda8   _$E131
// SYNTHETIC: EXE 0x4acdb4   _$E129
// SYNTHETIC: EXE 0x4acdd0   _$E137
// SYNTHETIC: EXE 0x4acdda   _$E134
// SYNTHETIC: EXE 0x4acdeb   _$E136
// SYNTHETIC: EXE 0x4acdf7   _$E135
// SYNTHETIC: EXE 0x4ace13   _$E142
// SYNTHETIC: EXE 0x4ace1d   _$E139
// SYNTHETIC: EXE 0x4ace2e   _$E141
// SYNTHETIC: EXE 0x4ace3a   _$E140
// SYNTHETIC: EXE 0x4ace56   _$E147
// SYNTHETIC: EXE 0x4ace60   _$E144
// SYNTHETIC: EXE 0x4ace71   _$E146
// SYNTHETIC: EXE 0x4ace7d   _$E145
// SYNTHETIC: EXE 0x4ace99   _$E152
// SYNTHETIC: EXE 0x4acea3   _$E149
// SYNTHETIC: EXE 0x4aceb4   _$E151
// SYNTHETIC: EXE 0x4acec0   _$E150
// SYNTHETIC: EXE 0x4acedc   _$E157
// SYNTHETIC: EXE 0x4acee6   _$E154
// SYNTHETIC: EXE 0x4acef7   _$E156
// SYNTHETIC: EXE 0x4acf03   _$E155
// SYNTHETIC: EXE 0x4acf1f   _$E162
// SYNTHETIC: EXE 0x4acf29   _$E159
// SYNTHETIC: EXE 0x4acf3a   _$E161
// SYNTHETIC: EXE 0x4acf46   _$E160
// SYNTHETIC: EXE 0x4acf62   _$E167
// SYNTHETIC: EXE 0x4acf6c   _$E164
// SYNTHETIC: EXE 0x4acf7d   _$E166
// SYNTHETIC: EXE 0x4acf89   _$E165
// SYNTHETIC: EXE 0x4acfa5   _$E173
// SYNTHETIC: EXE 0x4acfaf   _$E169
// SYNTHETIC: EXE 0x4acfc0   _$E172
// SYNTHETIC: EXE 0x4acfcc   _$E170
// SYNTHETIC: EXE 0x4acfe8   _$E178
// SYNTHETIC: EXE 0x4acff2   _$E175
// SYNTHETIC: EXE 0x4ad003   _$E177
// SYNTHETIC: EXE 0x4ad00f   _$E176
// SYNTHETIC: EXE 0x4ad02b   _$E183
// SYNTHETIC: EXE 0x4ad035   _$E180
// SYNTHETIC: EXE 0x4ad046   _$E182
// SYNTHETIC: EXE 0x4ad052   _$E181
// SYNTHETIC: EXE 0x4ad06e   _$E188
// SYNTHETIC: EXE 0x4ad078   _$E185
// SYNTHETIC: EXE 0x4ad089   _$E187
// SYNTHETIC: EXE 0x4ad095   _$E186
// SYNTHETIC: EXE 0x4ad0b1   _$E193
// SYNTHETIC: EXE 0x4ad0bb   _$E190
// SYNTHETIC: EXE 0x4ad0cc   _$E192
// SYNTHETIC: EXE 0x4ad0d8   _$E191

// FUNCTION: EXE 0x4ad0f4
// Creates an active calamity of the class for the caster (referenced) and loads the class's media for the caster's
// player (nature without a caster)
EECalamity::EECalamity(EECalamityClass* inClass, EEUnit* inCasterUnit)
    : mEffectGeneration(6), mTilePlaceHolder1X(0), mTilePlaceHolder1Y(0)
{
    this->mTargetUnit = NULL;
    this->mCasterUnit = NULL;
    this->mSound = NULL;
    this->mSecondarySound = NULL;
    this->Reset();
    this->mClass = inClass;
    this->SetCasterUnit(inCasterUnit);
    this->mClass->LoadMedia(inCasterUnit != NULL ? inCasterUnit->mPlayer : ::gWorld.mPlayers[0], true);
}

// SYNTHETIC: EXE 0x4ad191
// (the scalar deleting destructor, emitted with the vtable)

// FUNCTION: EXE 0x4ad1ad
// Creates an empty calamity with no class, target or caster (for loading)
EECalamity::EECalamity()
    : mEffectGeneration(6), mTilePlaceHolder1X(0), mTilePlaceHolder1Y(0)
{
    this->mTargetUnit = NULL;
    this->mCasterUnit = NULL;
    this->mSound = NULL;
    this->mSecondarySound = NULL;
    this->Reset();
}

// FUNCTION: EXE 0x4ad218
// Destroys the calamity: stops its sounds, unloads its class's media unless it has a caster id but no caster unit,
// and releases its units
EECalamity::~EECalamity()
{
    this->StopSound();
    this->StopSecondarySound();
    if (this->mOriginatingUnitID == -1 || this->mCasterUnit != NULL)
        this->mClass->UnloadMedia(this->mCasterUnit != NULL ? this->mCasterUnit->mPlayer : ::gWorld.mPlayers[0]);
    this->Reset();
}

// FUNCTION: EXE 0x4ad267
// Stops the calamity's main sound and releases it
void EECalamity::StopSound()
{
    if (this->mSound != NULL) {
        gSoundManager->Stop(this->mSound, false);
        this->ReleaseSound();
    }
}

// FUNCTION: EXE 0x4ad289
// Releases the calamity's main sound without stopping it
void EECalamity::ReleaseSound()
{
    if (this->mSound != NULL)
        this->mSound->Dereference();
    this->mSound = NULL;
}

// FUNCTION: EXE 0x4ad2ad
// Releases the calamity's secondary sound without stopping it
void EECalamity::ReleaseSecondarySound()
{
    if (this->mSecondarySound != NULL)
        this->mSecondarySound->Dereference();
    this->mSecondarySound = NULL;
}

// FUNCTION: EXE 0x4ad2d1
// Clears the calamity: no class, target or caster (both released), sounds released, every timer, flag and placeholder
// zeroed
void EECalamity::Reset()
{
    this->mEpicenterFactor = 1.0f;
    this->mClass = NULL;
    this->mHasTargetLocation = false;
    this->mHasTargetUnit = false;
    if (this->mTargetUnit != NULL) {
        this->mTargetUnit->Dereference();
        this->mTargetUnit = NULL;
    }
    if (this->mCasterUnit != NULL) {
        this->mCasterUnit->Dereference();
        this->mCasterUnit = NULL;
    }
    for (long i = 0; i < 6; i++)
        *(this->mEffectGeneration.begin() + i) = 0;
    this->ReleaseSound();
    this->ReleaseSecondarySound();
    this->mOriginatingUnitID = -1;
    this->mTimeAlive = 0.0f;
    this->mTargetUnitID = -1;
    this->mDiscreteUpdateTime = 0.0f;
    this->mInitialized = false;
    this->mEffectHandle.mValue = 0;
    this->mIntPlaceHolder1 = 0;
    this->mTimeElapsedSinceLastUpdate = 0.0f;
    this->mIntPlaceHolder2 = 0;
    this->mBoolPlaceHolder1 = false;
    this->mBoolPlaceHolder2 = false;
    this->mHeight = 0.0f;
    this->mPlayerNumber = 0;
    this->mFloatPlaceHolder1 = 0.0f;
    this->mFloatPlaceHolder2 = 0.0f;
}

// FUNCTION: EXE 0x4ad377
// Writes the calamity's state to the out-of-sync dump
void EECalamity::DumpOutOfSynch(FILE* inFile)
{
    fprintf(inFile, "\nOUT OF SYNCH DUMP:  Calamity Class is %d\n", this->mClass->mClassEnum);
    fprintf(inFile, "OUT OF SYNCH DUMP:  Calamity Target Location X: %6.6f\n", this->mTargetLocation.GetX());
    fprintf(inFile, "OUT OF SYNCH DUMP:  Calamity Target Location Y: %6.6f\n", this->mTargetLocation.GetY());
    fprintf(inFile, "OUT OF SYNCH DUMP:  Calamity Target Location Z: %6.6f\n", this->mTargetLocation.GetZ());
    fprintf(inFile, "OUT OF SYNCH DUMP:  Calamity Tile Int PlaceHolder1 X: %d\n", this->mTilePlaceHolder1X);
    fprintf(inFile, "OUT OF SYNCH DUMP:  Calamity Tile Int PlaceHolder1 Y: %d\n", this->mTilePlaceHolder1Y);
    fprintf(inFile, "OUT OF SYNCH DUMP:  Calamity Integer PlaceHolder1 %d\n", this->mIntPlaceHolder1);
    fprintf(inFile, "OUT OF SYNCH DUMP:  Calamity Integer PlaceHolder2 %d\n", this->mIntPlaceHolder2);
    fprintf(inFile, "OUT OF SYNCH DUMP:  Calamity Target Unit ID #%08x\n", this->mTargetUnitID);
    fprintf(inFile, "OUT OF SYNCH DUMP:  Calamity Caster Unit ID #%08x\n", this->mOriginatingUnitID);
    fprintf(inFile, "OUT OF SYNCH DUMP:  Calamity Player Number %d\n", this->mPlayerNumber);
    fprintf(inFile, "OUT OF SYNCH DUMP:  Calamity Bool PlaceHolder 1 has a value of %d\n", this->mBoolPlaceHolder1 == true);
    fprintf(inFile, "OUT OF SYNCH DUMP:  Calamity Bool PlaceHolder 2 has a value of %d\n", this->mBoolPlaceHolder2 == true);
    fprintf(inFile, "OUT OF SYNCH DUMP:  Calamity Bool Initialized has a value of %d\n", this->mInitialized == true);
    fprintf(inFile, "OUT OF SYNCH DUMP:  Calamity Bool HasTargetLocation has a value of %d\n",
            this->mHasTargetLocation == true);
    fprintf(inFile, "OUT OF SYNCH DUMP:  Calamity Bool HasTargetUnit has a value of %d\n", this->mHasTargetUnit == true);
    fprintf(inFile, "OUT OF SYNCH DUMP:  Calamity Float PlaceHolder 1 is %6.6f\n", this->mFloatPlaceHolder1);
    fprintf(inFile, "OUT OF SYNCH DUMP:  Calamity Float PlaceHolder 2 is %6.6f\n", this->mFloatPlaceHolder2);
    fprintf(inFile, "OUT OF SYNCH DUMP:  Calamity Height is %6.6f\n", this->mHeight);
    fprintf(inFile, "OUT OF SYNCH DUMP:  Calamity Discrete Update Time is %6.6f\n", this->mDiscreteUpdateTime);
    fprintf(inFile, "OUT OF SYNCH DUMP:  Calamity Time Elapsed Since Last Update is %6.6f\n",
            this->mTimeElapsedSinceLastUpdate);
}

// ICF-FOLDED: EXE 0x557269
// (empty: folded into a shared empty function)
void EECalamity::Virtual18(unsigned long)
{
}

// FUNCTION: EXE 0x4ad50f
// Returns a sync checksum: the sum of the calamity's float bit patterns, ids, placeholders and flags
long EECalamity::GetOutOfSynchChecksum()
{
    float theValue;
    long theChecksum;
    theValue = this->mTargetLocation.GetX();
    theChecksum = *reinterpret_cast<long*>(&theValue);
    theValue = this->mTargetLocation.GetY();
    theChecksum += *reinterpret_cast<long*>(&theValue);
    theValue = this->mTargetLocation.GetZ();
    theChecksum += *reinterpret_cast<long*>(&theValue) + this->mTilePlaceHolder1X + this->mTilePlaceHolder1Y + this->mIntPlaceHolder1 + this->mIntPlaceHolder2
                 + this->mTargetUnitID + this->mOriginatingUnitID + this->mPlayerNumber + (this->mBoolPlaceHolder1 == true)
                 + (this->mBoolPlaceHolder2 == true) + (this->mInitialized == true) + (this->mHasTargetLocation == true)
                 + (this->mHasTargetUnit == true);
    theValue = this->mFloatPlaceHolder1;
    theChecksum += *reinterpret_cast<long*>(&theValue);
    theValue = this->mFloatPlaceHolder2;
    theChecksum += *reinterpret_cast<long*>(&theValue);
    theValue = this->mHeight;
    theChecksum += *reinterpret_cast<long*>(&theValue);
    theValue = this->mDiscreteUpdateTime;
    theChecksum += *reinterpret_cast<long*>(&theValue);
    theValue = this->mTimeElapsedSinceLastUpdate;
    theChecksum += *reinterpret_cast<long*>(&theValue);
    return theChecksum;
}

// FUNCTION: EXE 0x4ad5bb
// Returns how many whole discrete update periods have elapsed and keeps the remainder
unsigned long EECalamity::GetDiscreteUpdates()
{
    unsigned long theUpdates = 0;
    if (this->mDiscreteUpdateTime != 0.0f) {
        theUpdates = UMFloatToLong(this->mTimeElapsedSinceLastUpdate / this->mDiscreteUpdateTime);
        this->mTimeElapsedSinceLastUpdate -= theUpdates * this->mDiscreteUpdateTime;
    }
    return theUpdates;
}

// FUNCTION: EXE 0x4ad5fc
// Returns the target unit through the argument (NULL if none) and whether there is one
bool EECalamity::GetTargetUnit(EEUnit*& outUnit)
{
    bool theResult = false;
    if (this->mTargetUnit != NULL) {
        outUnit = this->mTargetUnit;
        theResult = true;
    } else
        outUnit = NULL;
    return theResult;
}

// FUNCTION: EXE 0x4ad618
// Reads the calamity from a saved game (the class by id; target and caster only as ids); returns the first read error
long EECalamity::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    long theResult;
    theResult = this->mRectangle.Load(inFile);
    if (theResult == kErrorOK) {
        theResult = this->mTargetLocation.Load(inFile);
        if (theResult == kErrorOK) {
            theResult = this->mShakeLocation.Load(inFile);
            if (theResult == kErrorOK) {
                theResult = FSLoadVector(inFile, this->mEffectGeneration, EECalamity::mEffectGenerationStringID);
                if (theResult == kErrorOK) {
                    theResult = inFile.Read(this->mTargetUnitID);
                    if (theResult == kErrorOK) {
                        {
                            long theClassID;
                            theResult = inFile.Read(theClassID);
                            this->mClass = EECalamityManager::GetInstance()->GetCalamityClass(theClassID);
                        }
                        if (theResult == kErrorOK) {
                            theResult = inFile.Read(this->mEpicenterFactor);
                            if (theResult == kErrorOK) {
                                theResult = inFile.Read(this->mFloatPlaceHolder1);
                                if (theResult == kErrorOK) {
                                    theResult = inFile.Read(this->mFloatPlaceHolder2);
                                    if (theResult == kErrorOK) {
                                        theResult = inFile.Read(this->mHeight);
                                        if (theResult == kErrorOK) {
                                            theResult = inFile.Read(this->mIntPlaceHolder1);
                                            if (theResult == kErrorOK) {
                                                theResult = inFile.Read(this->mIntPlaceHolder2);
                                                if (theResult == kErrorOK) {
                                                    theResult = inFile.Read(this->mTimeAlive);
                                                    if (theResult == kErrorOK) {
                                                        theResult = inFile.Read(this->mOriginatingUnitID);
                                                        if (theResult == kErrorOK) {
                                                            theResult = inFile.Read(this->mPlayerNumber);
                                                            if (theResult == kErrorOK) {
                                                                theResult = inFile.Read(this->mDiscreteUpdateTime);
                                                                if (theResult == kErrorOK) {
                                                                    theResult = inFile.Read(this->mTimeElapsedSinceLastUpdate);
                                                                    if (theResult == kErrorOK) {
                                                                        {
                                                                            long theTileX;
                                                                            theResult = inFile.Read(theTileX);
                                                                            this->mTilePlaceHolder1X = theTileX;
                                                                        }
                                                                        if (theResult == kErrorOK) {
                                                                            {
                                                                                long theTileY;
                                                                                theResult = inFile.Read(theTileY);
                                                                                this->mTilePlaceHolder1Y = theTileY;
                                                                            }
                                                                            if (theResult == kErrorOK) {
                                                                                theResult = inFile.Read(this->mEffectHandle.mValue);
                                                                                if (theResult == kErrorOK) {
                                                                                    theResult = inFile.Read(this->mBoolPlaceHolder1);
                                                                                    if (theResult == kErrorOK) {
                                                                                        theResult = inFile.Read(this->mBoolPlaceHolder2);
                                                                                        if (theResult == kErrorOK) {
                                                                                            theResult = inFile.Read(this->mInitialized);
                                                                                            if (theResult == kErrorOK) {
                                                                                                theResult = inFile.Read(this->mHasTargetLocation);
                                                                                                if (theResult == kErrorOK) {
                                                                                                    theResult = inFile.Read(this->mHasTargetUnit);
                                                                                                }
                                                                                            }
                                                                                        }
                                                                                    }
                                                                                }
                                                                            }
                                                                        }
                                                                    }
                                                                }
                                                            }
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x4ad85e
// Writes the calamity to the saved game (target and caster as ids, -1 if none); returns the first write error
long EECalamity::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    unsigned long theUnitID = -1;
    long theResult = this->mRectangle.Save(outFile);
    if (theResult != kErrorOK)
        return theResult;
    theResult = this->mTargetLocation.Save(outFile);
    if (theResult != kErrorOK)
        return theResult;
    theResult = this->mShakeLocation.Save(outFile);
    if (theResult != kErrorOK)
        return theResult;
    theResult = FSSaveVector(outFile, this->mEffectGeneration, EECalamity::mEffectGenerationStringID);
    if (theResult != kErrorOK)
        return theResult;
    theUnitID = this->mTargetUnit != NULL ? this->mTargetUnit->GetUnitID() : -1;
    theResult = outFile.Write(theUnitID);
    if (theResult != kErrorOK)
        return theResult;
    theResult = outFile.Write((unsigned long)this->mClass->mID);
    if (theResult != kErrorOK)
        return theResult;
    theResult = outFile.Write(this->mEpicenterFactor);
    if (theResult != kErrorOK)
        return theResult;
    theResult = outFile.Write(this->mFloatPlaceHolder1);
    if (theResult != kErrorOK)
        return theResult;
    theResult = outFile.Write(this->mFloatPlaceHolder2);
    if (theResult != kErrorOK)
        return theResult;
    theResult = outFile.Write(this->mHeight);
    if (theResult != kErrorOK)
        return theResult;
    theResult = outFile.Write(this->mIntPlaceHolder1);
    if (theResult != kErrorOK)
        return theResult;
    theResult = outFile.Write(this->mIntPlaceHolder2);
    if (theResult != kErrorOK)
        return theResult;
    theResult = outFile.Write(this->mTimeAlive);
    if (theResult != kErrorOK)
        return theResult;
    theUnitID = this->mCasterUnit != NULL ? this->mCasterUnit->GetUnitID() : -1;
    theResult = outFile.Write(theUnitID);
    if (theResult != kErrorOK)
        return theResult;
    theResult = outFile.Write(this->mPlayerNumber);
    if (theResult != kErrorOK)
        return theResult;
    theResult = outFile.Write(this->mDiscreteUpdateTime);
    if (theResult != kErrorOK)
        return theResult;
    theResult = outFile.Write(this->mTimeElapsedSinceLastUpdate);
    if (theResult != kErrorOK)
        return theResult;
    theResult = outFile.Write((long)this->mTilePlaceHolder1X);
    if (theResult != kErrorOK)
        return theResult;
    theResult = outFile.Write((long)this->mTilePlaceHolder1Y);
    if (theResult != kErrorOK)
        return theResult;
    theResult = outFile.Write(this->mEffectHandle.mValue);
    if (theResult != kErrorOK)
        return theResult;
    theResult = outFile.Write(this->mBoolPlaceHolder1);
    if (theResult != kErrorOK)
        return theResult;
    theResult = outFile.Write(this->mBoolPlaceHolder2);
    if (theResult != kErrorOK)
        return theResult;
    theResult = outFile.Write(this->mInitialized);
    if (theResult != kErrorOK)
        return theResult;
    theResult = outFile.Write(this->mHasTargetLocation);
    if (theResult != kErrorOK)
        return theResult;
    theResult = outFile.Write(this->mHasTargetUnit);
    return theResult;
}

// FUNCTION: EXE 0x4adaa3
// Plays the calamity's sound through its class
void EECalamity::PlaySound()
{
    if (this->mClass != NULL)
        this->mClass->PlaySound(this);
}

// FUNCTION: EXE 0x4adab3
// Sets the caster unit, moving the reference and the originating unit id to it (-1 when cleared)
void EECalamity::SetCasterUnit(EEUnit* inUnit)
{
    if (this->mCasterUnit != NULL) {
        this->mCasterUnit->Dereference();
        this->mOriginatingUnitID = -1;
    }
    this->mCasterUnit = inUnit;
    if (inUnit != NULL) {
        this->mCasterUnit->Reference();
        this->mOriginatingUnitID = this->mCasterUnit->mUnitID;
    }
}

// FUNCTION: EXE 0x4adae7
// Stops the current main sound and keeps a referenced new one
void EECalamity::SetSound(SESMInstanceNode* inSound)
{
    this->StopSound();
    this->mSound = inSound;
    if (inSound != NULL)
        inSound->Reference();
}

// FUNCTION: EXE 0x4adb01
// Stops the current secondary sound and keeps a referenced new one
void EECalamity::SetSecondarySound(SESMInstanceNode* inSound)
{
    this->StopSecondarySound();
    this->mSecondarySound = inSound;
    if (inSound != NULL)
        inSound->Reference();
}

// FUNCTION: EXE 0x4adb1b
// Sets the target unit, moving the reference and the target id to it, and whether the calamity has a target unit
void EECalamity::SetTargetUnit(EEUnit* inUnit)
{
    if (this->mTargetUnit != NULL) {
        this->mTargetUnit->Dereference();
        this->mTargetUnitID = -1;
    }
    this->mTargetUnit = inUnit;
    if (inUnit != NULL) {
        this->mTargetUnit->Reference();
        this->mTargetUnitID = this->mTargetUnit->mUnitID;
    }
    if (this->mTargetUnit != NULL)
        this->mHasTargetUnit = true;
    else
        this->mHasTargetUnit = false;
}

// Compiler-generated for this unit (template COMDATs; names in analysis/exe/names.txt):
// TEMPLATE: EXE 0x4adb65
// TEMPLATE: EXE 0x4adb9f
// TEMPLATE: EXE 0x4adc3d

