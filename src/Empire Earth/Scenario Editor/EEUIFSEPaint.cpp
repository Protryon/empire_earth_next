// C:\EEX\Empire Earth\Scenario Editor\EEUIFSEPaint.cpp (file name inferred: no __FILE__). The brush paint area the
// terrain and elevation tabs derive from: 0x1a3f10..0x1a4470. Unit flags: the exe module set.
#include <windows.h>
#include "lle/LowLevelEngine.h"
#include "lle/ESChecksum.h"
#include "lle/FSFile.h"
#include "lle/FSBasicFile.h"
#include "lle/FSFileSpec.h"
#include "lle/FSMemoryFile.h"
#include "lle/GE2DPoint.h"
#include "lle/GEColor.h"
#include "lle/GE3DLine.h"
#include "lle/GE3DPlane.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVertexArray.h"
#include "lle/GEOrientation.h"
#include "lle/GERectangle.h"
#include "lle/GERhombus.h"
#include "lle/GETransformation.h"
#include "lle/GEVector.h"
#include "lle/ISEntity.h"
#include "lle/PIMModule.h"
#include "lle/NEProtocolConfiguration.h"
#include "lle/TSCriticalSection.h"
#include "lle/SEConductor.h"
#include "lle/SEMixer.h"
#include "lle/SESample.h"
#include "lle/TSEvent.h"
#include "lle/TSReadWriteLock.h"
#include "lle/TSThread.h"
#include "lle/TSScheduler.h"
#include "lle/UIEntity.h"
#include "lle/UIControl.h"
#include "lle/UMFastMath.h"
#include "lle/URandom.h"
#include "lle/UString.h"
#include "lle/UWideString.h"
#include "lle/GEViewport.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GETerrainMesh.h"
#include "lle/TMPoll.h"
#include "ee/EmpireEarth.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEMap.h"
#include "ee/EEAction.h"
#include "ee/EEUserInterface.h"
#include "ee/EEAScroll.h"
#include "ee/EEUIFSEBrushGroup.h"
#include "ee/EESEAPaint.h"
#include "ee/EEGraphics.h"
#include "GEWindow.h"
#include "lle/GETextureManager.h"
#include "lle/GESurface.h"
#include "lle/GETexture.h"
#include "lle/GEScreen.h"
#include "lle/GERasterizer.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"
#include "ee/EEUIManager.h"
#include "ee/EECivilization.h"
#include "ee/EEUIFScenarioEditor.h"
#include "ee/EEUIFSEPaint.h"
#include "ee/EEMouse.h"

// FUNCTION: EXE 0x5a3f10
// Creates the paint area over the game view, with no paint action in progress
EEUIFSEPaint::EEUIFSEPaint(EEAScroll* inScroll, EEUIFSEBrushGroup* inBrushes,
                         unsigned long inId, const GE2DPoint& inLocation,
                         unsigned long inWidth, unsigned long inHeight,
                         UIForm* inForm, unsigned long inIndex)
    : UIControl(kControlTypeCustom, inId, GERectangle(inLocation, (float)inWidth, (float)inHeight), inForm, inIndex, 0)
{
    this->mBrushes = inBrushes;
    this->mScroll = inScroll;
    this->mPac = NULL;
    this->mLastTileX = 0;
    this->mLastTileY = 0;
    this->mBrushRadius = 0;
    this->mIsCentered = false;
    this->SetAlwaysDirty(true);
}

// SYNTHETIC: EXE 0x5a3fb8
// (the scalar deleting destructor, emitted with the vtable)

// FUNCTION: EXE 0x5a3fd4
// Destroys the paint area, releasing the paint action in progress
EEUIFSEPaint::~EEUIFSEPaint()
{
    if (this->mPac != NULL) {
        this->mPac->Dereference();
    }
}

// FUNCTION: EXE 0x5a3ff7
// Remembers the tile under the mouse as the last painted tile, if the mouse is on one
void EEUIFSEPaint::StoreMouseTile()
{
    EETile* theTile;
    long theX;
    long theY;
    if (EEPointUnitOrTileXY(this->GetMouseLocation(), theTile, theX, theY, false)) {
        this->mLastTileX = theX;
        this->mLastTileY = theY;
    }
}

// FUNCTION: EXE 0x5a4036
// Repeats the mouse move at the current mouse position, then copies the control's data
long EEUIFSEPaint::CopyData()
{
    this->MouseMove(gMouse->mPosition);
    return UIControl::CopyData();
}

// FUNCTION: EXE 0x5a4054
// Starts painting: captures the mouse inside the game view, creates the tab's paint action and paints the brush
// square around the clicked tile
long EEUIFSEPaint::MouseLButtonDown()
{
    GE3DPoint thePoint;
    GERectangle theBounds = gViewport.GetViewRectangle();
    EETile* theTile;
    this->CaptureMouse();
    EEPointUnitOrTile(this->GetMouseLocation(), theTile, thePoint, false);
    float theXPosition = thePoint.GetX();
    float theYPosition = thePoint.GetY();
    gMouse->SetBounds(theBounds);
    this->mScroll->mEdges = GERectangle(theBounds.GetLeft(), theBounds.GetTop(),
                                theBounds.GetRight() - 1.0f, theBounds.GetBottom() - 1.0f);
    EESEAPaint* thePaint = this->CreatePaintAction();
    this->mPac = thePaint;
    long theX;
    long theY;
    if (this->mIsCentered) {
        theX = (long)(theXPosition - 0.5f);
        theY = (long)(theYPosition - 0.5f);
        theTile = gWorld.mMap->GetTile(theX, theY);
    } else {
        theX = (long)theXPosition;
        theY = (long)theYPosition;
    }
    if (theTile != NULL) {
        this->mBrushRadius = this->mBrushes->mBrushSize;
        if (thePaint->SetArea(theX - this->mBrushRadius, theY + this->mBrushRadius, theX + this->mBrushRadius, theY - this->mBrushRadius)) {
            gUserInterface->PostAction(this->mPac);
        }
    }
    this->mLastTileX = theX;
    this->mLastTileY = theY;
    return 0;
}

// FUNCTION: EXE 0x5a41cd
// Stops painting: releases the mouse and the paint action and restores the full-window mouse and scroll bounds
long EEUIFSEPaint::MouseLButtonUp()
{
    GERectangle theBounds(0.0f, 0.0f, (float)gGameWindow->GetWidth(), (float)gGameWindow->GetHeight());
    this->ReleaseCapturedMouse();
    if (this->mPac != NULL) {
        this->mPac->Dereference();
        this->mPac = NULL;
    }
    gMouse->SetBounds(theBounds);
    this->mScroll->mEdges = GERectangle(0.0f, 0.0f, theBounds.GetRight() - 1.0f, theBounds.GetBottom() - 1.0f);
    return 0;
}

// FUNCTION: EXE 0x5a4265
// While painting, paints the brush along the line from the last tile to the tile under the mouse; always
// moves the brush highlight
long EEUIFSEPaint::MouseMove(const GE2DPoint& inLocation)
{
    GE2DPoint theFrom;
    GE3DPoint thePoint;
    std::vector<GE2DPoint> thePoints;
    bool theChanged = false;
    EEMap* theMap = gWorld.mMap;
    if (this->mPac == NULL)
        return 0;
    EETile* theTile;
    EEPointUnitOrTile(inLocation, theTile, thePoint, false);
    float thePointX = thePoint.GetX();
    float thePointY = thePoint.GetY();
    long theX;
    long theY;
    if (this->mIsCentered) {
        theX = (long)(thePoint.GetX() - 0.5f);
        theY = (long)(thePoint.GetY() - 0.5f);
        theTile = theMap->GetTile(theX, theY);
    } else {
        theX = (long)thePoint.GetX();
        theY = (long)thePoint.GetY();
    }
    if (theTile != NULL && (theX != this->mLastTileX || theY != this->mLastTileY)) {
        theFrom.SetX((float)this->mLastTileX);
        theFrom.SetY((float)this->mLastTileY);
        GE2DPoint theTo((float)theX, (float)theY);
        theFrom.InterpolateTo(theTo, 1.0f, thePoints);
        for (unsigned long i = 1; i < thePoints.size(); i++) {
            theX = (long)thePoints[i].GetX();
            theY = (long)thePoints[i].GetY();
            if (this->mPac->SetArea(theX - this->mBrushRadius, theY + this->mBrushRadius, theX + this->mBrushRadius, theY - this->mBrushRadius))
                theChanged = true;
        }
        if (theChanged)
            gUserInterface->PostAction(this->mPac);
    }
    this->mLastTileX = theX;
    this->mLastTileY = theY;
    gUIManager->mScenarioEditorForm->mBrushGroup->SetHighlight(thePointX, thePointY, this->mIsCentered);
    return 0;
}
