// C:\EEX\Empire Earth\Database\EETerrainColorTable.cpp (__FILE__ 0x4bcd50). The terrain colour palette:
// 0xdae63..0xdb169 (the class static table's init chain first; EEStartingResources ends at 0xdae63, the next unit's
// chain starts at 0xdb169). Unit flags: the exe's module set. Data: the vtable 0x43f7d8, the scale 0x43f7dc.
#include <windows.h>
#include <stdint.h>
#include <vector>

#include "lle/LowLevelEngine.h"
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
#include "lle/FSFileSpec.h"
#include "lle/FSArchive.h"
#include "lle/FSPath.h"

#include "ee/EmpireEarth.h"
#include "ee/EETerrainColorTable.h"

std::vector<EETerrainColorTable*> EETerrainColorTable::sTable;

static const float kColorScale = 1.0f / 255.0f;

// FUNCTION: EXE 0x4daeac
// Creates an empty terrain colour entry
EETerrainColorTable::EETerrainColorTable()
{
}

// FUNCTION: EXE 0x4daec4
// Loads the terrain colours from the pixels of textures\ColorPalette.tga (16 per row); returns the texture load status
long EETerrainColorTable::Load()
{
    GESurfaceDescription theDescription;
    UString theFileName;
    GESurface* theSurface = NULL;
    FSFileSpec* theFile = NULL;
    theDescription.mType = kSurfaceSystemMemory;
    theDescription.mPixelFormat = kPixelFormatA8R8G8B8;
    theDescription.mPixelDepth = 32;
    theDescription.mSurfaceVolatility = 0;
    theDescription.mWidth = 16;
    theDescription.mHeight = 16;
    theDescription.m14 = 0;
    theDescription.mMipLevelCount = 0;
    theDescription.mIs3DRenderable = false;
    theDescription.mIsTransparencyEnabled = false;
    long theResult = gRasterizer->GetScreen(gScreenIndex)->CreateSurface(theDescription, &theSurface);
    ESVERIFY_AT(theResult == keseOK, 76);

    theFileName = UString("textures\\ColorPalette.tga");
    theResult = GEFindTargaFile(gTextureManager->GetTexturePath(), theFileName, theFile);   // or its .png
    ESVERIFY_AT(theResult == keseOK, 81);
    ESVERIFY_AT(theFile != NULL, 82);
    theResult = theSurface->Load(*theFile, kImageFileTGA);
    ESVERIFY_AT(theResult == keseOK, 84);
    if (theFile != NULL)
        delete theFile;
    theSurface->Lock();
    uint32_t* thePixels = reinterpret_cast<uint32_t*>(theSurface->GetMemoryAddress());
    unsigned long theRowLength = theSurface->GetBytesPerRow() / 4;
    if (theResult == kErrorOK) {
        unsigned long theCount = theSurface->GetWidth() * theSurface->GetHeight();
        EETerrainColorTable::sTable.resize(theCount);
        for (unsigned long i = 0; i < theCount; i++) {
            EETerrainColorTable* theColor = new EETerrainColorTable;
            unsigned long thePixel = thePixels[(i / 16) * theRowLength + (i % 16)];
            theColor->mColor.SetRed(GEUnpackRed(0, thePixel) * kColorScale);
            theColor->mColor.SetGreen(GEUnpackGreen(kPixelFormatA8R8G8B8, thePixel) * kColorScale);
            theColor->mColor.SetBlue(GEUnpackBlue(0, thePixel) * kColorScale);
            EETerrainColorTable::sTable[i] = theColor;
        }
    }
    theSurface->Unlock();
    theSurface->Dereference();
    return theResult;
}

// FUNCTION: EXE 0x4db0e7
// Destroys every terrain colour and empties the table
void EETerrainColorTable::Unload()
{
    for (unsigned long i = 0; i < EETerrainColorTable::sTable.size(); i++)
        delete EETerrainColorTable::sTable[i];
    EETerrainColorTable::sTable.resize(0);
}

// TEMPLATE: EXE 0x4db127
// std::copy_backward for the table's _Ptrit (vector insert; ICF-named for vector<long>)

// SYNTHETIC: EXE 0x4db14c
// (the scalar deleting destructor)

// SYNTHETIC: EXE 0x4dae63
// SYNTHETIC: EXE 0x4dae6d
// SYNTHETIC: EXE 0x4dae85
// SYNTHETIC: EXE 0x4dae91
