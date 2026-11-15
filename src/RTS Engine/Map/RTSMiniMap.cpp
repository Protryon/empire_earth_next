// C:\EEX\RTS Engine\Map\RTSMiniMap.cpp (__FILE__ 0x4bf138). The mini map's engine side (RTTI RTSMiniMap, vtable
// 0x441dc4): 0x1240b3..0x1245bb (the ctor, the dtor, the battle-flag cell queue, then the list<RTSMiniMapCell>
// COMDATs after their first users; RTSTile.cpp's key chains open the next unit). Unit flags: the exe's module set.
#include <windows.h>
#include <list>
#include <string.h>

#include "lle/LowLevelEngine.h"
#include "lle/GE2DPoint.h"
#include "lle/GEColor.h"
#include "lle/GETransformation.h"
#include "lle/GESurface.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GETextureManager.h"
#include "lle/GETexture.h"
#include "lle/GEScreen.h"
#include "lle/TSCriticalSection.h"
#include "lle/TSScheduler.h"

#include "ee/EEMiniMap.h"
#include "lle/GERenderState.h"

// FUNCTION: EXE 0x5240b3
// Creates a player's mini map: a 128x128 terrain texture, a 64x64 world texture and unit layer cleared to
// black, their locks and an empty 64x64 battle-flag grid
RTSMiniMap::RTSMiniMap(RTSPlayer* inPlayer, GEScreen* inScreen)
    // (the four mCorner GE2DPoints zero themselves: GE2DPoint() inline)
{
    long theResult;
    GESurface* theSurface;
    GESurfaceDescription theDescription;
    this->mViewingPlayer = inPlayer;
    this->mTerrainTexture = NULL;
    this->mWorldTexture = NULL;
    this->mUnitSurface = NULL;
    theDescription.mWidth = 0x80;
    theDescription.mHeight = 0x80;
    theSurface = NULL;
    theDescription.mType = kSurfaceTexture;
    theDescription.mPixelFormat = kPixelFormatR5G6B5;
    theDescription.mPixelDepth = 0x10;
    theDescription.mSurfaceVolatility = 1;
    theDescription.m14 = 0;
    theDescription.mMipLevelCount = 0;
    theDescription.mIs3DRenderable = false;
    theDescription.mIsTransparencyEnabled = false;
    theResult = inScreen->CreateSurface(theDescription, &theSurface);
    ESVERIFY_AT(theResult == keseOK, 61);

    // the terrain texture
    this->mTerrainTexture = inScreen->GetTextureManager()->MakeTexture(theSurface);
    ESVERIFY_AT(this->mTerrainTexture != NULL, 65);
    this->mTerrainTexture->Reference();
    this->mTerrainTexture->SetStageMinFilterMode(1, 0xffffffff);
    this->mTerrainTexture->SetStageMagFilterMode(1, 0xffffffff);
    this->mTerrainTexture->SetStageMipFilterMode(kMipFilterNone, 0xffffffff);
    this->mTerrainTexture->SetStageTextureAddressingMode(kAddressingClamp, 0xffffffff);
    theSurface->Dereference();

    // the world surface
    theDescription.mType = kSurfaceTexture;
    theDescription.mPixelFormat = kPixelFormatA4R4G4B4;
    theDescription.mSurfaceVolatility = 0;
    theDescription.mWidth = 0x40;
    theDescription.mHeight = 0x40;
    theDescription.m14 = 0;
    theDescription.mMipLevelCount = 0;
    theDescription.mIs3DRenderable = false;
    theDescription.mIsTransparencyEnabled = false;
    theResult = inScreen->CreateSurface(theDescription, &theSurface);
    ESVERIFY_AT(theResult == keseOK, 91);
    GEColor theTemp1(0.0f, 0.0f, 0.0f);
    // cleared to black
    theSurface->Clear(theTemp1, 1.0f);

    // the world texture's handle
    this->mWorldTexture = inScreen->GetTextureManager()->MakeTexture(theSurface);
    ESVERIFY_AT(this->mWorldTexture != NULL, 98);
    this->mWorldTexture->Reference();
    this->mWorldTexture->SetStageMinFilterMode(0, 0xffffffff);
    this->mWorldTexture->SetStageMagFilterMode(0, 0xffffffff);
    this->mWorldTexture->SetStageMipFilterMode(kMipFilterNone, 0xffffffff);
    this->mWorldTexture->SetStageTextureAddressingMode(kAddressingClamp, 0xffffffff);
    theSurface->Dereference();

    // the unit layer's surface
    theDescription.mType = kSurfaceSystemMemory;
    theDescription.mPixelFormat = kPixelFormatA4R4G4B4;
    theDescription.mSurfaceVolatility = 0;
    theDescription.mWidth = 0x40;
    theDescription.mHeight = 0x40;
    theDescription.m14 = 0;
    theDescription.mMipLevelCount = 0;
    theDescription.mIs3DRenderable = false;
    theDescription.mIsTransparencyEnabled = false;
    theResult = inScreen->CreateSurface(theDescription, &this->mUnitSurface);
    ESVERIFY_AT(theResult == keseOK, 124);
    GEColor theTemp2(0.0f, 0.0f, 0.0f);
    // the unit layer
    this->mUnitSurface->Clear(theTemp2, 1.0f);

    // the locks
    ESVERIFY_AT(TSScheduler::CreateCriticalSection(this->mTerrainCriticalSection) == keseOK, 131);
    ESVERIFY_AT(TSScheduler::CreateCriticalSection(this->mUnitCriticalSection) == keseOK, 132);

    // the battle flags
    this->mBattleFlagsWidth = 0x40;
    this->mBattleFlagsHeight = 0x40;
    this->mBattleFlags = new unsigned char[0x1000];
    ESVERIFY_AT(this->mBattleFlags != NULL, 138);
    memset(this->mBattleFlags, 0, 0x1000);
}

// SYNTHETIC: EXE 0x524418
// (the scalar deleting destructor, emitted with the vtable; slot RTSMiniMap+0x0)

// FUNCTION: EXE 0x524434
// Destroys the mini map, releasing its locks, textures, unit layer and battle-flag grid
RTSMiniMap::~RTSMiniMap()
{
    TSScheduler::DestroyCriticalSection(this->mTerrainCriticalSection);
    TSScheduler::DestroyCriticalSection(this->mUnitCriticalSection);
    this->mTerrainTexture->Dereference();
    this->mWorldTexture->Dereference();
    this->mUnitSurface->Dereference();
    if (this->mBattleFlags != NULL)
        delete[] this->mBattleFlags;
}

// FUNCTION: EXE 0x524497
// Flags the mini-map cell under a map point as a fresh battle (8), queueing the cell if it was not flagged
void RTSMiniMap::Ping(const GE2DPoint& inPoint)
{
    GE2DPoint theCenter(inPoint.GetX() + 0.5f, inPoint.GetY() + 0.5f);
    GE2DPoint thePoint;
    RTSMiniMapCell theCell;
    this->mMapToCell.Apply(theCenter, thePoint);
    theCell.mX = (unsigned char)thePoint.GetX();
    theCell.mY = (unsigned char)thePoint.GetY();
    if (this->mBattleFlags[theCell.mY * this->mBattleFlagsWidth + theCell.mX] == 0)
        this->mBattleCells.push_back(theCell);
    this->mBattleFlags[theCell.mY * this->mBattleFlagsWidth + theCell.mX] = 8;
}

// TEMPLATE: EXE 0x524540
// std::list<RTSMiniMapCell>::list (the ctor's mU148)

// TEMPLATE: EXE 0x524563
// std::list<RTSMiniMapCell>::_Insert (Ping's push_back)
