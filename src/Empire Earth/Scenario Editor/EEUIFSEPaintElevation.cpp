// C:\EEX\Empire Earth\Scenario Editor\EEUIFSEPaintElevation.cpp (file name inferred: no __FILE__; RTTI class
// EEUIFSEPaintElevation). The elevation tab's brush control: 0x1a4470..0x1a4c00 (ctor, ??_G, dtor, Virtual88, then
// EESEAPaintElevation's inline ctor/??_G/dtor at their first user, the mouse handlers). After EEUIFSEPaint.cpp
// (0x1a3f10), before EEUIFSERBBrush.cpp (0x1a4c00). Unit flags: the exe module set.
#include <windows.h>
#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "GEWindow.h"
#include "lle/GEScreen.h"
#include "lle/GEViewport.h"
#include "lle/GEVertexArray.h"
#include "lle/GERhombus.h"
#include "lle/Input System.h"
#include "lle/UIHotKey.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"
#include "lle/NEMessage.h"
#include "lle/UITextLabel.h"
#include "lle/UIEditBox.h"
#include "lle/UIListBox.h"
#include "lle/GEText.h"
#include "lle/UILBString.h"
#include "lle/UIFileComboBox.h"
#include "lle/UICheckBox.h"

#include "lle/UIRadioButton.h"

#include "ee/EmpireEarth.h"
#include "ee/EEAction.h"
#include "ee/EEUIManager.h"
#include "ee/EEUserInterface.h"
#include "ee/EECivilization.h"
#include "ee/EEUIFScenarioEditor.h"
#include "ee/EEUIFSEMainGroup.h"
#include "ee/EESETSBaseObject.h"
#include "lle/USingleton.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "lle/GETerrainMesh.h"
#include "ee/EEMap.h"
#include "ee/EEMouse.h"
#include "ee/EEGraphics.h"
#include "ee/EEAScroll.h"
#include "ee/EEMessage.h"
#include "ee/EEALasso.h"
#include "ee/EEAMouseClick.h"
#include "ee/EESEALasso.h"
#include "ee/EEUIFSEBrushGroup.h"
#include "ee/EEUIFSEPaint.h"
#include "ee/EEUIFSEPaintElevation.h"

// the unit's constants (.rdata 0x4455f8..0x445618): the scales 1.2247449f (0x4455f8), 4.8989797f (0x4455fc) and
// 2.4494898f (0x445614) are float literals (__real COMDATs: the original multiplies the local by them, theT * k);
// only the level height is a named constant
extern const float kSEPELevelHeight = 0.20412414f;     // 0x445618 (EESEAPaintElevation.cpp reads it)

// Creates the elevation tab's paint area, with no right-button drag in progress
EEUIFSEPaintElevation::EEUIFSEPaintElevation(EEAScroll* inScroll, EEUIFSEBrushGroup* inBrushes, unsigned long inId,
                                             const GE2DPoint& inLocation, unsigned long inWidth,
                                             unsigned long inHeight, UIForm* inForm, unsigned long inIndex)
    : EEUIFSEPaint(inScroll, inBrushes, inId, inLocation, inWidth, inHeight, inForm, inIndex), mTileX(0), mTileY(0),
      mLevel(0)
{
}

// Destroys the elevation paint area
EEUIFSEPaintElevation::~EEUIFSEPaintElevation()
{
}

// Creates an elevation paint action for the level to paint: from the dragged tile (right button), the tile under
// the mouse (Space held), or the hills / cliffs slider; the brush is never centered
EESEAPaint* EEUIFSEPaintElevation::CreatePaintAction()
{
    long theX;
    long theY;
    EETile* theTile;
    ISEntity* theSpaceKey;
    char theLevel = 0;
    long theResult = gISKeyboard->GetEntity(kKeySpace, theSpaceKey);

    if (this->GetRightMouseButtonDown() == true) {
        if (static_cast<UIRadioButton*>(gUIManager->mScenarioEditorForm->GetControlById(kSEElevationRaiseButton))->GetIsChecked() == true)
            theLevel = (char)UMFloor(gWorld.GetMap()->GetTile(this->mTileX, this->mTileY)->GetHeight() * 4.8989797f + 0.00001f);
        else if (static_cast<UIRadioButton*>(gUIManager->mScenarioEditorForm->GetControlById(kSEElevationLowerButton))->GetIsChecked() == true)
            theLevel = (char)UMFloor(gWorld.GetMap()->GetTile(this->mTileX, this->mTileY)->GetHeight() * 1.2247449f + 0.00001f);
    } else if (theResult == 0 && theSpaceKey->GetValue() > 0) {
        EEPointUnitOrTileXY(this->GetMouseLocation(), theTile, theX, theY, false);
        theLevel = (char)UMFloor(theTile->GetHeight() / (gWorld.GetMap()->GetF1955F4() * 0.5f) + 0.00001f);
    } else if (static_cast<UIRadioButton*>(this->GetForm()->GetControlById(kSEElevationRaiseButton))->GetIsChecked() == true) {
        UISlider* theSlider = static_cast<UISlider*>(this->GetForm()->GetControlById(kSEElevationElevationSlider));
        theLevel = (char)(theSlider->GetMaxRange() - theSlider->GetCurrentRangeValue() - 0x18);
    } else {
        UISlider* theSlider = static_cast<UISlider*>(this->GetForm()->GetControlById(kSEElevationStrengthSlider));
        theLevel = (char)((float)(theSlider->GetMaxRange() - theSlider->GetCurrentRangeValue()) - 6.0f);
    }
    EESEAPaint* theAction = new EESEAPaintRust(kSEAPaintElevation, theLevel);
    this->SetIsCentered(false);
    return theAction;
}

// Stops painting; if the right button is still down, starts a right-button elevation drag again
long EEUIFSEPaintElevation::MouseLButtonUp()
{
    long theResult = EEUIFSEPaint::MouseLButtonUp();
    if (this->GetRightMouseButtonDown())
        theResult = this->MouseRButtonDown();
    return theResult;
}

// During a right-button drag, raises or lowers the grabbed tile's brush square to the height under the mouse
// (within the level limits); otherwise paints as usual
long EEUIFSEPaintElevation::MouseMove(const GE2DPoint& inLocation)
{
    GE3DLine theLine;
    GE3DPoint thePoint;
    long theResult = 0;
    float theT;
    long theNewLevel = 0;
    long theOldLevel;
    long theCheck;

    if (this->GetHasMouseCapture() && this->GetRightMouseButtonDown() && !this->GetLeftMouseButtonDown()) {
        gWorld.mMap->GetScreenLine(inLocation, theLine);
        theLine.Intersects(this->mPlane, theT);
        theLine.GetPointAt(theT, thePoint);
        theT = (float)this->mLevel * kSEPELevelHeight + thePoint.GetZ() - this->mPlane.GetPoint().GetZ();
        if (static_cast<UIRadioButton*>(gUIManager->mScenarioEditorForm->GetControlById(kSEElevationRaiseButton))->GetIsChecked() == true) {
            theNewLevel = (long)(float)UMFloor(theT * 4.8989797f + 0.00001f);
            theCheck = theNewLevel;
            // the grabbed tile's current level on the same scale as the new one
            theOldLevel =
                (long)(float)UMFloor(gWorld.mMap->GetTile(this->mTileX, this->mTileY)->GetHeight() * 4.8989797f + 0.00001f);
        } else if (static_cast<UIRadioButton*>(gUIManager->mScenarioEditorForm->GetControlById(kSEElevationLowerButton))->GetIsChecked() == true) {
            theNewLevel = (long)(float)UMFloor(theT * 1.2247449f + 0.00001f);
            theCheck = theNewLevel * 4;
            theOldLevel =
                (long)(float)UMFloor(gWorld.mMap->GetTile(this->mTileX, this->mTileY)->GetHeight() * 1.2247449f + 0.00001f);
        } else {
            theOldLevel = 0;
            theCheck = 0;
        }
        if (theNewLevel != theOldLevel && theCheck >= -0x18 && theCheck <= 0x28) {
            EESEAPaint* theAction = new EESEAPaintRust(kSEAPaintElevation, (char)theNewLevel);
            if (theAction->SetArea(this->mTileX - this->mBrushSize, this->mTileY + this->mBrushSize, this->mTileX + this->mBrushSize, this->mTileY - this->mBrushSize))
                gUserInterface->PostAction(theAction);
            theAction->Dereference();
        }
    } else
        theResult = EEUIFSEPaint::MouseMove(inLocation);
    return theResult;
}

// Starts a right-button elevation drag on the tile under the mouse: captures the mouse, remembers the tile,
// its level, the brush size and a vertical drag plane through it
long EEUIFSEPaintElevation::MouseRButtonDown()
{
    GEVector theNormal(1.0f, -1.0f, 0.0f);
    GE3DLine theLine;
    GE3DPoint thePoint;
    GERectangle theRectangle = gViewport.GetViewRectangle();
    EETile* theTile = gWorld.GetMap()->GetLocationAt(this->GetMouseLocation(), thePoint, false);
    if (theTile == NULL)
        return 0;
    this->CaptureMouse();
    gMouse->SetBounds(theRectangle);
    this->mScroll->mEdges = GERectangle(theRectangle.GetLeft(), theRectangle.GetTop(), theRectangle.GetRight() - 1.0f,
                                  theRectangle.GetBottom() - 1.0f);
    this->mTileX = (long)thePoint.GetX();
    this->mTileY = (long)thePoint.GetY();
    this->mLevel = (char)UMFloor(theTile->GetHeight() * 4.8989797f + 0.00001f);
    thePoint.SetZ(theTile->GetHeight());
    this->mPlane.Define(thePoint, theNormal);
    this->mBrushSize = this->mBrushes->mBrushSize;
    return 0;
}

// Ends a right-button elevation drag (unless the left button is down), restoring the mouse and scroll bounds
long EEUIFSEPaintElevation::MouseRButtonUp()
{
    GERectangle theRectangle(0.0f, 0.0f, (float)gGameWindow->GetWidth(), (float)gGameWindow->GetHeight());
    if (!this->GetHasMouseCapture() || this->GetLeftMouseButtonDown())
        return 0;
    this->ReleaseCapturedMouse();
    gMouse->SetBounds(theRectangle);
    this->mScroll->mEdges = GERectangle(0.0f, 0.0f, theRectangle.GetRight() - 1.0f, theRectangle.GetBottom() - 1.0f);
    return 0;
}
