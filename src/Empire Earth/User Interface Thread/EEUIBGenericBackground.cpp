// C:\EEX\Empire Earth\User Interface Thread\EEUIBGenericBackground.cpp (file name inferred: no __FILE__; RTTI class
// EEUIBGenericBackground). The tinted, framed form background: 0x254911..0x255c2a (the four GERectangle `$E` chains
// of the frame's texture cells, ctor, ??_G, dtor, the frame pieces, DrawBackground, Open, then 0x2556e6;
// EEUIBColor ends at 0x254911, EEUIBSavedGroup starts at 0x255c2a). Unit flags: the exe module set.
#include <windows.h>
#include <mbstring.h>
#include <algorithm>
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
#include "lle/NEMessage.h"
#include "lle/NESynchronousCommunications.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEMessage.h"
#include "ee/EECommunications.h"
#include "lle/UResourceFile.h"
#include "lle/GEFont.h"
#include "lle/GEText.h"
#include "lle/UIScrollBar.h"
#include "lle/UITextLabel.h"
#include "lle/UIEditBox.h"
#include "ee/EmpireEarth.h"
#include "lle/UIBitmapBackground.h"
#include "ee/EEUIBGenericBackground.h"
#include "lle/FSPath.h"
#include "lle/UITextureBackground.h"
#include "lle/UIBFull.h"
#include "ee/EEGameOptions.h"
#include "ee/EEGamePreferences.h"
#include "ee/EEUIManager.h"
#include "ee/EESaveGameFiles.h"
#include "ee/EEUIBMapBackground.h"
#include "rust/ee_screens.h"   // ee_screens_frame_draw (ee_screens frame.rs)

extern char gEmptyString[];                 // 0x4e5b98 (pooled "")

// the frame's cells in its 64x64 texture (provisional names)
static GERectangle sEdgeCell(57.0f, 0.0f, 64.0f, 64.0f);           // 0x564558 (the edges)
static GERectangle sFrameCornerCell(0.0f, 0.0f, 28.0f, 38.0f);     // 0x564548 (the large corners)
static GERectangle sCornerCell(49.0f, 0.0f, 56.0f, 7.0f);          // 0x564538 (the small corners)
static GERectangle sSideCell(30.0f, 0.0f, 48.0f, 20.0f);           // 0x564528 (the side ornaments)


// SYNTHETIC: EXE 0x654911
// SYNTHETIC: EXE 0x65491b
// SYNTHETIC: EXE 0x654940
// SYNTHETIC: EXE 0x65494c
// SYNTHETIC: EXE 0x65494d
// SYNTHETIC: EXE 0x654957
// SYNTHETIC: EXE 0x65497c
// SYNTHETIC: EXE 0x654988
// SYNTHETIC: EXE 0x654989
// SYNTHETIC: EXE 0x654993
// SYNTHETIC: EXE 0x6549ba
// SYNTHETIC: EXE 0x6549c6
// SYNTHETIC: EXE 0x6549c7
// SYNTHETIC: EXE 0x6549d1
// SYNTHETIC: EXE 0x6549f8
// SYNTHETIC: EXE 0x654a04

// FUNCTION: EXE 0x654a05
// Creates a tinted form background with optional frame pieces (flags: 1 edges, 2 frame corners, 4 sides)
EEUIBGenericBackground::EEUIBGenericBackground(unsigned long inBackgroundID, const UString& inTextureFileName,
                                               const GETextureDescription& inDescription,
                                               const UILayoutUtility& inLayout, UIForm* inForm,
                                               unsigned long inDrawPriority, const GEColor& inColor, bool inVisible,
                                               float inOpacity, float inScaleX, float inScaleY, unsigned long inFlags)
    : UIBitmapBackground(inBackgroundID, inTextureFileName, inDescription, GERectangle(), inLayout, inForm,
                         inDrawPriority, inVisible)
{
    this->mOpacity = inOpacity;
    this->mFrameFlags = inFlags;
    this->mColor = inColor;
    this->mScaleX = inScaleX;
    this->mScaleY = inScaleY;
}

// SYNTHETIC: EXE 0x654aa4
// (the scalar deleting destructor, emitted with the vtable; slot EEUIBGenericBackground+0x0)

// FUNCTION: EXE 0x654ac0
// Destroys the form background
EEUIBGenericBackground::~EEUIBGenericBackground()
{
}

// FUNCTION: EXE 0x654acc
// FUNCTION: EXE 0x654c5d
// FUNCTION: EXE 0x654e05
// FUNCTION: EXE 0x654f98
// FUNCTION: EXE 0x655131
// FUNCTION: EXE 0x65528a
// FUNCTION: EXE 0x655463
// Draws the tinted background inside the side insets, then the frame pieces its flags ask for, each clipped to the
// redraw rectangle (the original's DrawCorners, DrawEdges, DrawFrameCorners, DrawSides, DrawEdge and DrawCorner:
// ee_screens frame.rs over the two host calls below)
long EEUIBGenericBackground::DrawBackground(const GERectangle& inRectangle)
{
    return ee_screens_frame_draw(this, &this->mRectangle, this->mScaleX, this->mScaleY, this->mFrameFlags, &inRectangle);
}

// The background's host (frame.rs): the tinted panel over a rectangle (left, top, right, bottom); the draw's result
extern "C" long ee_screenscpp_frame_fill(void* inBackground, const float* inInner)
{
    EEUIBGenericBackground* theBackground = static_cast<EEUIBGenericBackground*>(inBackground);
    GERhombus theRhombus;
    theRhombus.SetOpacity(theBackground->mOpacity);
    theRhombus.SetColor(theBackground->mColor);
    theRhombus.SetRectangle(GERectangle(inInner[0], inInner[1], inInner[2], inInner[3]));
    UIManager* theManager = theBackground->GetForm()->GetManager();
    return theManager->GetRasterizer()->Draw(&theRhombus, theManager->GetScreen());
}

// One frame piece: the bitmap stretched from the piece's rectangle onto its visible part with the cell, its texture
// coordinates reordered (each order the new corners' indices into the current top-left, top-right, bottom-right,
// bottom-left; the scales swapped after a quarter turn), placed and drawn
extern "C" void ee_screenscpp_frame_piece(void* inBackground, const EeScreensFramePiece* inPiece)
{
    static const GERectangle* kCells[] = {&sEdgeCell, &sFrameCornerCell, &sCornerCell, &sSideCell};
    EEUIBGenericBackground* theBackground = static_cast<EEUIBGenericBackground*>(inBackground);
    GEBitmap* theBitmap = theBackground->GetBitmap();
    UIManager* theManager = theBackground->GetForm()->GetManager();
    unsigned long theScreen = theManager->GetScreen();
    const float* t = inPiece->texture;
    const float* d = inPiece->destination;
    theBitmap->ComputeStretchedCoordsAndScale(GERectangle(t[0], t[1], t[2], t[3]), GERectangle(d[0], d[1], d[2], d[3]),
                                              *kCells[inPiece->cell], theScreen);
    for (int i = 0; i < 3; i++) {
        uint32_t theOrder = inPiece->orders[i];
        if (theOrder == EE_SCREENS_FRAME_KEEP)
            continue;
        GETextureCoordinate c[4];
        theBitmap->GetTextureCoordinatesPerVertex(c[0], c[1], c[2], c[3]);
        theBitmap->SetTextureCoordinates(c[theOrder & 3], c[theOrder >> 2 & 3], c[theOrder >> 4 & 3], c[theOrder >> 6 & 3]);
        if (i == 0 && inPiece->swap_scale)
            theBitmap->SetXYScale(theBitmap->GetYScale(), theBitmap->GetXScale());
    }
    theBitmap->SetLocation(GE2DPoint(inPiece->location[0], inPiece->location[1]));
    theManager->GetRasterizer()->Draw(theBitmap, theScreen);
}

// FUNCTION: EXE 0x655675
// Opens the background and scales its frame to the window size against the form's designed size
void EEUIBGenericBackground::Open()
{
    UIBackground::Open();
    this->mScaleX = (float)gGameWindow->GetWidth() / (float)this->mForm->GetDesignedWidth();
    this->mScaleY = (float)gGameWindow->GetHeight() / (float)this->mForm->GetDesignedHeight();
}

// FUNCTION: EXE 0x6556d9
// Draws the campaign map background
long EEUIBMapBackground::DrawBackground(const GERectangle& inRectangle)
{
    return UIBFull::DrawBackground(inRectangle);
}

// FUNCTION: EXE 0x6556e6
// Loads the campaign map image (PNG or JPG; the original's were TGA or JPG) and cuts it into 256x256 textures, one
// per background cell
// The image comes from the saved game/scenario resources, else from the startup path
long EEUIBMapBackground::LoadBackground()
{
    GERectangle theSource;
    GERectangle theDestination;
    GETextureDescription theTextureDescription = { 1, 0, 16, 0, false, false, true, true };
    GESurfaceDescription theDescription;
    GESurfaceDescription theCellDescription;
    UString theExtension;
    FSFileSpec theDirectory;
    long theResult;
    FSFileSpec* theFile = NULL;
    GESurface* theSecondary =
        GERasterizer::GetActiveRasterizer()->GetSecondarySurface(gUIManager->GetScreen());
    GESurface* theImage = NULL;
    GESurface* theCell = NULL;
    GEScreen* theScreen = GERasterizer::GetActiveRasterizer()->GetScreen(gUIManager->GetScreen());
    theDescription.mPixelFormat = theSecondary->GetPixelFormat();
    theDescription.mPixelDepth = theSecondary->GetPixelDepth();
    theDescription.mType = kSurfaceSystemMemory;
    theDescription.mSurfaceVolatility = 1;
    theDescription.m14 = 0;
    theDescription.mMipLevelCount = 0;
    theDescription.mIs3DRenderable = false;
    theDescription.mIsTransparencyEnabled = false;
    theResult = EEOpenSaveGameFile(this->mTextureFileName, UString(gEmptyString), theFile);
    if (theResult != 0) {
        theDirectory.SetVolume(gStartupVolume);
        theDirectory.SetDirectory(gStartupDirectory);
        FSPath* thePath = new FSPath;
        thePath->AddDirectory(theDirectory, true);
        theResult = thePath->FindFile(this->mTextureFileName, theFile);
        thePath->Dereference();
        if (theResult != 0)
            return theResult;
    }
    theExtension = theFile->GetFile().GetSubstring(theFile->GetFile().GetLength() - 4, 4);
    unsigned long theBits;
    if (PlatStrICmp(theExtension, ".png") == 0) {
        theResult = GESurface::GetImageFileAttributes(*theFile, kImageFilePNG, theDescription.mWidth, theDescription.mHeight,
                                                      theBits);
        if (theResult != 0)
            goto done;
        theScreen->CreateSurface(theDescription, &theImage);
        theResult = theImage->Load(*theFile, kImageFilePNG);
    } else {
        theResult = GESurface::GetImageFileAttributes(*theFile, kImageFileJPG, theDescription.mWidth, theDescription.mHeight,
                                                      theBits);
        if (theResult != 0)
            goto done;
        theScreen->CreateSurface(theDescription, &theImage);
        theResult = theImage->Load(*theFile, kImageFileJPG);
    }
    if (theResult == 0) {
        unsigned long theWidth = theDescription.mWidth;
        unsigned long theHeight = theDescription.mHeight;
        unsigned long theColumns = theWidth >> 8;
        unsigned long theRows = theHeight >> 8;
        if (theWidth & 0xff)
            theColumns++;
        if (theHeight & 0xff)
            theRows++;
        theCellDescription = theDescription;
        theCellDescription.mType = kSurfaceTexture;
        theCellDescription.mWidth = 256;
        theCellDescription.mHeight = 256;
        this->SetNumberColumnsAndRows(theColumns, theRows);
        theTextureDescription.mIs3DRenderable = theCellDescription.mIs3DRenderable;
        theTextureDescription.mPixelDepth = gGamePreferences.mSettings.mGameBitDepth;
        theTextureDescription.mSurfaceVolatility = theCellDescription.mSurfaceVolatility;
        theTextureDescription.mIsTransparencyEnabled = theCellDescription.mIsTransparencyEnabled;
        this->mTextureDescription = theTextureDescription;
        this->mTextureRectangle = GERectangle(0.0f, 0.0f, (float)(theWidth - 1), (float)(theHeight - 1));
        unsigned long theX = 0;
        for (unsigned long theColumn = 0; theColumn < theColumns; theColumn++, theX += 256) {
            unsigned long theY = 0;
            for (unsigned long theRow = 0; theRow < theRows; theRow++, theY += 256) {
                unsigned long theRight = theX + 256;
                if (theRight > theWidth)
                    theRight = theWidth;
                unsigned long theBottom = theY + 256;
                if (theBottom > theHeight)
                    theBottom = theHeight;
                theResult = theScreen->CreateSurface(theCellDescription, &theCell);
                if (theResult == 0) {
                    theSource = GERectangle((float)theX, (float)theY, (float)theRight, (float)theBottom);
                    theDestination = GERectangle(0.0f, 0.0f, (float)(theRight - theX), (float)(theBottom - theY));
                    theResult = theImage->CopyTo(theCell, theSource, theDestination, false);
                    if (theResult == 0) {
                        GETexture* theTexture = theScreen->GetTextureManager()->MakeTexture(theCell);
                        if (theTexture != NULL)
                            this->SetBitmapTexture(theTexture, theColumn, theRow);
                    }
                    theCell->Dereference();
                }
            }
        }
    }
    // Release the image surface also when its load failed.
    if (theImage != NULL)
        theImage->Dereference();
done:
    if (theFile != NULL)
        delete theFile;
    return theResult;
}

