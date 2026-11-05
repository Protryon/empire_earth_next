// C:\EEX\Empire Earth\User Interface Thread\EEUICSelectUnits.cpp (file name inferred: no __FILE__; RTTI class
// EEUICSelectUnits). The main game form's left-drag selection control: 0x25952f..0x259f7f (the two GEColor `$E`
// chains, ctor, ??_G, dtor, then the members in alphabetical order; EEUICUnitIconBar's ctor at 0x259f7f). Unit flags:
// the exe module set.
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
#include "ee/EEDatabaseTables.h"
#include "ee/EEGraphics.h"
#include "ee/EEMouse.h"
#include "ee/EEUnitPicker.h"
#include "ee/EEAction.h"
#include "ee/EEAMouseClick.h"
#include "ee/EEALasso.h"
#include "lle/GELine.h"
#include "ee/EEUICSelectUnits.h"


static GEColor sSelectBoxColor(1.0f, 1.0f, 1.0f);   // 0x564580
static GEColor sSelectShadowColor(0.0f, 0.0f, 0.0f);    // 0x564570

// SYNTHETIC: EXE 0x65952f
// SYNTHETIC: EXE 0x659539
// SYNTHETIC: EXE 0x659552
// SYNTHETIC: EXE 0x65955e
// SYNTHETIC: EXE 0x65955f
// SYNTHETIC: EXE 0x659569
// SYNTHETIC: EXE 0x659582
// SYNTHETIC: EXE 0x65958e

// FUNCTION: EXE 0x65958f
// Creates the main game view's drag-selection box (white lines over black shadow lines), hidden
EEUICSelectUnits::EEUICSelectUnits(UIForm* inForm, unsigned long inId, unsigned long inIndex, EEAMouseClick* inClick,
                                   EEALasso* inLasso)
    : UIControl(kControlTypeCustom, inId, GERectangle(GE2DPoint(gViewport.GetViewRectangle().GetTopLeft()), 2.0f, 2.0f), inForm,
                inIndex, 0),
      mClick(inClick)
{
    this->mDisplayMode = kDisplayModeHidden;
    this->mDragging = false;
    this->mD1 = false;
    this->mClick->Reference();
    this->mLasso = inLasso;
    this->mLasso->Reference();
    this->SetAlwaysDirty(true);
    this->mLines = new GELine[8];
    this->mLines[0].SetColor(sSelectShadowColor);
    this->mLines[1].SetColor(sSelectShadowColor);
    this->mLines[2].SetColor(sSelectShadowColor);
    this->mLines[3].SetColor(sSelectShadowColor);
    this->mLines[4].SetColor(sSelectBoxColor);
    this->mLines[5].SetColor(sSelectBoxColor);
    this->mLines[6].SetColor(sSelectBoxColor);
    this->mLines[7].SetColor(sSelectBoxColor);
}

// SYNTHETIC: EXE 0x65971d
// (the scalar deleting destructor, emitted with the vtable; slot EEUICSelectUnits+0x0)

// FUNCTION: EXE 0x659739
// Destroys the selection box, releasing its click and lasso handlers
EEUICSelectUnits::~EEUICSelectUnits()
{
    this->mClick->Dereference();
    this->mLasso->Dereference();
    delete[] this->mLines;
}

// FUNCTION: EXE 0x659775
// Snapshots the box for drawing while dragging, clipped to the game view, and redraws what it covers
long EEUICSelectUnits::CopyData()
{
    if (this->mDragging) {
        this->mRA0 = this->GetRectangle();
        this->mRA0.And(gViewport.GetViewRectangle());
        this->GetForm()->GetManager()->AddDirtyRectangle(this->mRA0, this->GetForm());
        this->GetForm()->RedrawIntersectControls(this->mRA0);
        this->mD1 = true;
    } else
        this->mD1 = false;
    return UIControl::CopyData();
}

// FUNCTION: EXE 0x6597e2
// Draws the selection rectangle as four shadow lines and four white lines
long EEUICSelectUnits::DrawPolygons()
{
    if (this->mD1) {
        this->mLines[0].SetStartPoint(GE2DPoint(this->mRA0.GetLeft() + 1.0f, this->mRA0.GetTop() + 1.0f));
        this->mLines[0].SetEndPoint(GE2DPoint(this->mRA0.GetRight() + 1.0f, this->mRA0.GetTop() + 1.0f));
        this->mLines[1].SetStartPoint(GE2DPoint(this->mRA0.GetLeft() + 1.0f, this->mRA0.GetTop() + 1.0f));
        this->mLines[1].SetEndPoint(GE2DPoint(this->mRA0.GetLeft() + 1.0f, this->mRA0.GetBottom() + 1.0f));
        this->mLines[2].SetStartPoint(GE2DPoint(this->mRA0.GetLeft() + 1.0f, this->mRA0.GetBottom()));
        this->mLines[2].SetEndPoint(GE2DPoint(this->mRA0.GetRight() + 1.0f, this->mRA0.GetBottom()));
        this->mLines[3].SetStartPoint(GE2DPoint(this->mRA0.GetRight() + 1.0f, this->mRA0.GetTop() + 1.0f));
        this->mLines[3].SetEndPoint(GE2DPoint(this->mRA0.GetRight() + 1.0f, this->mRA0.GetBottom() + 1.0f));
        this->mLines[4].SetStartPoint(GE2DPoint(this->mRA0.GetLeft(), this->mRA0.GetTop()));
        this->mLines[4].SetEndPoint(GE2DPoint(this->mRA0.GetRight() + 1.0f, this->mRA0.GetTop()));
        this->mLines[5].SetStartPoint(GE2DPoint(this->mRA0.GetLeft(), this->mRA0.GetTop()));
        this->mLines[5].SetEndPoint(GE2DPoint(this->mRA0.GetLeft(), this->mRA0.GetBottom()));
        this->mLines[6].SetStartPoint(GE2DPoint(this->mRA0.GetLeft(), this->mRA0.GetBottom() - 1.0f));
        this->mLines[6].SetEndPoint(GE2DPoint(this->mRA0.GetRight(), this->mRA0.GetBottom() - 1.0f));
        this->mLines[7].SetStartPoint(GE2DPoint(this->mRA0.GetRight(), this->mRA0.GetTop()));
        this->mLines[7].SetEndPoint(GE2DPoint(this->mRA0.GetRight(), this->mRA0.GetBottom()));
        gRasterizer->Draw(this->mLines, 8, this->GetScreenIndex());
    }
    return UIControl::DrawPolygons();
}

// FUNCTION: EXE 0x659af1
// On a double click on one of the player's living units, selects every unit of its type in the view
long EEUICSelectUnits::MouseLButtonDoublePressed()
{
    unsigned long theModifiers = gISKeyboard->GetModifierState();
    long theResult;
    EEUnit* theUnit = gUnitPicker.PickUnit(gMouse->mPosition, 0);
    if (theUnit != NULL && theUnit->GetHitPoints() > 0 && theUnit->GetPlayer() == gWorld.GetCurrentPlayer()) {
        this->mLasso->SelectUnits(gU4E8F08.GetViewRectangle(), theModifiers, theUnit->GetUnitClass()->mUnitType, theUnit->mUnitID);
        gMouse->SetBounds(GERectangle(0.0f, 0.0f, (float)gGameWindow->GetWidth(), (float)gGameWindow->GetHeight()));
        this->SetDisplayMode(kDisplayModeHidden);
        this->ReleaseCapturedMouse();
        this->mDragging = false;
        theResult = 0;
    } else
        theResult = UIControl::MouseLButtonDoublePressed();
    if (theUnit != NULL)
        theUnit->Dereference();
    return theResult;
}

// FUNCTION: EXE 0x659bda
// On release, selects the units in the dragged box, or the unit under the mouse for a plain click; frees the
// mouse
long EEUICSelectUnits::MouseLButtonUp()
{
    GERectangle theRectangle;
    unsigned long theModifiers = gISKeyboard->GetModifierState();
    if (this->mDragging) {
        this->mD1 = false;
        theRectangle.SetTopLeft(this->mDragStart);
        theRectangle.SetBottomRight(this->mDragEnd);
        if (theRectangle.GetLeft() > theRectangle.GetRight()) {
            float theLeft = theRectangle.GetLeft();
            theRectangle.SetLeft(theRectangle.GetRight());
            theRectangle.SetRight(theLeft);
        }
        if (theRectangle.GetTop() > theRectangle.GetBottom()) {
            float theTop = theRectangle.GetTop();
            theRectangle.SetTop(theRectangle.GetBottom());
            theRectangle.SetBottom(theTop);
        }
        this->mLasso->SelectUnits(theRectangle, theModifiers, 0x366, 0);
    } else if (gViewport.GetViewRectangle().Contains(this->GetMouseLocation()))
        this->mClick->SelectAtMouse(theModifiers);
    gMouse->SetBounds(GERectangle(0.0f, 0.0f, (float)gGameWindow->GetWidth(), (float)gGameWindow->GetHeight()));
    this->SetDisplayMode(kDisplayModeHidden);
    this->ReleaseCapturedMouse();
    this->mDragging = false;
    return 0;
}

// FUNCTION: EXE 0x659cfc
// Extends the drag box to the mouse, clamped to the game view; dragging starts after 10 pixels
long EEUICSelectUnits::MouseMove(const GE2DPoint& inLocation)
{
    GERectangle theRectangle;
    GE2DPoint theOld = this->mDragEnd;
    if (gViewport.GetViewRectangle().Contains(inLocation)) {
        this->mDragEnd = inLocation;
        this->mDragEnd.SetX((float)UMFloatToLong(this->mDragEnd.GetX()));
        this->mDragEnd.SetY((float)UMFloatToLong(this->mDragEnd.GetY()));
    } else {
        float theY;
        float theX;
        if (inLocation.GetY() < gViewport.GetViewRectangle().GetTop())
            theY = gViewport.GetViewRectangle().GetTop();
        else if (inLocation.GetY() > gViewport.GetViewRectangle().GetBottom() - 1.0f)
            theY = gViewport.GetViewRectangle().GetBottom() - 1.0f;
        else
            theY = inLocation.GetY();
        if (inLocation.GetX() < gViewport.GetViewRectangle().GetLeft())
            theX = gViewport.GetViewRectangle().GetLeft();
        else if (inLocation.GetX() > gViewport.GetViewRectangle().GetRight())
            theX = gViewport.GetViewRectangle().GetRight();
        else
            theX = inLocation.GetX();
        this->mDragEnd.SetX((float)UMFloatToLong(theX));
        this->mDragEnd.SetY((float)UMFloatToLong(theY));
    }
    if (!this->mDragging && this->GetDisplayMode() == kDisplayModeShown && this->mDragStart.DistanceTo(this->mDragEnd) >= 10.0f)
        this->mDragging = true;
    if (theOld == this->mDragEnd)
        return 0;
    if (this->mDragStart.GetX() < this->mDragEnd.GetX()) {
        theRectangle.SetLeft(this->mDragStart.GetX());
        theRectangle.SetRight(this->mDragEnd.GetX());
    } else {
        theRectangle.SetRight(this->mDragStart.GetX());
        theRectangle.SetLeft(this->mDragEnd.GetX());
    }
    if (this->mDragStart.GetY() < this->mDragEnd.GetY()) {
        theRectangle.SetTop(this->mDragStart.GetY());
        theRectangle.SetBottom(this->mDragEnd.GetY());
    } else {
        theRectangle.SetBottom(this->mDragStart.GetY());
        theRectangle.SetTop(this->mDragEnd.GetY());
    }
    this->SetRectangle(theRectangle);
    this->GetForm()->GetManager()->AddDirtyRectangle(this->GetRectangle(), this->GetForm());
    this->GetForm()->RedrawIntersectControls(this->GetRectangle());
    return 0;
}

// FUNCTION: EXE 0x659ec5
// Starts a drag selection at a point, confining the mouse to the game view
void EEUICSelectUnits::StartSelection(const GE2DPoint& inLocation)
{
    this->mDragStart = inLocation;
    this->mDragStart.SetX((float)UMFloatToLong(this->mDragStart.GetX()));
    this->mDragStart.SetY((float)UMFloatToLong(this->mDragStart.GetY()));
    gMouse->SetBounds(gViewport.GetViewRectangle());
    this->SetRectangle(GERectangle(this->mDragStart, this->mDragStart));
    this->CaptureKeyboard();
    this->CaptureMouse();
    this->ReleaseCapturedKeyboard();
    this->SetDisplayMode(kDisplayModeShown);
    this->UpdateMouse(inLocation);
}
