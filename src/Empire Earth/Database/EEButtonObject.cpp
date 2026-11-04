// C:\EEX\Empire Earth\Database\EEButtonObject.cpp (name from RTTI EEButtonObject; no __FILE__). 0xd5405..0xd5d08:
// the class static table's init chain, the header lock's chain (Database\EEDatabase.h), ctor, ??_G, dtor, Load
// (db\dbButtons.dat), Unload, DumpAllLoadedMedia, Acquire (reference + load on demand), LoadTexture, two drawing
// helpers, Release (dereference), Reload, the two virtual thunks. EEAnimal.cpp ends at 0xd5405, the globals-chain
// unit (EEColorTable?) starts at 0xd5d08. Unit flags: the exe's module set.

#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <vector>

#include "lle/LowLevelEngine.h"
#include "lle/TSScheduler.h"
#include "lle/FSFileSpec.h"
#include "lle/Graphics Engine.h"
#include "lle/GEColor.h"
#include "lle/GESurface.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GETextureManager.h"
#include "lle/GETexture.h"
#include "lle/GEScreen.h"
#include "lle/GEViewport.h"
#include "lle/GEVertexArray.h"
#include "lle/GERasterizer.h"
#include "lle/GEBitmap.h"

#include "ee/EmpireEarth.h"
#include "ee/EEButtonObject.h"

std::vector<EEButtonObject*> EEButtonObject::sTable;

#include <EEDatabase.h>

// 0x5308b4: the pixel depth textures are loaded with (16 or 32; provisional name)
extern unsigned long gTexturePixelDepth;

// One record of db\dbButtons.dat (0xe0 bytes, read whole). Inferred layout.
struct EEButtonRecord {
    char _gap00[0x64];
    char mFileName[0x68];                   // +0x64 -> mName
    long mID;                               // +0xcc -> +0x30
    long mRow;                              // +0xd0 -> +0x2c
    long mColumn;                           // +0xd4 -> +0x28
    long mPosition;                         // +0xd8 -> +0x20
    long mHotKey;                           // +0xdc -> +0x24
};

// FUNCTION: EXE 0x4d5498
// Creates a button image entry with no texture and no references
EEButtonObject::EEButtonObject()
    : mTexture(NULL), mCellU(0.0f), mCellV(0.0f), mMediaCount(0)
{
    this->mIsLoading = false;
}

// SYNTHETIC: EXE 0x4d54c3
// (the scalar deleting destructor)

// FUNCTION: EXE 0x4d54df
// Destroys the button image entry, releasing its texture
EEButtonObject::~EEButtonObject()
{
    if (this->mTexture != NULL)
        this->mTexture->Dereference();
}

// FUNCTION: EXE 0x4d5500
// Loads the button image entries (file name, cell row and column, id, hot key, position) from db\dbButtons.dat;
// stops at the first read error and returns it
long EEButtonObject::Load(FSFileSpec* inFile)
{
    unsigned long theCount = 0;
    long theResult;
    unsigned long theRead = 0;
    EEButtonRecord theRecord;
    inFile->Open(kFileAccessRead);
    theResult = inFile->Read(theCount);
    if (theResult == kErrorOK) {
        EEButtonObject::sTable.resize(theCount, NULL);
        for (unsigned long i = 0; i < theCount; i++) {
            EEButtonObject* theButton = new EEButtonObject;
            EEButtonObject::sTable[i] = theButton;
            theResult = inFile->Read(reinterpret_cast<char*>(&theRecord), sizeof(theRecord), theRead);
            theButton->mColumn = theRecord.mColumn;
            theButton->mRow = theRecord.mRow;
            theButton->mID = theRecord.mID;
            theButton->mName.Copy(theRecord.mFileName, 0, strlen(theRecord.mFileName), 0, true);
            theButton->mHotKeyIndex = theRecord.mHotKey;
            theButton->mPosition = theRecord.mPosition;
            if (theResult != kErrorOK)
                break;
        }
    }
    inFile->Close();
    return theResult;
}

// FUNCTION: EXE 0x4d55f1
// Destroys every button image entry and empties the table
void EEButtonObject::Unload()
{
    for (unsigned long i = 0; i < EEButtonObject::sTable.size(); i++)
        delete EEButtonObject::sTable[i];
    EEButtonObject::sTable.resize(0);
}

// FUNCTION: EXE 0x4d5631
// Writes each button image with a non-zero reference count to a media dump file
void EEButtonObject::DumpAllLoadedMedia(FSFileSpec& inFile)
{
    fprintf(inFile, "\n\n**********Button Database Table*********\n\n");
    for (unsigned long i = 0; i < EEButtonObject::sTable.size(); i++)
        if (EEButtonObject::sTable[i] != NULL && EEButtonObject::sTable[i]->mMediaCount > 0)
            fprintf(inFile, "\t%s (filename) has a media count of %d\n", (const char*)EEButtonObject::sTable[i]->mName,
                    EEButtonObject::sTable[i]->mMediaCount);
}

// FUNCTION: EXE 0x4d5690
// Adds a reference to the button image, loading its texture on the first one (later callers wait for that load);
// returns the load error
long EEButtonObject::Acquire()
{
    long theResult = 0;
    sDatabaseLock.mLock->Enter();
    if (this->mMediaCount == 0) {
        this->mMediaCount = 1;
        this->mIsLoading = true;
        sDatabaseLock.mLock->Leave();
        theResult = this->LoadTexture();
        if (theResult != 0) {
            sDatabaseLock.mLock->Enter();
            this->mMediaCount = 0;
            sDatabaseLock.mLock->Leave();
        }
        sDatabaseLock.mLock->Enter();
        this->mIsLoading = false;
    } else {
        this->mMediaCount++;
        sDatabaseLock.mLock->Leave();
        while (this->mIsLoading == true)
            Sleep(1);
        sDatabaseLock.mLock->Enter();
    }
    sDatabaseLock.mLock->Leave();
    return theResult;
}

// FUNCTION: EXE 0x4d572e
// Loads the button's texture (a mip level dropped at 800 pixels wide or less, alpha for ids below 50) and computes
// its cell's texture offset; returns 3 when the texture is missing
long EEButtonObject::LoadTexture()
{
    GETextureDescription theDescription;
    theDescription.mPixelDepth = gTexturePixelDepth;
    theDescription.mNumMipLevelsToDrop = 0;
    theDescription.mSurfaceVolatility = 1;
    theDescription.mFrameFormat = 0;
    theDescription.mIs3DRenderable = false;
    theDescription.mIsTransparencyEnabled = false;
    theDescription.mHasAlpha = true;
    theDescription.mCreateMIPMaps = false;
    long theResult = kErrorOK;
    theDescription.mNumMipLevelsToDrop = gRasterizer->GetPrimarySurface(gScreenIndex)->GetWidth() > 800 ? 0 : 1;
    theDescription.mHasAlpha = this->mID < 50;
    this->mTexture = gTextureManager->LoadTexture(this->mName, theDescription);
    if (this->mTexture != NULL) {
        this->mTexture->Reference();
        this->mTexture->SetStageMinFilterMode(0, -1);
        this->mTexture->SetStageMagFilterMode(0, -1);
        this->mTexture->SetStageMipFilterMode(kMipFilterNone, -1);
        this->mTexture->SetStageMipMapBias(0.0f, -1);
        this->mCellU = (float)(this->mColumn * 41) / (float)(this->mTexture->GetWidth() << this->mTexture->GetNumMipLevelsToDrop());
        this->mCellV = (float)(this->mRow * 41) / (float)(this->mTexture->GetHeight() << this->mTexture->GetNumMipLevelsToDrop());
    } else {
        theResult = 3;
    }
    return theResult;
}

// FUNCTION: EXE 0x4d584e
// Shows the button's cell, inset by one texel, on a bitmap scaled to a rectangle (a 40-texel cell on textures 256 or
// more wide, else the whole texture); a button without a texture is skipped
void EEButtonObject::SetFaceBitmap(GEBitmap* inBitmap, const GERectangle& inRectangle, unsigned long inScreen)
{
    GETextureCoordinate theTopLeft;
    GETextureCoordinate theTopRight;
    GETextureCoordinate theBottomLeft;
    GETextureCoordinate theBottomRight;
    // Test the texture before reading its size (a button whose texture failed to load is skipped).
    if (this->mTexture == NULL)
        return;
    float theWidth = (float)(this->mTexture->GetWidth() << this->mTexture->GetNumMipLevelsToDrop());
    unsigned long theHeight = this->mTexture->GetSurface()->GetHeight();
    unsigned long theDrop = this->mTexture->GetNumMipLevelsToDrop();
    inBitmap->SetTexture(this->mTexture, inScreen);
    float theU;
    float theV;
    if (theWidth >= 256.0f) {
        theU = 40.0f / theWidth;
        theV = 40.0f / (float)(theHeight << theDrop);
    } else {
        theU = (float)(this->mTexture->GetWidth() - 1) / (float)this->mTexture->GetWidth();
        theV = (float)(this->mTexture->GetHeight() - 1) / (float)this->mTexture->GetHeight();
    }
    float theTexel = 1.0f / (float)this->mTexture->GetWidth();
    theTopLeft.SetU(theTexel + this->mCellU);
    theTopLeft.SetV(theTexel + this->mCellV);
    theBottomRight.SetU(theTexel + this->mCellU + theU);
    theBottomRight.SetV(theTexel + this->mCellV + theV);
    theTopRight.SetU(theBottomRight.GetU());
    theTopRight.SetV(theTopLeft.GetV());
    theBottomLeft.SetU(theTopLeft.GetU());
    theBottomLeft.SetV(theBottomRight.GetV());
    inBitmap->SetTextureCoordinates(theTopLeft, theTopRight, theBottomLeft, theBottomRight);
    unsigned long theTextureHeight = this->mTexture->GetSurface()->GetHeight();
    unsigned long theTextureWidth = this->mTexture->GetWidth();
    inBitmap->SetXYScale((inRectangle.GetWidth() + 1.0f) / theTextureWidth,
                         (inRectangle.GetHeight() + 1.0f) / theTextureHeight);
}

// FUNCTION: EXE 0x4d5a2e
// Shows the pressed button's cell (no texel inset) on a bitmap scaled to a rectangle; as SetFaceBitmap without the
// texture test
// NEAR: the original converts (theHeight << theDrop) through fild qword + fdivp; ours fuses it into fidiv
void EEButtonObject::SetPressedBitmap(GEBitmap* inBitmap, const GERectangle& inRectangle, unsigned long inScreen)
{
    GETextureCoordinate theTopLeft;
    GETextureCoordinate theTopRight;
    GETextureCoordinate theBottomLeft;
    GETextureCoordinate theBottomRight;
    float theWidth = (float)(this->mTexture->GetWidth() << this->mTexture->GetNumMipLevelsToDrop());
    unsigned long theHeight = this->mTexture->GetSurface()->GetHeight();
    unsigned long theDrop = this->mTexture->GetNumMipLevelsToDrop();
    inBitmap->SetTexture(this->mTexture, inScreen);
    float theU;
    float theV;
    if (theWidth >= 256.0f) {
        theU = 40.0f / theWidth;
        theV = 40.0f / (float)(theHeight << theDrop);
    } else {
        theU = (float)(this->mTexture->GetWidth() - 1) / (float)this->mTexture->GetWidth();
        theV = (float)(this->mTexture->GetHeight() - 1) / (float)this->mTexture->GetHeight();
    }
    theTopLeft.SetU(this->mCellU);
    theTopLeft.SetV(this->mCellV);
    theBottomRight.SetU(theU + this->mCellU);
    theBottomRight.SetV(theV + this->mCellV);
    theTopRight.SetU(theBottomRight.GetU());
    theTopRight.SetV(theTopLeft.GetV());
    theBottomLeft.SetU(theTopLeft.GetU());
    theBottomLeft.SetV(theBottomRight.GetV());
    inBitmap->SetTextureCoordinates(theTopLeft, theTopRight, theBottomLeft, theBottomRight);
    unsigned long theTextureHeight = this->mTexture->GetSurface()->GetHeight();
    unsigned long theTextureWidth = this->mTexture->GetWidth();
    inBitmap->SetXYScale((inRectangle.GetWidth() + 1.0f) / theTextureWidth,
                         (inRectangle.GetHeight() + 1.0f) / theTextureHeight);
}

// FUNCTION: EXE 0x4d5bdf
// Drops a reference to the button image once any load has finished, releasing its texture with the last one; returns 0
long EEButtonObject::Release()
{
    while (this->mIsLoading == true)
        Sleep(1);
    sDatabaseLock.mLock->Enter();
    if (this->mMediaCount == 1) {
        this->mTexture->Dereference();
        this->mTexture = NULL;
        this->mMediaCount = 0;
    } else if (this->mMediaCount > 0) {
        this->mMediaCount--;
    }
    sDatabaseLock.mLock->Leave();
    return 0;
}

// FUNCTION: EXE 0x4d5c31
// Reloads the textures of every referenced button image (a mip level dropped at 800 pixels wide or less, as on the
// first load); returns the first error
long EEButtonObject::Reload()
{
    GETextureDescription theDescription;
    theDescription.mPixelDepth = 16;
    theDescription.mNumMipLevelsToDrop = 0;
    theDescription.mSurfaceVolatility = 1;
    theDescription.mFrameFormat = 0;
    theDescription.mIs3DRenderable = false;
    theDescription.mIsTransparencyEnabled = false;
    theDescription.mHasAlpha = true;
    theDescription.mCreateMIPMaps = true;
    long theResult = kErrorOK;
    sDatabaseLock.mLock->Enter();
    theDescription.mPixelDepth = gTexturePixelDepth;
    theDescription.mCreateMIPMaps = false;
    // Same mip drop as the first load (one level at 800 pixels wide or less); both arms were 0.
    theDescription.mNumMipLevelsToDrop = gRasterizer->GetPrimarySurface(gScreenIndex)->GetWidth() > 800 ? 0 : 1;
    for (unsigned long i = 0; i < EEButtonObject::sTable.size() && theResult == kErrorOK; i++) {
        EEButtonObject* theButton = EEButtonObject::sTable[i];
        if (theButton != NULL && theButton->mMediaCount > 0) {
            theDescription.mHasAlpha = theButton->mID < 50;
            theResult = gTextureManager->ReloadTexture(theButton->mName, theDescription);
        }
    }
    sDatabaseLock.mLock->Leave();
    return theResult;
}

// FUNCTION: EXE 0x4d5cfe
// Loads the button image for a user (media interface): adds a reference
long EEButtonObject::LoadMedia()
{
    return this->Acquire();
}

// FUNCTION: EXE 0x4d5d03
// Unloads the button image for a user (media interface): drops a reference
long EEButtonObject::UnloadMedia()
{
    return this->Release();
}

// SYNTHETIC: EXE 0x4d5405
// SYNTHETIC: EXE 0x4d540f
// SYNTHETIC: EXE 0x4d5427
// SYNTHETIC: EXE 0x4d5433
// SYNTHETIC: EXE 0x4d544e
// SYNTHETIC: EXE 0x4d5458
// SYNTHETIC: EXE 0x4d547e
// SYNTHETIC: EXE 0x4d548a
