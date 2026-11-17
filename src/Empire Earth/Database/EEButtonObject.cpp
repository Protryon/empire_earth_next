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
#include "ee/EEDbRecords.h"

std::vector<EEButtonObject*> EEButtonObject::sTable;

#include <EEDatabase.h>

// 0x5308b4: the pixel depth textures are loaded with (16 or 32; provisional name)
extern unsigned long& gTexturePixelDepth;

// Creates a button image entry with no texture and no references
EEButtonObject::EEButtonObject()
    : EeDataButtonFields(), mTexture(NULL), mCellU(0.0f), mCellV(0.0f), mMediaCount(0)
{
    this->mIsLoading = false;
}

// Destroys the button image entry, releasing its texture
EEButtonObject::~EEButtonObject()
{
    if (this->mTexture != NULL)
        this->mTexture->Dereference();
}

// Loads the button image entries (file name, cell row and column, id, hot key, position: the record and its copy are
// ee_data tables.rs') from db\dbButtons.dat; stops at the first read error and returns it
long EEButtonObject::Load(FSFileSpec* inFile)
{
    return EEDbReadRecords(inFile, ee_data_button_record_size(),
        [](unsigned long inCount) { EEButtonObject::sTable.resize(inCount, NULL); return true; },
        [](unsigned long inIndex, const unsigned char* inRecord) {
            EEButtonObject* theButton = new EEButtonObject;
            EEButtonObject::sTable[inIndex] = theButton;
            EEDbCopyName(theButton->mName, ee_data_button_from_record(inRecord, theButton));
        }, true);
}

// Destroys every button image entry and empties the table
void EEButtonObject::Unload()
{
    for (unsigned long i = 0; i < EEButtonObject::sTable.size(); i++)
        delete EEButtonObject::sTable[i];
    EEButtonObject::sTable.resize(0);
}

// Writes each button image with a non-zero reference count to a media dump file
void EEButtonObject::DumpAllLoadedMedia(FSFileSpec& inFile)
{
    fprintf(inFile, "\n\n**********Button Database Table*********\n\n");
    for (unsigned long i = 0; i < EEButtonObject::sTable.size(); i++)
        if (EEButtonObject::sTable[i] != NULL && EEButtonObject::sTable[i]->mMediaCount > 0)
            fprintf(inFile, "\t%s (filename) has a media count of %ld\n", (const char*)EEButtonObject::sTable[i]->mName,
                    EEButtonObject::sTable[i]->mMediaCount);
}

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
            PlatSleep(1);
        sDatabaseLock.mLock->Enter();
    }
    sDatabaseLock.mLock->Leave();
    return theResult;
}

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
        EeDataButtonTexture theSize = {this->mTexture->GetWidth(), this->mTexture->GetHeight(),
                                       this->mTexture->GetNumMipLevelsToDrop(), 0};
        float theCell[2];
        ee_data_button_cell_offset(this->mColumn, this->mRow, &theSize, theCell);
        this->mCellU = theCell[0];
        this->mCellV = theCell[1];
    } else {
        theResult = 3;
    }
    return theResult;
}

// Shows the button's cell on a bitmap scaled to a rectangle: a 40-texel cell on textures 256 or more wide, else the
// whole texture (ee_data tables.rs); inInset: one texel in
static void EESetButtonBitmap(GETexture* inTexture, float inCellU, float inCellV, bool inInset, GEBitmap* inBitmap,
                              const GERectangle& inRectangle, unsigned long inScreen)
{
    EeDataButtonTexture theSize = {inTexture->GetWidth(), inTexture->GetHeight(), inTexture->GetNumMipLevelsToDrop(),
                                   inTexture->GetSurface()->GetHeight()};
    inBitmap->SetTexture(inTexture, inScreen);
    EeDataButtonCell theCell = ee_data_button_cell(&theSize, inCellU, inCellV, inInset, inRectangle.GetWidth(),
                                                   inRectangle.GetHeight());
    GETextureCoordinate theTopLeft;
    GETextureCoordinate theTopRight;
    GETextureCoordinate theBottomLeft;
    GETextureCoordinate theBottomRight;
    theTopLeft.SetU(theCell.left);
    theTopLeft.SetV(theCell.top);
    theTopRight.SetU(theCell.right);
    theTopRight.SetV(theCell.top);
    theBottomLeft.SetU(theCell.left);
    theBottomLeft.SetV(theCell.bottom);
    theBottomRight.SetU(theCell.right);
    theBottomRight.SetV(theCell.bottom);
    inBitmap->SetTextureCoordinates(theTopLeft, theTopRight, theBottomLeft, theBottomRight);
    inBitmap->SetXYScale(theCell.scale_x, theCell.scale_y);
}

// Shows the button's cell, inset by one texel, on a bitmap scaled to a rectangle; a button without a texture (or
// whose surface is not loaded: EE-SIM's textures, a scenario's information portrait) is skipped
void EEButtonObject::SetFaceBitmap(GEBitmap* inBitmap, const GERectangle& inRectangle, unsigned long inScreen)
{
    if (this->mTexture == NULL || this->mTexture->GetSurface() == NULL)
        return;
    EESetButtonBitmap(this->mTexture, this->mCellU, this->mCellV, true, inBitmap, inRectangle, inScreen);
}

// Shows the pressed button's cell (no texel inset) on a bitmap scaled to a rectangle (no texture test)
void EEButtonObject::SetPressedBitmap(GEBitmap* inBitmap, const GERectangle& inRectangle, unsigned long inScreen)
{
    EESetButtonBitmap(this->mTexture, this->mCellU, this->mCellV, false, inBitmap, inRectangle, inScreen);
}

// Drops a reference to the button image once any load has finished, releasing its texture with the last one; returns 0
long EEButtonObject::Release()
{
    while (this->mIsLoading == true)
        PlatSleep(1);
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

// Loads the button image for a user (media interface): adds a reference
long EEButtonObject::LoadMedia()
{
    return this->Acquire();
}

// Unloads the button image for a user (media interface): drops a reference
long EEButtonObject::UnloadMedia()
{
    return this->Release();
}
