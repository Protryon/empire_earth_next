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

extern char gU4E5B98[];                     // 0x4e5b98 (pooled "")
#include "LostDebug.h"

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
// Draws the frame's four small corners, clipped to the redraw rectangle
void EEUIBGenericBackground::DrawCorners(const GERectangle& inRectangle)
{
    GERectangle theClipped;
    GEBitmap* theBitmap = this->mBitmap;
    float theInset;
    if (this->mFrameFlags & 4)
        theInset = this->mScaleX * 17.0f;
    else
        theInset = 0.0f;
    GERectangle theCorner;
    theCorner.SetLeft(this->mRectangle.GetLeft() + theInset);
    theCorner.SetRight(this->mScaleX * 7.0f - 1.0f + theCorner.GetLeft());
    theCorner.SetTop(this->mRectangle.GetTop());
    theCorner.SetBottom(this->mScaleY * 7.0f - 1.0f + theCorner.GetTop());
    if (theCorner.And(inRectangle, theClipped) == true)
        this->DrawCorner(theBitmap, sCornerCell, theClipped, theCorner, 0);
    theCorner.SetTop(this->mRectangle.GetBottom() - this->mScaleY * 7.0f + 1.0f);
    theCorner.SetBottom(this->mRectangle.GetBottom());
    if (theCorner.And(inRectangle, theClipped) == true)
        this->DrawCorner(theBitmap, sCornerCell, theClipped, theCorner, 1);
    theCorner.SetLeft(this->mRectangle.GetRight() - this->mScaleX * 7.0f - theInset + 1.0f);
    theCorner.SetRight(this->mRectangle.GetRight() - theInset);
    if (theCorner.And(inRectangle, theClipped) == true)
        this->DrawCorner(theBitmap, sCornerCell, theClipped, theCorner, 2);
    theCorner.SetTop(this->mRectangle.GetTop());
    theCorner.SetBottom(this->mScaleY * 7.0f - 1.0f + this->mRectangle.GetTop());
    if (theCorner.And(inRectangle, theClipped) == true)
        this->DrawCorner(theBitmap, sCornerCell, theClipped, theCorner, 3);
}

// FUNCTION: EXE 0x654c5d
// Draws the frame's four edges (the top and bottom ones rotated), clipped to the redraw rectangle
void EEUIBGenericBackground::DrawEdges(const GERectangle& inRectangle)
{
    GERectangle theClipped;
    float theInset;
    if (this->mFrameFlags & 4)
        theInset = this->mScaleX * 18.0f - 1.0f;
    else
        theInset = 0.0f;
    GERectangle theEdge;
    theEdge.SetLeft(this->mRectangle.GetLeft() + theInset);
    theEdge.SetRight(this->mScaleX * 7.0f - 1.0f + theEdge.GetLeft());
    theEdge.SetTop(this->mRectangle.GetTop() + this->mScaleY * 7.0f);
    theEdge.SetBottom(this->mRectangle.GetBottom() - this->mScaleY * 7.0f);
    if (theEdge.And(inRectangle, theClipped) == true)
        this->DrawEdge(theClipped, theEdge, false, false, false);
    theEdge.SetLeft(this->mRectangle.GetRight() - (this->mScaleX * 7.0f - 1.0f) - theInset);
    theEdge.SetRight(this->mRectangle.GetRight() - theInset);
    if (theEdge.And(inRectangle, theClipped) == true)
        this->DrawEdge(theClipped, theEdge, false, false, true);
    theEdge.SetLeft(this->mRectangle.GetLeft() + this->mScaleX * 7.0f + theInset);
    theEdge.SetRight(this->mRectangle.GetRight() - theInset - this->mScaleX * 7.0f);
    theEdge.SetTop(this->mRectangle.GetTop());
    theEdge.SetBottom(this->mScaleY * 7.0f - 1.0f + theEdge.GetTop());
    if (theEdge.And(inRectangle, theClipped) == true)
        this->DrawEdge(theClipped, theEdge, true, false, false);
    theEdge.SetTop(this->mRectangle.GetBottom() - (this->mScaleY * 7.0f - 1.0f));
    theEdge.SetBottom(this->mRectangle.GetBottom());
    if (theEdge.And(inRectangle, theClipped) == true)
        this->DrawEdge(theClipped, theEdge, true, true, false);
}

// FUNCTION: EXE 0x654e05
// Draws the frame's four large corners, clipped to the redraw rectangle
void EEUIBGenericBackground::DrawFrameCorners(const GERectangle& inRectangle)
{
    GERectangle theClipped;
    GEBitmap* theBitmap = this->mBitmap;
    float theInset;
    if (this->mFrameFlags & 4)
        theInset = this->mScaleX * 18.0f - 1.0f;
    else
        theInset = 0.0f;
    GERectangle theCorner;
    theCorner.SetLeft(this->mRectangle.GetLeft() + theInset);
    theCorner.SetRight(this->mScaleX * 28.0f - 1.0f + theCorner.GetLeft());
    theCorner.SetTop(this->mRectangle.GetTop());
    theCorner.SetBottom(this->mScaleY * 38.0f - 1.0f + theCorner.GetTop());
    if (theCorner.And(inRectangle, theClipped) == true)
        this->DrawCorner(theBitmap, sFrameCornerCell, theClipped, theCorner, 0);
    theCorner.SetTop(this->mRectangle.GetBottom() - this->mScaleY * 38.0f + 1.0f);
    theCorner.SetBottom(this->mRectangle.GetBottom());
    if (theCorner.And(inRectangle, theClipped) == true)
        this->DrawCorner(theBitmap, sFrameCornerCell, theClipped, theCorner, 1);
    theCorner.SetLeft(this->mRectangle.GetRight() - this->mScaleX * 28.0f + 1.0f - theInset);
    theCorner.SetRight(this->mRectangle.GetRight() - theInset);
    if (theCorner.And(inRectangle, theClipped) == true)
        this->DrawCorner(theBitmap, sFrameCornerCell, theClipped, theCorner, 2);
    theCorner.SetTop(this->mRectangle.GetTop());
    theCorner.SetBottom(this->mScaleY * 38.0f - 1.0f + this->mRectangle.GetTop());
    if (theCorner.And(inRectangle, theClipped) == true)
        this->DrawCorner(theBitmap, sFrameCornerCell, theClipped, theCorner, 3);
}

// FUNCTION: EXE 0x654f98
// Draws the two side ornaments at mid height, clipped to the redraw rectangle
// NEAR: the first GetTopLeft expands the float GE2DPoint ctor in the orig (ours calls the import);
// the stand-in that made it exact before G1 no longer does at any n/position (inline-budget knife edge)
void EEUIBGenericBackground::DrawSides(const GERectangle& inRectangle)
{
    GERectangle theSide;
    GERectangle theClipped;
    GEBitmap* theBitmap = this->mBitmap;
    unsigned long theScreen = this->GetForm()->GetManager()->GetScreen();
    theSide.SetLeft(this->mRectangle.GetLeft());
    theSide.SetRight(this->mScaleX * 18.0f - 1.0f + theSide.GetLeft());
    theSide.SetTop(this->mRectangle.GetHeight() * 0.5f + this->mRectangle.GetTop() - this->mScaleY * 10.0f);
    theSide.SetBottom(this->mScaleY * 20.0f - 1.0f + theSide.GetTop());
    if (theSide.And(inRectangle, theClipped) == true) {
        theBitmap->ComputeStretchedCoordsAndScale(theSide, theClipped, sSideCell, theScreen);
        theBitmap->SetLocation(theClipped.GetTopLeft());
        this->GetForm()->GetManager()->GetRasterizer()->Draw(theBitmap, theScreen);
    }
    theSide.SetLeft(this->GetRectangle().GetRight() - this->mScaleX * 18.0f + 1.0f);
    theSide.SetRight(this->GetRectangle().GetRight());
    if (theSide.And(inRectangle, theClipped) != true)
        return;
    theBitmap->ComputeStretchedCoordsAndScale(theSide, theClipped, sSideCell, theScreen);
    theBitmap->SetLocation(theClipped.GetTopLeft());
    this->GetForm()->GetManager()->GetRasterizer()->Draw(theBitmap, theScreen);
}

// FUNCTION: EXE 0x655131
// Draws the tinted background inside the side insets, then the frame pieces its flags ask for
// NEAR: the original also re-stores theInner's left and top before the +1 (as a GERectangle(l, t, r + 1, b + 1)
// built in theInner's own slots); SetLeft(GetLeft())/SetTop(GetTop()) makes it exact but is filler (rejected)
long EEUIBGenericBackground::DrawBackground(const GERectangle& inRectangle)
{
    GERhombus theRhombus;
    GERectangle theInner;
    GERectangle theRectangle = this->mRectangle;
    float theInset;
    if (this->mFrameFlags & 4)
        theInset = 18.0f;
    else
        theInset = 0.0f;
    theRectangle.And(inRectangle);
    theRhombus.SetOpacity(this->mOpacity);
    theRhombus.SetColor(this->mColor);
    theInner = this->mRectangle;
    theInner.SetLeft(theInner.GetLeft() + theInset);
    theInner.SetRight(theInner.GetRight() - theInset);
    theInner.And(inRectangle);
    theInner.SetRight(theInner.GetRight() + 1.0f);
    theInner.SetBottom(theInner.GetBottom() + 1.0f);
    theRhombus.SetRectangle(theInner);
    long theResult = this->GetForm()->GetManager()->GetRasterizer()->Draw(&theRhombus,
                                                                    this->GetForm()->GetManager()->GetScreen());
    if (this->mFrameFlags & 1) {
        this->DrawEdges(theRectangle);
        if (!(this->mFrameFlags & 2))
            this->DrawCorners(theRectangle);
    }
    if (this->mFrameFlags & 2)
        this->DrawFrameCorners(theRectangle);
    if (this->mFrameFlags & 4)
        this->DrawSides(theRectangle);
    return theResult;
}

// FUNCTION: EXE 0x65528a
// Draws one stretched edge piece, rotated and/or flipped as asked
// NEAR: inline budget (orig refuses the GE2DPoint copy ctor in SetLocation(GetTopLeft()) and SetXYScale's body differs) and the rotated rectangle's store order
void EEUIBGenericBackground::DrawEdge(const GERectangle& inDestination, const GERectangle& inTexture,
                                      bool inRotate, bool inFlipVertically, bool inFlipHorizontally)
{
    GETextureCoordinate theTopLeft;
    GETextureCoordinate theTopRight;
    GERectangle theDestination = inDestination;
    GETextureCoordinate theBottomRight;
    GETextureCoordinate theBottomLeft;
    GERectangle theTexture = inTexture;
    GEBitmap* theBitmap = this->mBitmap;
    unsigned long theScreen = this->GetForm()->GetManager()->GetScreen();
    if (inRotate == true) {
        theDestination.SetLeft(inTexture.GetBottom() - inDestination.GetBottom() + inTexture.GetTop());
        theDestination.SetRight(inTexture.GetBottom() - inDestination.GetTop() + inTexture.GetTop());
        theDestination.SetTop(inDestination.GetLeft());
        theDestination.SetBottom(inDestination.GetRight());
        theTexture.SetTop(inTexture.GetLeft());
        theTexture.SetBottom(inTexture.GetRight());
        theTexture.SetLeft(inTexture.GetTop());
        theTexture.SetRight(inTexture.GetBottom());
        theBitmap->ComputeStretchedCoordsAndScale(theTexture, theDestination, sEdgeCell, theScreen);
        theBitmap->GetTextureCoordinatesPerVertex(theTopLeft, theTopRight, theBottomRight, theBottomLeft);
        theBitmap->SetTextureCoordinates(theTopRight, theBottomLeft, theTopLeft, theBottomRight);
        theBitmap->SetXYScale(theBitmap->GetYScale(), theBitmap->GetXScale());
    } else
        theBitmap->ComputeStretchedCoordsAndScale(inTexture, theDestination, sEdgeCell, theScreen);
    if (inFlipHorizontally) {
        theBitmap->GetTextureCoordinatesPerVertex(theTopLeft, theTopRight, theBottomRight, theBottomLeft);
        theBitmap->SetTextureCoordinates(theTopRight, theTopLeft, theBottomLeft, theBottomRight);
    }
    if (inFlipVertically) {
        theBitmap->GetTextureCoordinatesPerVertex(theTopLeft, theTopRight, theBottomRight, theBottomLeft);
        theBitmap->SetTextureCoordinates(theBottomRight, theBottomLeft, theTopLeft, theTopRight);
    }
    theBitmap->SetLocation(inDestination.GetTopLeft());
    this->GetForm()->GetManager()->GetRasterizer()->Draw(theBitmap, theScreen);
}

// FUNCTION: EXE 0x655463
// Draws one corner piece, mirrored into the corner given by the rotation (0..3)
// NEAR: inline budget (orig refuses the GE2DPoint copy ctor in SetLocation(GetTopLeft()): 8 more frame bytes) and store order of the rotated rectangle
void EEUIBGenericBackground::DrawCorner(GEBitmap* inBitmap, const GERectangle& inCell,
                                        const GERectangle& inDestination, const GERectangle& inCorner,
                                        long inRotation)
{
    GETextureCoordinate theTopLeft;
    GETextureCoordinate theTopRight;
    GERectangle theDestination = inDestination;
    GETextureCoordinate theBottomRight;
    GETextureCoordinate theBottomLeft;
    unsigned long theScreen = this->GetForm()->GetManager()->GetScreen();
    switch (inRotation) {
    case 0:
        inBitmap->ComputeStretchedCoordsAndScale(inCorner, theDestination, inCell, theScreen);
        break;
    case 1:
        theDestination.SetTop(inCorner.GetBottom() - inDestination.GetBottom() + inCorner.GetTop());
        theDestination.SetBottom(inCorner.GetBottom() - inDestination.GetTop() + inCorner.GetTop());
        inBitmap->ComputeStretchedCoordsAndScale(inCorner, theDestination, inCell, theScreen);
        inBitmap->GetTextureCoordinatesPerVertex(theTopLeft, theTopRight, theBottomRight, theBottomLeft);
        inBitmap->SetTextureCoordinates(theBottomRight, theBottomLeft, theTopLeft, theTopRight);
        break;
    case 2:
        theDestination.SetLeft(inCorner.GetRight() - inDestination.GetRight() + inCorner.GetLeft());
        theDestination.SetRight(inCorner.GetRight() - inDestination.GetLeft() + inCorner.GetLeft());
        theDestination.SetTop(inCorner.GetBottom() - inDestination.GetBottom() + inCorner.GetTop());
        theDestination.SetBottom(inCorner.GetBottom() - inDestination.GetTop() + inCorner.GetTop());
        inBitmap->ComputeStretchedCoordsAndScale(inCorner, theDestination, inCell, theScreen);
        inBitmap->GetTextureCoordinatesPerVertex(theTopLeft, theTopRight, theBottomRight, theBottomLeft);
        inBitmap->SetTextureCoordinates(theBottomLeft, theBottomRight, theTopRight, theTopLeft);
        break;
    case 3:
        theDestination.SetLeft(inCorner.GetRight() - inDestination.GetRight() + inCorner.GetLeft());
        theDestination.SetRight(inCorner.GetRight() - inDestination.GetLeft() + inCorner.GetLeft());
        inBitmap->ComputeStretchedCoordsAndScale(inCorner, theDestination, inCell, theScreen);
        inBitmap->GetTextureCoordinatesPerVertex(theTopLeft, theTopRight, theBottomRight, theBottomLeft);
        inBitmap->SetTextureCoordinates(theTopRight, theTopLeft, theBottomLeft, theBottomRight);
        break;
    }
    inBitmap->SetLocation(inDestination.GetTopLeft());
    this->GetForm()->GetManager()->GetRasterizer()->Draw(inBitmap, theScreen);
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
// Loads the campaign map image (TGA or JPG) and cuts it into 256x256 textures, one per background cell
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
    theResult = EEOpenSaveGameFile(this->mTextureFileName, UString(gU4E5B98), theFile);
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
    if (_stricmp(theExtension, ".tga") == 0) {
        theResult = GESurface::GetImageFileAttributes(*theFile, kImageFileTGA, theDescription.mWidth, theDescription.mHeight,
                                                      theBits);
        if (theResult != 0)
            goto done;
        theScreen->CreateSurface(theDescription, &theImage);
        theResult = theImage->Load(*theFile, kImageFileTGA);
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

