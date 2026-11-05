// C:\EEX\Empire Earth\Data Pre-Loading Thread\EEGraphicObject.cpp (name from RTTI EEGraphicObject; no __FILE__).
// 0xd72c1..0xd9b2b: the graphics of the database (EEGraphicObject : EEMedia). The media table's init chain
// (gMediaTable, 0x4e8448) and EEDatabase.h's lock chain (0x4e8430) first, then the class (ctor, ??_G, dtor, the
// orbital/turret part scan, LoadTable, the table cleanup and dump statics, the record loader, the tag-point and model
// lookups, the texture helper, the colour setter), then the deferred COMDATs (vector/map members, EEOrbital's ??_G,
// EETurretPart's ctor). EEGraphicEffect.cpp ends at 0xd72c1, EEMusicObject.cpp starts at 0xd9b2b. Unit flags: the
// exe's module set.
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <vector>

#include "lle/LowLevelEngine.h"
#include "lle/FSFileSpec.h"
#include "lle/TSCriticalSection.h"
#include "lle/TSScheduler.h"

#include "ee/EEMedia.h"
#include "EEDatabase.h"
#include "lle/GERenderState.h"

std::vector<EEMedia*> gMediaTable;          // 0x4e8448 (EEMedia::Get/GetCount read it)

#include "lle/UString.h"
#include "lle/GEColor.h"
#include "lle/GEMaterial.h"
#include "lle/GEModel.h"
#include "lle/GEModelManager.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GESurface.h"
#include "lle/GETextureManager.h"
#include "lle/GETexture.h"
#include "ee/EEPhysics.h"
#include "ee/EmpireEarth.h"
#include "ee/EEGraphicObject.h"
#include "ee/EEWheel.h"
#include "ee/EESoundObject.h"
#include "ee/EEGlobalConstants.h"

extern unsigned long& gTexturePixelDepth;     // 0x5308b4
extern unsigned long& gTextureMipLevelsToDrop; // 0x530894 (provisional name)
extern bool& gTrilinearFilter;

// 0x43e584: the tag-point kind names, one per graphic-effect kind (0x48 of 0x40 chars; the last two empty). .rdata
// after EEGraphicEffect.cpp's vtables, so this unit defines it; initializer from the original bytes
extern const char gTagPointKindNames[0x48][0x40];
const char gTagPointKindNames[0x48][0x40] = {
    "damage_trail", "2ddebris", "invalid", "exhaust_trail",
    "2dexplosion", "2dfire", "impact", "impact",
    "invalid", "weapon_mount", "invalid", "damage_trail",
    "water_splash", "light", "invalid", "invalid",
    "invalid", "invalid", "blast_wave", "invalid",
    "invalid", "invalid", "winter_breath", "invalid",
    "2dexplosion", "3ddebris", "invalid", "exhaust_trail",
    "3dexplosion", "3dfire", "weapon_mount", "weapon_mount",
    "terrain_deformation", "weapon_mount", "water_ripple", "invalid",
    "invalid", "invalid", "invalid", "invalid",
    "invalid", "invalid", "invalid", "invalid",
    "invalid", "invalid", "invalid", "invalid",
    "invalid", "invalid", "invalid", "invalid",
    "invalid", "invalid", "invalid", "end_water_ripple",
    "end_2dfire", "invalid", "invalid", "invalid",
    "invalid", "invalid", "invalid", "invalid",
    "invalid", "invalid", "invalid", "invalid",
    "invalid", "invalid", "", ""
};

// 0x4e8434..0x4e8440: four built-in graphic objects outside gMediaTable; LoadTable clears them, EEEvent EEUnloadBuiltInGraphicObjects
// 0x1ca6d9 unloads them and deletes those with mMediaIndex -1 (provisional names)
EEGraphicObject* gGraphicObjectU4E8434;
EEGraphicObject* gGraphicObjectU4E8438;
EEGraphicObject* gGraphicObjectU4E843C;
EEGraphicObject* gGraphicObjectU4E8440;

// One record of the graphics table (0x2cc bytes, read whole; LoadTable passes it to the loader 0xd7b37). Field names
// by offset: the loader's copies give the types (the per-state animation blocks are packed: each state's closing
// flag shares a dword with the next state's first flag). +0x00..+0x68 is not read by the loader
struct EEGraphicObjectRecord {
    char mLabel[100];                       // +0x000 the row name (the game never reads it; the text's label)
    int32_t mUnusedDatabaseID;              // +0x064 the database row id (not read)
    int32_t mMediaIndex;      // +0x68
    bool mIsDrawn;            // +0x6c
    int32_t mUnusedBuildingValue1; // +0x70
    int32_t mUnusedBuildingValue2; // +0x74
    bool mSharesFrames0;      // +0x78
    uint32_t mFirstFrame0;       // +0x7c
    uint32_t mLastFrame0;       // +0x80
    uint32_t mFrameTime0;       // +0x84
    bool mLoops0;             // +0x88
    uint32_t mUnusedHoldTime0;      // +0x8c (not read: the loader sets mCamouflageTimer[0] to 300000)
    uint32_t mSound1Frame0;       // +0x90
    uint32_t mSound2Frame0;       // +0x94
    uint32_t mSound1Index0;       // +0x98
    uint32_t mSound2Index0;       // +0x9c
    bool mIsSpeedScaled0;     // +0xa0
    bool mSharesFrames13;     // +0xa1
    uint32_t mFirstFrame13;       // +0xa4
    uint32_t mLastFrame13;       // +0xa8
    uint32_t mFrameTime13;       // +0xac
    bool mLoops13;            // +0xb0
    uint32_t mHoldTime13;       // +0xb4
    uint32_t mSound1Frame13;       // +0xb8
    uint32_t mSound2Frame13;       // +0xbc
    uint32_t mSound1Index13;       // +0xc0
    uint32_t mSound2Index13;       // +0xc4
    bool mIsSpeedScaled13;    // +0xc8
    bool mSharesFrames7;      // +0xc9
    uint32_t mFirstFrame7;       // +0xcc
    uint32_t mLastFrame7;       // +0xd0
    uint32_t mFrameTime7;       // +0xd4
    bool mLoops7;             // +0xd8
    uint32_t mHoldTime7;       // +0xdc
    uint32_t mSound1Frame7;       // +0xe0
    uint32_t mSound2Frame7;       // +0xe4
    uint32_t mSound1Index7;       // +0xe8
    uint32_t mSound2Index7;       // +0xec
    bool mIsSpeedScaled7;     // +0xf0
    bool mSharesFrames8;      // +0xf1
    uint32_t mFirstFrame8;       // +0xf4
    uint32_t mLastFrame8;       // +0xf8
    uint32_t mFrameTime8;       // +0xfc
    bool mLoops8;             // +0x100
    uint32_t mHoldTime8;       // +0x104
    uint32_t mSound1Frame8;       // +0x108
    uint32_t mSound2Frame8;       // +0x10c
    uint32_t mSound1Index8;       // +0x110
    uint32_t mSound2Index8;       // +0x114
    bool mIsSpeedScaled8;     // +0x118
    bool mSharesFrames4;      // +0x119
    uint32_t mFirstFrame4;       // +0x11c
    uint32_t mLastFrame4;       // +0x120
    uint32_t mFrameTime4;       // +0x124
    bool mLoops4;             // +0x128
    uint32_t mHoldTime4;       // +0x12c
    bool mIsSpeedScaled4;     // +0x130
    bool mSharesFrames5;      // +0x131
    uint32_t mFirstFrame5;       // +0x134
    uint32_t mLastFrame5;       // +0x138
    uint32_t mFrameTime5;       // +0x13c
    bool mLoops5;             // +0x140
    uint32_t mHoldTime5;       // +0x144
    bool mIsSpeedScaled5;     // +0x148
    bool mSharesFrames12;     // +0x149
    uint32_t mFirstFrame12;       // +0x14c
    uint32_t mLastFrame12;       // +0x150
    uint32_t mFrameTime12;       // +0x154
    bool mLoops12;            // +0x158
    uint32_t mHoldTime12;       // +0x15c
    uint32_t mSound1Frame12;       // +0x160
    uint32_t mSound2Frame12;       // +0x164
    uint32_t mSound1Index12;       // +0x168
    uint32_t mSound2Index12;       // +0x16c
    bool mIsSpeedScaled12;    // +0x170
    bool mSharesFrames11;     // +0x171
    uint32_t mFirstFrame11;       // +0x174
    uint32_t mLastFrame11;       // +0x178
    uint32_t mFrameTime11;       // +0x17c
    bool mLoops11;            // +0x180
    uint32_t mHoldTime11;       // +0x184
    uint32_t mSound1Frame11;       // +0x188
    uint32_t mSound2Frame11;       // +0x18c
    uint32_t mSound1Index11;       // +0x190
    uint32_t mSound2Index11;       // +0x194
    bool mIsSpeedScaled11;    // +0x198
    bool mSharesFrames9;      // +0x199
    uint32_t mFirstFrame9;       // +0x19c
    uint32_t mLastFrame9;       // +0x1a0
    uint32_t mFrameTime9;       // +0x1a4
    bool mLoops9;             // +0x1a8
    uint32_t mHoldTime9;       // +0x1ac
    uint32_t mSound1Frame9;       // +0x1b0
    uint32_t mSound2Frame9;       // +0x1b4
    uint32_t mSound1Index9;       // +0x1b8
    uint32_t mSound2Index9;       // +0x1bc
    bool mIsSpeedScaled9;     // +0x1c0
    bool mSharesFrames10;     // +0x1c1
    uint32_t mFirstFrame10;       // +0x1c4
    uint32_t mLastFrame10;       // +0x1c8
    uint32_t mFrameTime10;       // +0x1cc
    bool mLoops10;            // +0x1d0
    uint32_t mHoldTime10;       // +0x1d4
    uint32_t mSound1Frame10;       // +0x1d8
    uint32_t mSound2Frame10;       // +0x1dc
    uint32_t mSound1Index10;       // +0x1e0
    uint32_t mSound2Index10;       // +0x1e4
    bool mIsSpeedScaled10;    // +0x1e8
    bool mSharesFrames6;      // +0x1e9
    uint32_t mFirstFrame6;       // +0x1ec
    uint32_t mLastFrame6;       // +0x1f0
    uint32_t mFrameTime6;       // +0x1f4
    bool mLoops6;             // +0x1f8
    uint32_t mHoldTime6;       // +0x1fc
    uint32_t mSound1Frame6;       // +0x200
    uint32_t mSound2Frame6;       // +0x204
    uint32_t mSound1Index6;       // +0x208
    uint32_t mSound2Index6;       // +0x20c
    bool mIsSpeedScaled6;     // +0x210
    bool mSharesFrames3;      // +0x211
    uint32_t mFirstFrame3;       // +0x214
    uint32_t mLastFrame3;       // +0x218
    uint32_t mFrameTime3;       // +0x21c
    bool mLoops3;             // +0x220
    uint32_t mHoldTime3;       // +0x224
    uint32_t mSound1Frame3;       // +0x228
    uint32_t mSound2Frame3;       // +0x22c
    uint32_t mSound1Index3;       // +0x230
    uint32_t mSound2Index3;       // +0x234
    bool mIsSpeedScaled3;     // +0x238
    bool mSharesFrames1;      // +0x239
    uint32_t mFirstFrame1;       // +0x23c
    uint32_t mLastFrame1;       // +0x240
    uint32_t mFrameTime1;       // +0x244
    bool mLoops1;             // +0x248
    uint32_t mHoldTime1;       // +0x24c
    uint32_t mSound1Frame1;       // +0x250
    uint32_t mSound2Frame1;       // +0x254
    uint32_t mSound1Index1;       // +0x258
    uint32_t mSound2Index1;       // +0x25c
    bool mIsSpeedScaled1;     // +0x260
    bool mSharesFrames2;      // +0x261
    uint32_t mFirstFrame2;       // +0x264
    uint32_t mLastFrame2;       // +0x268
    uint32_t mFrameTime2;       // +0x26c
    bool mLoops2;             // +0x270
    uint32_t mHoldTime2;       // +0x274
    uint32_t mSound1Frame2;       // +0x278
    uint32_t mSound2Frame2;       // +0x27c
    uint32_t mSound1Index2;       // +0x280
    uint32_t mSound2Index2;       // +0x284
    bool mIsSpeedScaled2;     // +0x288
    int32_t mUnusedIsBuilding; // +0x28c
    bool mUnusedIsSprite;     // +0x290
    char _gap291[0x7];
    int32_t mUnusedDummyValue; // +0x298
    int32_t mUnusedBaseGraphic; // +0x29c
    int32_t mAlphaReference;  // +0x2a0
    int32_t mUnusedBuildingIndex; // +0x2a4
    char _gap2A8[0x4];
    int32_t mKind;            // +0x2ac
    bool mHasAlpha;           // +0x2b0
    float mOpacity;           // +0x2b4
    char _gap2B8[0x4];
    float mMipMapBias;        // +0x2bc
    int32_t mNumMipLevelsToDrop; // +0x2c0
    int32_t mMaxMipLevelsToDrop; // +0x2c4
    bool mCreateMipMaps;      // +0x2c8
};
static_assert(sizeof(EEGraphicObjectRecord) == 716, "on-disk record: 716 bytes on every target");

void EELoadGraphicObjectRecord(EEGraphicObject* inObject, EEGraphicObjectRecord* inRecord);  // 0xd7b37 (cdecl)

// FUNCTION: EXE 0x4d7354
// Creates an empty, unloaded graphic object: no names, models, textures or sounds, drawn, mip maps on
EEGraphicObject::EEGraphicObject()
{
    for (unsigned long i = 0; i < 14; i++) {
        this->mTextures[i] = NULL;
        this->mModels[i] = NULL;
        this->mFrameCounts[i] = 0;
        this->mSounds1[i] = NULL;
        this->mSounds2[i] = NULL;
        this->mIsSpeedScaled[i] = false;
        this->mHoldTimes[i] = 0;
        this->mStartFrames[i] = 0;
        this->mLastFrames[i] = 0;
        this->mFrameTimes[i] = 0;
        this->mSound1Frames[i] = 0;
        this->mSound2Frames[i] = 0;
        this->mLoops[i] = false;
        this->mSharesFrames[i] = false;
        this->mTagPoints[i] = NULL;
    }
    this->mFileName = NULL;
    this->mTextureName = NULL;
    this->mShadowModelName = NULL;
    this->mShadowTextureName = NULL;
    for (unsigned long j = 0; j < 14; j++) {
        this->mModelNames[j] = NULL;
        this->mTextureNames[j] = NULL;
    }
    this->mMediaIndex = -1;
    this->mKind = -1;
    this->mMipMapBias = 0.0f;
    this->mModel = NULL;
    this->mAlphaReference = 0.0f;
    this->mTexture = NULL;
    this->mShadowModel = NULL;
    this->mShadowTexture = NULL;
    this->mL318 = 0;
    this->mL31C = 0;
    this->mL320 = 0;
    this->mL4B0 = 0;
    this->mL4B4 = 0;
    this->mMediaCount = 0;
    this->mOpacity = 0.01f;
    this->mMaxMipLevelsToDrop = 2;
    this->mNumMipLevelsToDrop = 0;
    this->mB4CE = false;
    this->mIsDrawn = true;
    this->mHasAlpha = false;
    this->mCreateMipMaps = true;
    this->mIsLoading = false;
}

// FUNCTION: EXE 0x4d754c
// Destroys the graphic object and frees its memory when asked (scalar deleting destructor)
// Compiler-generated: the ??_G of the virtual dtor below

// FUNCTION: EXE 0x4d7568
// Destroys the graphic object: unloads its media if loaded, frees its file names and colour groups
EEGraphicObject::~EEGraphicObject()
{
    if (this->mMediaCount > 0) {
        this->mMediaCount = 1;
        this->UnloadMedia();
    }
    delete this->mFileName;
    delete this->mTextureName;
    delete this->mShadowModelName;
    delete this->mShadowTextureName;
    for (unsigned long i = 0; i < 14; i++) {
        delete this->mModelNames[i];
        delete this->mTextureNames[i];
    }
    for (unsigned long thePart = 0; thePart < 14; thePart++)
        for (unsigned long j = 0; j < this->mColorGroups[thePart].size(); j++)
            delete this->mColorGroups[thePart][j];
}

// FUNCTION: EXE 0x4d76a1
// Collects the model's turret parts, orbitals and wheels, recursing into its sub-models
// The part scan of inModel under inParent: a "turret_" model becomes a turret part at its transformed origin, an
// orbiting model an EEOrbital, a wheel model an EEWheel; then the sub-models under the combined transformation
void EEGraphicObject::CollectParts(GEModel* inModel, const GETransformation& inParent)
{
    GE3DPoint theOrigin;
    GE3DPoint thePosition;
    GEVector theUp(0.0f, 1.0f, 0.0f);
    GEVector theDirection;
    if (inModel == NULL)
        return;
    GETransformation theTransformation(false);
    inParent.Apply(inModel->GetInternalTransform(0), theTransformation);
    theTransformation.Apply(theUp, theDirection);
    if (strncmp(inModel->GetName(), "turret_", strlen("turret_")) == 0) {
        theTransformation.Apply(theOrigin, thePosition);
        this->mTurretParts.push_back(new EETurretPart(thePosition, theTransformation, inModel));
    } else if (EEOrbital::IsOrbitalModel(inModel)) {
        this->mOrbitals.push_back(new EEOrbital(inModel));
    } else if (EEIsWheelModel(inModel)) {
        this->mWheels.push_back(new EEWheel(inModel));
    }
    for (unsigned long i = 0; i < inModel->GetSubModelCount(); i++)
        this->CollectParts(inModel->GetSubModel(i), theTransformation);
}

// FUNCTION: EXE 0x4d7833
// Reads the graphics table into the media table (names, then the record); entries with media index 0 are
// dropped. Returns the last read result
long EEGraphicObject::LoadTable(FSFile* inFile)
{
    UString theName;
    unsigned long theCount = 0;
    unsigned long theRead;
    EEGraphicObjectRecord theRecord;
    inFile->Open(kFileAccessRead);
    gGraphicObjectU4E8440 = NULL;
    gGraphicObjectU4E843C = NULL;
    gGraphicObjectU4E8438 = NULL;
    gGraphicObjectU4E8434 = NULL;
    long theResult = inFile->Read(theCount);
    if (theResult == kErrorOK) {
        gMediaTable.resize(theCount, NULL);
        for (unsigned long i = 0; i < theCount; i++) {
            EEGraphicObject* theObject = new EEGraphicObject;
            gMediaTable[i] = theObject;
            theName.Load(*inFile);
            theName.Load(*inFile);
            theObject->mFileName = new UString((const char*)theName);
            theName.Load(*inFile);
            theObject->mTextureName = new UString((const char*)theName);
            for (unsigned long j = 0; j < 14; j++) {
                theName.Load(*inFile);
                theObject->mTextureNames[j] = new UString((const char*)theName);
                theName.Load(*inFile);
                theObject->mModelNames[j] = new UString((const char*)theName);
            }
            theName.Load(*inFile);
            theObject->mShadowModelName = new UString((const char*)theName);
            theName.Load(*inFile);
            theObject->mShadowTextureName = new UString((const char*)theName);
            theResult = inFile->Read(reinterpret_cast<char*>(&theRecord), sizeof(theRecord), theRead);
            EELoadGraphicObjectRecord(theObject, &theRecord);
            if (theObject->mMediaIndex == 0) {
                delete theObject;
                gMediaTable[i] = NULL;
            }
        }
    }
    inFile->Close();
    return theResult;
}

// FUNCTION: EXE 0x4d7a8f
// Deletes every entry of the media table and empties it
void EEDeleteMediaTable()
{
    for (unsigned long i = 0; i < gMediaTable.size(); i++)
        delete gMediaTable[i];
    gMediaTable.resize(0);
}

// FUNCTION: EXE 0x4d7acf
// Writes to the file each graphic object that is still loaded, with its file name and use count
void EEDumpGraphicObjectTable(FSFileSpec& inFile)
{
    fprintf(inFile, "\n\n**********Graphic Object Table*********\n\n");
    for (unsigned long i = 0; i < gMediaTable.size(); i++) {
        EEGraphicObject* theObject = static_cast<EEGraphicObject*>(gMediaTable[i]);
        if (theObject != NULL && theObject->mMediaCount > 0)
            fprintf(inFile, "\t%s (filename) has a media count of %ld\n",(const char*)*theObject->mFileName,
                    theObject->mMediaCount);
    }
}

// FUNCTION: EXE 0x4d7b37
// Copies a graphics table record into the graphic object: animation states, sounds, alpha and mip settings
// The record loader: the record's fields into the object, the sounds by id (an unset sound clears its index to -1),
// then the frame counts from the frame ranges
void EELoadGraphicObjectRecord(EEGraphicObject* inObject, EEGraphicObjectRecord* inRecord)
{
    if (inObject != NULL) {
        inObject->mMediaIndex = inRecord->mMediaIndex;
        inObject->mKind = inRecord->mKind;
        inObject->mIsDrawn = inRecord->mIsDrawn;
        inObject->mL318 = inRecord->mUnusedBuildingValue1;
        inObject->mL31C = inRecord->mUnusedBuildingValue2;
        inObject->mStartFrames[0] = inRecord->mFirstFrame0;
        inObject->mLastFrames[0] = inRecord->mLastFrame0;
        inObject->mFrameTimes[0] = inRecord->mFrameTime0;
        inObject->mLoops[0] = inRecord->mLoops0;
        inObject->mSharesFrames[0] = inRecord->mSharesFrames0;
        inObject->mHoldTimes[0] = 300000;
        inObject->mSounds1[0] = EESoundObject::Get(inRecord->mSound1Index0);
        if (inObject->mSounds1[0] != NULL)
            inObject->mSound1Frames[0] = inRecord->mSound1Frame0;
        else
            inObject->mSound1Frames[0] = -1;
        inObject->mSounds2[0] = EESoundObject::Get(inRecord->mSound2Index0);
        if (inObject->mSounds2[0] != NULL)
            inObject->mSound2Frames[0] = inRecord->mSound2Frame0;
        else
            inObject->mSound2Frames[0] = -1;
        inObject->mIsSpeedScaled[0] = inRecord->mIsSpeedScaled0;
        inObject->mStartFrames[13] = inRecord->mFirstFrame13;
        inObject->mLastFrames[13] = inRecord->mLastFrame13;
        inObject->mFrameTimes[13] = inRecord->mFrameTime13;
        inObject->mLoops[13] = inRecord->mLoops13;
        inObject->mSharesFrames[13] = inRecord->mSharesFrames13;
        inObject->mHoldTimes[13] = inRecord->mHoldTime13;
        inObject->mSounds1[13] = EESoundObject::Get(inRecord->mSound1Index13);
        if (inObject->mSounds1[13] != NULL)
            inObject->mSound1Frames[13] = inRecord->mSound1Frame13;
        else
            inObject->mSound1Frames[13] = -1;
        inObject->mSounds2[13] = EESoundObject::Get(inRecord->mSound2Index13);
        if (inObject->mSounds2[13] != NULL)
            inObject->mSound2Frames[13] = inRecord->mSound2Frame13;
        else
            inObject->mSound2Frames[13] = -1;
        inObject->mIsSpeedScaled[13] = inRecord->mIsSpeedScaled13;
        inObject->mStartFrames[7] = inRecord->mFirstFrame7;
        inObject->mLastFrames[7] = inRecord->mLastFrame7;
        inObject->mFrameTimes[7] = inRecord->mFrameTime7;
        inObject->mLoops[7] = inRecord->mLoops7;
        inObject->mSharesFrames[7] = inRecord->mSharesFrames7;
        inObject->mHoldTimes[7] = inRecord->mHoldTime7;
        inObject->mSounds1[7] = EESoundObject::Get(inRecord->mSound1Index7);
        if (inObject->mSounds1[7] != NULL)
            inObject->mSound1Frames[7] = inRecord->mSound1Frame7;
        else
            inObject->mSound1Frames[7] = -1;
        inObject->mSounds2[7] = EESoundObject::Get(inRecord->mSound2Index7);
        if (inObject->mSounds2[7] != NULL)
            inObject->mSound2Frames[7] = inRecord->mSound2Frame7;
        else
            inObject->mSound2Frames[7] = -1;
        inObject->mIsSpeedScaled[7] = inRecord->mIsSpeedScaled7;
        inObject->mStartFrames[8] = inRecord->mFirstFrame8;
        inObject->mLastFrames[8] = inRecord->mLastFrame8;
        inObject->mFrameTimes[8] = inRecord->mFrameTime8;
        inObject->mLoops[8] = inRecord->mLoops8;
        inObject->mSharesFrames[8] = inRecord->mSharesFrames8;
        inObject->mHoldTimes[8] = inRecord->mHoldTime8;
        inObject->mSounds1[8] = EESoundObject::Get(inRecord->mSound1Index8);
        if (inObject->mSounds1[8] != NULL)
            inObject->mSound1Frames[8] = inRecord->mSound1Frame8;
        else
            inObject->mSound1Frames[8] = -1;
        inObject->mSounds2[8] = EESoundObject::Get(inRecord->mSound2Index8);
        if (inObject->mSounds2[8] != NULL)
            inObject->mSound2Frames[8] = inRecord->mSound2Frame8;
        else
            inObject->mSound2Frames[8] = -1;
        inObject->mIsSpeedScaled[8] = inRecord->mIsSpeedScaled8;
        inObject->mStartFrames[4] = inRecord->mFirstFrame4;
        inObject->mLastFrames[4] = inRecord->mLastFrame4;
        inObject->mFrameTimes[4] = inRecord->mFrameTime4;
        inObject->mLoops[4] = inRecord->mLoops4;
        inObject->mSharesFrames[4] = inRecord->mSharesFrames4;
        inObject->mHoldTimes[4] = inRecord->mHoldTime4;
        inObject->mSounds1[4] = NULL;
        inObject->mSound1Frames[4] = -1;
        inObject->mSounds2[4] = NULL;
        inObject->mSound2Frames[4] = -1;
        inObject->mIsSpeedScaled[4] = inRecord->mIsSpeedScaled4;
        inObject->mStartFrames[5] = inRecord->mFirstFrame5;
        inObject->mLastFrames[5] = inRecord->mLastFrame5;
        inObject->mFrameTimes[5] = inRecord->mFrameTime5;
        inObject->mLoops[5] = inRecord->mLoops5;
        inObject->mSharesFrames[5] = inRecord->mSharesFrames5;
        inObject->mHoldTimes[5] = inRecord->mHoldTime5;
        inObject->mSounds1[5] = NULL;
        inObject->mSound1Frames[5] = -1;
        inObject->mSounds2[5] = NULL;
        inObject->mSound2Frames[5] = -1;
        inObject->mIsSpeedScaled[5] = inRecord->mIsSpeedScaled5;
        inObject->mStartFrames[12] = inRecord->mFirstFrame12;
        inObject->mLastFrames[12] = inRecord->mLastFrame12;
        inObject->mFrameTimes[12] = inRecord->mFrameTime12;
        inObject->mLoops[12] = inRecord->mLoops12;
        inObject->mSharesFrames[12] = inRecord->mSharesFrames12;
        inObject->mHoldTimes[12] = inRecord->mHoldTime12;
        inObject->mSounds1[12] = EESoundObject::Get(inRecord->mSound1Index12);
        if (inObject->mSounds1[12] != NULL)
            inObject->mSound1Frames[12] = inRecord->mSound1Frame12;
        else
            inObject->mSound1Frames[12] = -1;
        inObject->mSounds2[12] = EESoundObject::Get(inRecord->mSound2Index12);
        if (inObject->mSounds2[12] != NULL)
            inObject->mSound2Frames[12] = inRecord->mSound2Frame12;
        else
            inObject->mSound2Frames[12] = -1;
        inObject->mIsSpeedScaled[12] = inRecord->mIsSpeedScaled12;
        inObject->mStartFrames[11] = inRecord->mFirstFrame11;
        inObject->mLastFrames[11] = inRecord->mLastFrame11;
        inObject->mFrameTimes[11] = inRecord->mFrameTime11;
        inObject->mLoops[11] = inRecord->mLoops11;
        inObject->mSharesFrames[11] = inRecord->mSharesFrames11;
        inObject->mHoldTimes[11] = inRecord->mHoldTime11;
        inObject->mSounds1[11] = EESoundObject::Get(inRecord->mSound1Index11);
        if (inObject->mSounds1[11] != NULL)
            inObject->mSound1Frames[11] = inRecord->mSound1Frame11;
        else
            inObject->mSound1Frames[11] = -1;
        inObject->mSounds2[11] = EESoundObject::Get(inRecord->mSound2Index11);
        if (inObject->mSounds2[11] != NULL)
            inObject->mSound2Frames[11] = inRecord->mSound2Frame11;
        else
            inObject->mSound2Frames[11] = -1;
        inObject->mIsSpeedScaled[11] = inRecord->mIsSpeedScaled11;
        inObject->mStartFrames[9] = inRecord->mFirstFrame9;
        inObject->mLastFrames[9] = inRecord->mLastFrame9;
        inObject->mFrameTimes[9] = inRecord->mFrameTime9;
        inObject->mLoops[9] = inRecord->mLoops9;
        inObject->mSharesFrames[9] = inRecord->mSharesFrames9;
        inObject->mHoldTimes[9] = inRecord->mHoldTime9;
        inObject->mSounds1[9] = EESoundObject::Get(inRecord->mSound1Index9);
        if (inObject->mSounds1[9] != NULL)
            inObject->mSound1Frames[9] = inRecord->mSound1Frame9;
        else
            inObject->mSound1Frames[9] = -1;
        inObject->mSounds2[9] = EESoundObject::Get(inRecord->mSound2Index9);
        if (inObject->mSounds2[9] != NULL)
            inObject->mSound2Frames[9] = inRecord->mSound2Frame9;
        else
            inObject->mSound2Frames[9] = -1;
        inObject->mIsSpeedScaled[9] = inRecord->mIsSpeedScaled9;
        inObject->mStartFrames[10] = inRecord->mFirstFrame10;
        inObject->mLastFrames[10] = inRecord->mLastFrame10;
        inObject->mFrameTimes[10] = inRecord->mFrameTime10;
        inObject->mLoops[10] = inRecord->mLoops10;
        inObject->mSharesFrames[10] = inRecord->mSharesFrames10;
        inObject->mHoldTimes[10] = inRecord->mHoldTime10;
        inObject->mSounds1[10] = EESoundObject::Get(inRecord->mSound1Index10);
        if (inObject->mSounds1[10] != NULL)
            inObject->mSound1Frames[10] = inRecord->mSound1Frame10;
        else
            inObject->mSound1Frames[10] = -1;
        inObject->mSounds2[10] = EESoundObject::Get(inRecord->mSound2Index10);
        if (inObject->mSounds2[10] != NULL)
            inObject->mSound2Frames[10] = inRecord->mSound2Frame10;
        else
            inObject->mSound2Frames[10] = -1;
        inObject->mIsSpeedScaled[10] = inRecord->mIsSpeedScaled10;
        inObject->mStartFrames[6] = inRecord->mFirstFrame6;
        inObject->mLastFrames[6] = inRecord->mLastFrame6;
        inObject->mFrameTimes[6] = inRecord->mFrameTime6;
        inObject->mLoops[6] = inRecord->mLoops6;
        inObject->mSharesFrames[6] = inRecord->mSharesFrames6;
        inObject->mHoldTimes[6] = inRecord->mHoldTime6;
        inObject->mSounds1[6] = EESoundObject::Get(inRecord->mSound1Index6);
        if (inObject->mSounds1[6] != NULL)
            inObject->mSound1Frames[6] = inRecord->mSound1Frame6;
        else
            inObject->mSound1Frames[6] = -1;
        inObject->mSounds2[6] = EESoundObject::Get(inRecord->mSound2Index6);
        if (inObject->mSounds2[6] != NULL)
            inObject->mSound2Frames[6] = inRecord->mSound2Frame6;
        else
            inObject->mSound2Frames[6] = -1;
        inObject->mIsSpeedScaled[6] = inRecord->mIsSpeedScaled6;
        inObject->mStartFrames[3] = inRecord->mFirstFrame3;
        inObject->mLastFrames[3] = inRecord->mLastFrame3;
        inObject->mFrameTimes[3] = inRecord->mFrameTime3;
        inObject->mLoops[3] = inRecord->mLoops3;
        inObject->mSharesFrames[3] = inRecord->mSharesFrames3;
        inObject->mHoldTimes[3] = inRecord->mHoldTime3;
        inObject->mSounds1[3] = EESoundObject::Get(inRecord->mSound1Index3);
        if (inObject->mSounds1[3] != NULL)
            inObject->mSound1Frames[3] = inRecord->mSound1Frame3;
        else
            inObject->mSound1Frames[3] = -1;
        inObject->mSounds2[3] = EESoundObject::Get(inRecord->mSound2Index3);
        if (inObject->mSounds2[3] != NULL)
            inObject->mSound2Frames[3] = inRecord->mSound2Frame3;
        else
            inObject->mSound2Frames[3] = -1;
        inObject->mIsSpeedScaled[3] = inRecord->mIsSpeedScaled3;
        inObject->mStartFrames[1] = inRecord->mFirstFrame1;
        inObject->mLastFrames[1] = inRecord->mLastFrame1;
        inObject->mFrameTimes[1] = inRecord->mFrameTime1;
        inObject->mLoops[1] = inRecord->mLoops1;
        inObject->mSharesFrames[1] = inRecord->mSharesFrames1;
        inObject->mHoldTimes[1] = inRecord->mHoldTime1;
        inObject->mSounds1[1] = EESoundObject::Get(inRecord->mSound1Index1);
        if (inObject->mSounds1[1] != NULL)
            inObject->mSound1Frames[1] = inRecord->mSound1Frame1;
        else
            inObject->mSound1Frames[1] = -1;
        inObject->mSounds2[1] = EESoundObject::Get(inRecord->mSound2Index1);
        if (inObject->mSounds2[1] != NULL)
            inObject->mSound2Frames[1] = inRecord->mSound2Frame1;
        else
            inObject->mSound2Frames[1] = -1;
        inObject->mIsSpeedScaled[1] = inRecord->mIsSpeedScaled1;
        inObject->mStartFrames[2] = inRecord->mFirstFrame2;
        inObject->mLastFrames[2] = inRecord->mLastFrame2;
        inObject->mFrameTimes[2] = inRecord->mFrameTime2;
        inObject->mLoops[2] = inRecord->mLoops2;
        inObject->mSharesFrames[2] = inRecord->mSharesFrames2;
        inObject->mHoldTimes[2] = inRecord->mHoldTime2;
        inObject->mSounds1[2] = EESoundObject::Get(inRecord->mSound1Index2);
        if (inObject->mSounds1[2] != NULL)
            inObject->mSound1Frames[2] = inRecord->mSound1Frame2;
        else
            inObject->mSound1Frames[2] = -1;
        inObject->mSounds2[2] = EESoundObject::Get(inRecord->mSound2Index2);
        if (inObject->mSounds2[2] != NULL)
            inObject->mSound2Frames[2] = inRecord->mSound2Frame2;
        else
            inObject->mSound2Frames[2] = -1;
        inObject->mIsSpeedScaled[2] = inRecord->mIsSpeedScaled2;
        inObject->mL4B4 = inRecord->mUnusedIsBuilding;
        inObject->mB4CE = inRecord->mUnusedIsSprite;
        inObject->mL4B0 = inRecord->mUnusedDummyValue;
        inObject->mL2FC = inRecord->mUnusedBaseGraphic;
        inObject->mL320 = inRecord->mUnusedBuildingIndex;
        inObject->mHasAlpha = inRecord->mHasAlpha;
        inObject->mMipMapBias = inRecord->mMipMapBias;
        inObject->mOpacity = inRecord->mOpacity;
        inObject->mAlphaReference = inRecord->mAlphaReference * kHeightEpsilon;
        inObject->mNumMipLevelsToDrop = inRecord->mNumMipLevelsToDrop;
        inObject->mCreateMipMaps = inRecord->mCreateMipMaps;
        inObject->mMaxMipLevelsToDrop = inRecord->mMaxMipLevelsToDrop;
        for (unsigned long i = 0; i < 14; i++)
            inObject->mFrameCounts[i] = inObject->mLastFrames[i] - inObject->mStartFrames[i] + 1;
    }
}


// FUNCTION: EXE 0x4d84ec
// Returns the tag points of a kind on a part's model, or NULL when it has none
EETagPointList* EEGraphicObject::GetTagPoints(unsigned long inPart, long inKind)
{
    EETagPointList* theResult = NULL;
    std::map<long, std::map<const GEModel*, EETagPointList*>*>::iterator theKind = this->mTagPointMaps.find(inKind);
    if (theKind != this->mTagPointMaps.end()) {
        std::map<const GEModel*, EETagPointList*>* theLists = theKind->second;
        const GEModel* theModel = this->mModels[inPart];
        std::map<const GEModel*, EETagPointList*>::iterator theList = theLists->find(theModel);
        if (theList != theLists->end())
            theResult = theList->second;
    }
    return theResult;
}

// FUNCTION: EXE 0x4d8546
// Returns one of the attack points, picked by the game time, or the default point when there are none
const GE3DPoint& EEGraphicObject::PickPoint()
{
    if (this->mPoints.size() == 0)
        return this->mDefaultPoint;
    long theTime = gPhysics->mGameTime;
    return this->mPoints[theTime % this->mPoints.size()];
}

// FUNCTION: EXE 0x4d85aa
// Finds the turret part for a model and returns its index; returns 0 when found, else 3
long EEGraphicObject::FindTurretPart(GEModel* inModel, unsigned long& outIndex)
{
    long theResult = 3;
    unsigned long theCount = this->mTurretParts.size();
    for (unsigned long i = 0; i < theCount; i++) {
        if (this->mTurretParts[i]->mModel == inModel) {
            outIndex = i;
            theResult = 0;
            break;
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x4d85ed
// Loads the object's models, textures, sounds and parts on first use (counting later uses); returns 9 if a
// model failed
// The load (vtable 0x43f788 +0x04): on the first use, under the database lock, the frame ranges of each animated part
// into a per-model texture-frame map (the model names numbered in first-seen order; 14: no model), then the shared
// model, the shadow model and texture, each part's texture and model (and its part scan), the sounds, and the part
// scan of the shared model; a later use waits until the first load is done
long EEGraphicObject::LoadMedia()
{
    GETransformation theTransformation;
    GETextureDescription theShadowDescription;
    theShadowDescription.mPixelDepth = gTexturePixelDepth;
    theShadowDescription.mNumMipLevelsToDrop = 0;
    theShadowDescription.mSurfaceVolatility = 1;
    theShadowDescription.mFrameFormat = 0;
    theShadowDescription.mIs3DRenderable = false;
    theShadowDescription.mIsTransparencyEnabled = false;
    theShadowDescription.mHasAlpha = true;
    theShadowDescription.mCreateMIPMaps = true;
    GETextureDescription theDescription;
    theDescription.mPixelDepth = gTexturePixelDepth;
    theDescription.mNumMipLevelsToDrop = 0;
    theDescription.mSurfaceVolatility = 1;
    theDescription.mFrameFormat = 0;
    theDescription.mIs3DRenderable = false;
    theDescription.mIsTransparencyEnabled = false;
    theDescription.mHasAlpha = this->mHasAlpha;
    theDescription.mCreateMIPMaps = true;
    std::map<unsigned long, long> theFrames[15];
    std::map<unsigned long, long> theNoFrames;
    std::map<UString, long> theModelIndices;
    long theResult = 0;
    long theNextIndex = 0;
    sDatabaseLock.mLock->Enter();
    if (this->mMediaCount == 0) {
        this->mMediaCount = 1;
        this->mIsLoading = true;
        sDatabaseLock.mLock->Leave();
        for (unsigned long i = 0; i < 14; i++) {
            if (this->mFrameCounts[i] > 4 && this->mSharesFrames[i]) {
                long theIndex;
                if (this->mModelNames[i] != NULL && this->mModelNames[i]->GetLength() > 0) {
                    std::map<UString, long>::iterator theFound = theModelIndices.find(*this->mModelNames[i]);
                    if (theFound == theModelIndices.end()) {
                        theModelIndices.insert(std::make_pair(*this->mModelNames[i], theNextIndex));
                        theIndex = theNextIndex++;
                    } else
                        theIndex = theFound->second;
                } else
                    theIndex = 14;
                unsigned long theFrame;
                if (this->mFrameCounts[i] % 2 == 0) {
                    for (theFrame = this->mStartFrames[i]; theFrame <= this->mLastFrames[i]; theFrame++) {
                        if ((theFrame - this->mStartFrames[i]) % 2 == 1) {
                            if (theFrame != this->mLastFrames[i])
                                theFrames[theIndex].insert(std::map<unsigned long, long>::value_type(theFrame, -1));
                            else
                                theFrames[theIndex].insert(std::map<unsigned long, long>::value_type(theFrame - 1, 1));
                        }
                    }
                } else {
                    for (theFrame = this->mStartFrames[i]; theFrame <= this->mLastFrames[i]; theFrame++) {
                        if ((theFrame - this->mStartFrames[i]) % 2 == 1)
                            theFrames[theIndex].insert(std::map<unsigned long, long>::value_type(theFrame, -1));
                    }
                }
            }
        }
        if (this->mModel == NULL && this->mFileName != NULL && this->mFileName->GetLength() > 0) {
            this->mModel = gModelManager->LoadModel(*this->mFileName, theFrames[14]);
            if (this->mModel != NULL)
                this->mModel->Reference();
        }
        if (this->mShadowModel == NULL && this->mShadowModelName != NULL && this->mShadowModelName->GetLength() > 0) {
            this->mShadowModel = gModelManager->LoadModel(*this->mShadowModelName, theNoFrames);
            if (this->mShadowModel != NULL)
                this->mShadowModel->Reference();
            else
                theResult = 9;
        }
        theDescription.mCreateMIPMaps = this->mCreateMipMaps;
        theDescription.mNumMipLevelsToDrop = this->mNumMipLevelsToDrop + gTextureMipLevelsToDrop;
        if (theDescription.mNumMipLevelsToDrop >= this->mMaxMipLevelsToDrop)
            theDescription.mNumMipLevelsToDrop = this->mMaxMipLevelsToDrop;
        theShadowDescription.mNumMipLevelsToDrop = 0;
        theShadowDescription.mCreateMIPMaps = true;
        if (this->mTextureName != NULL)
            this->LoadTexture(*this->mTextureName, this->mTexture, theDescription, this->mIsDrawn);
        if (this->mShadowTextureName != NULL)
            this->LoadTexture(*this->mShadowTextureName, this->mShadowTexture, theShadowDescription, this->mIsDrawn);
        unsigned long thePart;
        for (thePart = 0; thePart < 14; thePart++) {
            if (this->mTextureNames[thePart] != NULL && this->mTextureNames[thePart]->GetLength() > 0)
                this->LoadTexture(*this->mTextureNames[thePart], this->mTextures[thePart], theDescription, this->mIsDrawn);
            else if (this->mTexture != NULL) {
                this->mTexture->Reference();
                this->mTextures[thePart] = this->mTexture;
            }
            if (this->mModelNames[thePart] != NULL && this->mModelNames[thePart]->GetLength() > 0) {
                if (this->mFrameCounts[thePart] > 4 && this->mSharesFrames[thePart])
                    this->mModels[thePart] = gModelManager->LoadModel(*this->mModelNames[thePart],
                        theFrames[theModelIndices.find(*this->mModelNames[thePart])->second]);
                else
                    this->mModels[thePart] = gModelManager->LoadModel(*this->mModelNames[thePart], theNoFrames);
                if (this->mModels[thePart] != NULL)
                    this->mModels[thePart]->Reference();
                else
                    theResult = 9;
            } else if (this->mModel != NULL) {
                this->mModel->Reference();
                this->mModels[thePart] = this->mModel;
            } else
                this->mModels[thePart] = NULL;
            if (this->mModels[thePart] != NULL)
                this->CollectTagPoints(thePart, this->mModels[thePart], theTransformation);
        }
        for (thePart = 0; thePart < 14; thePart++) {
            if (this->mSounds1[thePart] != NULL)
                this->mSounds1[thePart]->LoadMedia();
            if (this->mSounds2[thePart] != NULL)
                this->mSounds2[thePart]->LoadMedia();
        }
        if (this->mModel != NULL)
            this->CollectParts(this->mModel, theTransformation);
        sDatabaseLock.mLock->Enter();
        this->mIsLoading = false;
    } else {
        this->mMediaCount++;
        sDatabaseLock.mLock->Leave();
        while (this->mIsLoading == true)
            PlatSleep(1);
        sDatabaseLock.mLock->Enter();
    }
    sDatabaseLock.mLock->Leave();
    return theResult;
}

// FUNCTION: EXE 0x4d8b09
// Loads a texture by name into the slot if it is empty, with modulate, wrap and the object's mip settings
void EEGraphicObject::LoadTexture(const UString& inName, GETexture*& ioTexture, const GETextureDescription& inDescription,
                              bool inUnused)
{
    if (ioTexture == NULL && inName.GetLength() > 0) {
        ioTexture = gTextureManager->LoadTexture(inName, inDescription);
        if (ioTexture != NULL) {
            ioTexture->Reference();
            ioTexture->SetStageAlphaMode(kStageOpModulate, -1);
            ioTexture->SetStageColorMode(kStageOpModulate, -1);
            ioTexture->SetStageMinFilterMode(1, -1);
            ioTexture->SetStageMagFilterMode(1, -1);
            ioTexture->SetStageMipMapBias(this->mMipMapBias, -1);
            ioTexture->SetStageTextureAddressingMode(kAddressingWrap, -1);
            if (gTrilinearFilter) ioTexture->SetStageMipFilterMode(kMipFilterLinear, -1); else ioTexture->SetStageMipFilterMode(kMipFilterPoint, -1);
        }
    }
}

// FUNCTION: EXE 0x4d8bb2
// Collects a part model's tag points, attack and status bar points and player-colour materials, recursively
// The tag points and colour groups of model part inPart's model inModel (recursive over the sub-models): each tag point
// named for a point kind goes into the kind's list for the root model (not for the sub-models of part 0's model when
// inPart > 0), "weapon_mount" into the part's list, "attack" gives the default point (and the top point at the frame
// mLastFrames[5]), "status bar" the bar offset (otherwise the top of the bounding box raises it); the materials named
// "player color" form the part's colour group
void EEGraphicObject::CollectTagPoints(unsigned long inPart, GEModel* inModel, const GETransformation& inParent)
{
    GETransformation theTransformation;
    GE3DPoint theTop;
    GE3DPoint theBarPoint;
    bool theFound = false;
    unsigned long theSubModelCount = inModel->GetSubModelCount();
    unsigned long theTagPointCount = inModel->GetTagPointCount();
    inParent.Apply(inModel->GetInternalTransform(0), theTransformation);
    GEModel* theRoot = inModel;
    while (theRoot->GetParentModel() != NULL)
        theRoot = theRoot->GetParentModel();
    unsigned long i;
    for (i = 0; i < theTagPointCount; i++) {
        if (theRoot != this->mModels[0] || inPart == 0) {
            for (long theKind = 0; (unsigned long)theKind < 0x48; theKind++) {
                size_t theLength = strlen(gTagPointKindNames[theKind]);
                if (theLength > 0
                    && strncmp(inModel->GetTagPointName(i), gTagPointKindNames[theKind], theLength) == 0) {
                    std::map<const GEModel*, EETagPointList*>* theLists;
                    std::map<long, std::map<const GEModel*, EETagPointList*>*>::iterator theKindFound =
                        this->mTagPointMaps.find(theKind);
                    if (theKindFound == this->mTagPointMaps.end()) {
                        theLists = new std::map<const GEModel*, EETagPointList*>;
                        this->mTagPointMaps.insert(
                            std::map<long, std::map<const GEModel*, EETagPointList*>*>::value_type(theKind, theLists));
                    } else
                        theLists = theKindFound->second;
                    EETagPointList* theList;
                    std::map<const GEModel*, EETagPointList*>::iterator theListFound = theLists->find(theRoot);
                    if (theListFound == theLists->end()) {
                        theList = new EETagPointList;
                        theLists->insert(std::map<const GEModel*, EETagPointList*>::value_type(theRoot, theList));
                    } else
                        theList = theListFound->second;
                    theList->Add(inModel, i);
                }
            }
        }
        if (strncmp(inModel->GetTagPointName(i), "weapon_mount", strlen("weapon_mount")) == 0) {
            if (this->mTagPoints[inPart] == NULL)
                this->mTagPoints[inPart] = new EETagPointList;
            this->mTagPoints[inPart]->Add(inModel, i);
        }
        if (strncmp(inModel->GetTagPointName(i), "attack", strlen("attack")) == 0) {
            theTransformation.Apply(inModel->GetTagPoint(i, 0), this->mDefaultPoint);
            this->mPoints.push_back(this->mDefaultPoint);
            if (this->mLastFrames[5] < inModel->GetFrameCount())
                theTransformation.Apply(inModel->GetTagPoint(i, this->mLastFrames[5]), this->mTopPoint);
        }
        if (strncmp(inModel->GetTagPointName(i), "status bar", strlen("status bar")) == 0) {
            theTransformation.Apply(inModel->GetTagPoint(i, 0), this->mBar);
            theFound = true;
        }
    }
    if (!theFound) {
        theTop.SetZ(inModel->GetBoundingBox(0).GetTopRightBack().GetZ());
        theTransformation.Apply(theTop, theBarPoint);
        this->mBar.SetZ(theBarPoint.GetZ() < this->mBar.GetZ() ? this->mBar.GetZ() : theBarPoint.GetZ());
    }
    bool theHasGroup = false;
    for (long theMaterial = 0; (unsigned long)theMaterial < inModel->GetMaterialCount(); theMaterial++) {
        if (inModel->GetMaterial(theMaterial)->GetName() == "player color") {
            if (!theHasGroup) {
                theHasGroup = true;
                EEColorGroup* theGroup = new EEColorGroup;
                theGroup->mModel = inModel;
                this->mColorGroups[inPart].push_back(theGroup);
            }
            this->mColorGroups[inPart][this->mColorGroups[inPart].size() - 1]->mMaterials.push_back(theMaterial);
        }
    }
    for (i = 0; i < theSubModelCount; i++)
        this->CollectTagPoints(inPart, inModel->GetSubModel(i), theTransformation);
}

// FUNCTION: EXE 0x4d9008
// Sets the colour of every player-colour material of a part
void EEGraphicObject::SetGroupColor(const GEColor& inColor, unsigned long inPart)
{
    for (unsigned long i = 0; i < this->mColorGroups[inPart].size(); i++) {
        EEColorGroup* theGroup = this->mColorGroups[inPart][i];
        for (unsigned long j = 0; j < theGroup->mMaterials.size(); j++)
            theGroup->mModel->GetMaterial(theGroup->mMaterials[j])->SetColor(inColor);
    }
}

// FUNCTION: EXE 0x4d907e
// Counts one use down; on the last use releases all the object's models, textures, sounds and parts.
// Returns 0
// The unload: waits while the object is being loaded (mIsLoading), then under the database lock, on the last use, releases
// the shadow model, each part's texture, tag points and model, the tag-point maps, the models, textures and sounds,
// and deletes the orbitals, turret parts, wheels and colour groups; an earlier use only counts down
long EEGraphicObject::UnloadMedia()
{
    std::map<long, std::map<const GEModel*, EETagPointList*>*>::iterator theKind;
    std::map<const GEModel*, EETagPointList*>::iterator theModel;
    while (this->mIsLoading == true)
        PlatSleep(1);
    sDatabaseLock.mLock->Enter();
    if (this->mMediaCount == 1) {
        this->mMediaCount = 0;
        if (this->mShadowModel != NULL) {
            this->mShadowModel->Dereference();
            this->mShadowModel = NULL;
        }
        unsigned long i;
        for (i = 0; i < 14; i++) {
            if (this->mTextures[i] != NULL) {
                this->mTextures[i]->Dereference();
                this->mTextures[i] = NULL;
            }
            if (this->mTagPoints[i] != NULL) {
                this->mTagPoints[i]->Clear();
                delete this->mTagPoints[i];
                this->mTagPoints[i] = NULL;
            }
            if (this->mModels[i] != NULL) {
                this->mModels[i]->Dereference();
                this->mModels[i] = NULL;
            }
        }
        for (theKind = this->mTagPointMaps.begin(); theKind != this->mTagPointMaps.end(); theKind++) {
            std::map<const GEModel*, EETagPointList*>* theLists = theKind->second;
            for (theModel = theLists->begin(); theModel != theLists->end(); theModel++) {
                theModel->second->Clear();
                delete theModel->second;
            }
            theLists->clear();
            delete theKind->second;
        }
        this->mTagPointMaps.clear();
        if (this->mModel != NULL) {
            this->mModel->Dereference();
            this->mModel = NULL;
        }
        if (this->mTexture != NULL) {
            this->mTexture->Dereference();
            this->mTexture = NULL;
        }
        if (this->mShadowTexture != NULL) {
            this->mShadowTexture->Dereference();
            this->mShadowTexture = NULL;
        }
        for (i = 0; i < 14; i++) {
            if (this->mSounds1[i] != NULL)
                this->mSounds1[i]->UnloadMedia();
            if (this->mSounds2[i] != NULL)
                this->mSounds2[i]->UnloadMedia();
        }
        for (i = 0; i < this->mOrbitals.size(); i++)
            delete this->mOrbitals[i];
        this->mOrbitals.clear();
        for (i = 0; i < this->mTurretParts.size(); i++)
            delete this->mTurretParts[i];
        this->mTurretParts.clear();
        for (i = 0; i < this->mWheels.size(); i++)
            delete this->mWheels[i];
        this->mWheels.clear();
        for (unsigned long thePart = 0; thePart < 14; thePart++) {
            for (i = 0; i < this->mColorGroups[thePart].size(); i++)
                delete this->mColorGroups[thePart][i];
            this->mColorGroups[thePart].clear();
        }
    } else if (this->mMediaCount > 1) {
        this->mMediaCount--;
    }
    sDatabaseLock.mLock->Leave();
    return 0;
}

// TEMPLATE: EXE 0x4d93de
// (std::vector<EETurretPart*>::push_back, a template COMDAT emitted by this unit)

// TEMPLATE: EXE 0x4d940a
// (std::vector<EEOrbital*>::push_back, a template COMDAT emitted by this unit)

// TEMPLATE: EXE 0x4d9436
// (std::vector<EEWheel*>::push_back, a template COMDAT emitted by this unit)

// TEMPLATE: EXE 0x4d948e
// (the tag-point kind map's _Tree destructor, a template COMDAT emitted by this unit)

// TEMPLATE: EXE 0x4d95af
// (the tag-point kind map's _Tree::erase(first, last), a template COMDAT emitted by this unit)


// TEMPLATE: EXE 0x4d9988
// (std::map<long, long>'s _Tree::_Lbound, a template COMDAT emitted by this unit)

// TEMPLATE: EXE 0x4d99b1
// (the tag-point kind map's _Tree::_Erase, a template COMDAT emitted by this unit)



// FUNCTION: EXE 0x4d9a5d
// Creates a tag point referring to tag point inIndex of the model, holding a reference to the model
inline EETagPoint::EETagPoint(GEModel* inModel, unsigned long inIndex)
{
    this->mModel = inModel;
    inModel->Reference();
    this->mIndex = inIndex;
}

inline void EETagPointList::Add(GEModel* inModel, unsigned long inIndex)
{
    this->mPoints.push_back(new EETagPoint(inModel, inIndex));
}

// TEMPLATE: EXE 0x4d9a97
// (compiler-generated ??_GEEOrbital@@UAEPAXI@Z; name in analysis/exe/names.txt)

// TEMPLATE: EXE 0x4d9ab4
// (EETurretPart's inline ctor, emitted for CollectParts)

// The init chains (gMediaTable; the second global):
// SYNTHETIC: EXE 0x4d72c1
// SYNTHETIC: EXE 0x4d72cb
// SYNTHETIC: EXE 0x4d72e3
// SYNTHETIC: EXE 0x4d72ef
// SYNTHETIC: EXE 0x4d730a
// SYNTHETIC: EXE 0x4d7314
// SYNTHETIC: EXE 0x4d733a
// SYNTHETIC: EXE 0x4d7346

// The map/vector COMDATs of the tag-point tables (template tail)
// TEMPLATE: EXE 0x4d9358
// TEMPLATE: EXE 0x4d939f
// TEMPLATE: EXE 0x4d93ca
// TEMPLATE: EXE 0x4d9462
// TEMPLATE: EXE 0x4d94b9
// TEMPLATE: EXE 0x4d94cd
// TEMPLATE: EXE 0x4d953e
// TEMPLATE: EXE 0x4d9620
// TEMPLATE: EXE 0x4d98d7
// TEMPLATE: EXE 0x4d9912
// TEMPLATE: EXE 0x4d9956
// TEMPLATE: EXE 0x4d99e3
// TEMPLATE: EXE 0x4d9a25
// TEMPLATE: EXE 0x4d9aff
