// C:\EEX\Low-Level Engine\User Interface\UIFullBackground.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// 0x10075330..0x10075b7c: UIFullBackground's implicit members (copy ctor, operator=, ??_E), the U2D key init chains,
// the ctor and the out-of-line members.
// Not exact: DrawBackground (inline budget). The original expands both GetWindow calls (vector operator[] out of
// line) and refuses GetTopLeft's GE2DPoint copy ctor; ours refuses the second GetWindow whole and expands the copy
// ctor. About 20 empty inline calls after the loop would reproduce the original's refusal set, so its source spent
// more budget late in the function than this one does.
#include <windows.h>

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
#include "lle/U2DRectangle.h"
#include "lle/GERasterizer.h"
#include "lle/GEBitmap.h"
#include "lle/TSCriticalSection.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"
#include "lle/UIFullBackground.h"

// FUNCTION: LLE 0x1007552f
// Creates a full-screen background of columns x rows tile bitmaps for a file name and design size, none loaded
UIFullBackground::UIFullBackground(const UString& inFileName, unsigned long inColumns, unsigned long inRows,
                                   UIForm* inForm, unsigned long inDesignedWidth, unsigned long inDesignedHeight)
    : mFileName(inFileName), mForm(inForm), mColumns(inColumns), mRows(inRows)
{
    this->mBitmaps = new GEBitmap*[inColumns * inRows];
    this->mDesignedWidth = inDesignedWidth;
    this->mDesignedHeight = inDesignedHeight;
    for (unsigned long theRow = 0; theRow < this->mRows; theRow++)
        for (unsigned long theColumn = 0; theColumn < this->mColumns; theColumn++)
            this->mBitmaps[this->mColumns * theRow + theColumn] = NULL;
}

// FUNCTION: LLE 0x100755a7
// Destroys the background, releasing its tile bitmaps
UIFullBackground::~UIFullBackground()
{
    this->UnloadBackground();
    delete[] this->mBitmaps;
}

// FUNCTION: LLE 0x100755c8
// Draws the tiles under the rectangle, each scaled from the designed screen size to the window
long UIFullBackground::DrawBackground(const GERectangle& inRectangle)
{
    GERectangle theClipped;
    long theResult = kErrorOK;
    GERectangle theTileRectangle;
    GERectangle theTextureRectangle;
    unsigned long theScreen;
    float theTileWidth;
    float theTileHeight;
    unsigned long theFirstRow;
    unsigned long theLastRow;
    unsigned long theFirstColumn;
    unsigned long theLastColumn;
    unsigned long theRow;
    unsigned long theColumn;
    theScreen = this->mForm->GetManager()->GetScreen();
    GEScreen* theScreenObject = this->mForm->GetManager()->GetRasterizer()->GetScreen(theScreen);
    unsigned long theTextureWidth = this->mBitmaps[0]->GetTexture(theScreen)->GetWidth();
    unsigned long theTextureHeight = this->mBitmaps[0]->GetTexture(theScreen)->GetHeight();
    theTextureRectangle = GERectangle(0.0f, 0.0f, (float)theTextureWidth, (float)theTextureHeight);
    theTileWidth = (float)this->mForm->GetManager()->GetRasterizer()->GetWindow(theScreen)->GetWidth() / this->mDesignedWidth *
                   theTextureWidth;
    theTileHeight = (float)this->mForm->GetManager()->GetRasterizer()->GetWindow(theScreen)->GetHeight() /
                    this->mDesignedHeight * theTextureHeight;
    theFirstRow = UMFloor(inRectangle.GetTop() / theTileHeight);
    theLastRow = UMFloor(inRectangle.GetBottom() / theTileHeight);
    theFirstColumn = UMFloor(inRectangle.GetLeft() / theTileWidth);
    theLastColumn = UMFloor(inRectangle.GetRight() / theTileWidth);
    if (theLastRow >= this->mRows)
        theLastRow = this->mRows - 1;
    if (theLastColumn >= this->mColumns)
        theLastColumn = this->mColumns - 1;
    for (theRow = theFirstRow; theRow <= theLastRow; theRow++) {
        for (theColumn = theFirstColumn; theColumn <= theLastColumn; theColumn++) {
            unsigned long theIndex = this->mColumns * theRow + theColumn;
            float theTop = theRow * theTileHeight;
            float theBottom = theTop + theTileHeight - 1.0f;
            float theLeft = theColumn * theTileWidth;
            float theRight = theLeft + theTileWidth - 1.0f;
            theTileRectangle.SetTop(theTop);
            theTileRectangle.SetBottom(theBottom);
            theTileRectangle.SetLeft(theLeft);
            theTileRectangle.SetRight(theRight);
            if (!theTileRectangle.And(inRectangle, theClipped))
                continue;
            this->mBitmaps[theIndex]->ComputeStretchedCoordsAndScale(theTileRectangle, theClipped, theTextureRectangle,
                                                               theScreen);
            this->mBitmaps[theIndex]->SetLocation(theClipped.GetTopLeft());
            theResult = this->mForm->GetManager()->GetRasterizer()->Draw(this->mBitmaps[theIndex], theScreen);
        }
    }
    return theResult;
}

// FUNCTION: LLE 0x100758d7
// Loads one clamped, filtered texture per tile ("name_row_column.ext") into a new bitmap each; returns kErrorOK
long UIFullBackground::LoadBackground(const GETextureDescription& inDescription)
{
    UString theColumnString;
    UString theRowString;
    UString theFileName;
    char theNumber[3];                  // 3, not 4: a 4-byte array takes the slot below theRow
    unsigned long theScreen = this->mForm->GetManager()->GetScreen();
    GETextureManager* theManager = this->mForm->GetManager()->GetRasterizer()->GetTextureManager(theScreen);
    for (unsigned long theRow = 0; theRow < this->mRows; theRow++) {
        PlatIToA(theRow + 1, theNumber, 10);
        theRowString = theNumber;
        for (unsigned long theColumn = 0; theColumn < this->mColumns; theColumn++) {
            unsigned long theIndex = this->mColumns * theRow + theColumn;
            PlatIToA(theColumn + 1, theNumber, 10);
            theColumnString = theNumber;
            const char* theExtension = strrchr(this->mFileName, '.');
            if (theExtension) {
                char theBuffer[256];
                unsigned long theLength = theExtension - (const char*)this->mFileName;
                memcpy(theBuffer, (const char*)this->mFileName, theLength);
                sprintf(theBuffer + theLength, "_%d_%d%s", theRow + 1, theColumn + 1, theExtension);
                theFileName = theBuffer;
            } else
                theFileName = this->mFileName + "_" + theRowString + "_" + theColumnString;
            GETexture* theTexture = theManager->LoadTexture(theFileName, inDescription);
            theTexture->SetStageAlphaMode(kStageOpModulate, -1);
            theTexture->SetStageColorMode(kStageOpModulate, -1);
            theTexture->SetStageMinFilterMode(1, -1);
            theTexture->SetStageMagFilterMode(1, -1);
            theTexture->SetStageMipFilterMode(kMipFilterPoint, -1);
            theTexture->SetStageMipMapBias(-0.75f, -1);
            theTexture->SetStageTextureAddressingMode(kAddressingClamp, -1);
            this->mBitmaps[theIndex] = new GEBitmap;
            this->mBitmaps[theIndex]->SetTexture(theTexture, theScreen);
        }
    }
    return kErrorOK;
}

// FUNCTION: LLE 0x10075b31
// Releases every tile bitmap
long UIFullBackground::UnloadBackground()
{
    for (unsigned long theRow = 0; theRow < this->mRows; theRow++)
        for (unsigned long theColumn = 0; theColumn < this->mColumns; theColumn++) {
            unsigned long theIndex = this->mColumns * theRow + theColumn;
            if (this->mBitmaps[theIndex]) {
                this->mBitmaps[theIndex]->Dereference();
                this->mBitmaps[theIndex] = NULL;
            }
        }
    return kErrorOK;
}

// Compiler-generated: the implicit copy ctor and operator= of the exported class, and ??_E
// SYNTHETIC: LLE 0x10075330
// SYNTHETIC: LLE 0x10075375
// SYNTHETIC: LLE 0x100753b4

// Compiler-generated: the U2D key init chains (names in analysis/lle/names.txt)
// SYNTHETIC: LLE 0x1007540f
// SYNTHETIC: LLE 0x10075419
// SYNTHETIC: LLE 0x10075429
// SYNTHETIC: LLE 0x10075435
// SYNTHETIC: LLE 0x1007543f
// SYNTHETIC: LLE 0x10075449
// SYNTHETIC: LLE 0x10075459
// SYNTHETIC: LLE 0x10075465
// SYNTHETIC: LLE 0x1007546f
// SYNTHETIC: LLE 0x10075479
// SYNTHETIC: LLE 0x10075489
// SYNTHETIC: LLE 0x10075495
// SYNTHETIC: LLE 0x1007549f
// SYNTHETIC: LLE 0x100754a9
// SYNTHETIC: LLE 0x100754b9
// SYNTHETIC: LLE 0x100754c5
// SYNTHETIC: LLE 0x100754cf
// SYNTHETIC: LLE 0x100754d9
// SYNTHETIC: LLE 0x100754e9
// SYNTHETIC: LLE 0x100754f5
// SYNTHETIC: LLE 0x100754ff
// SYNTHETIC: LLE 0x10075509
// SYNTHETIC: LLE 0x10075519
// SYNTHETIC: LLE 0x10075525
