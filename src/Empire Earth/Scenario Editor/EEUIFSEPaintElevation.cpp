// C:\EEX\Empire Earth\Scenario Editor\EEUIFSEPaintElevation.cpp (file name inferred: no __FILE__; RTTI class
// EEUIFSEPaintElevation). The elevation tab's brush control: 0x1a4470..0x1a4c00 (ctor, ??_G, dtor, Virtual88, then
// EESEAPaintElevation's inline ctor/??_G/dtor at their first user, the mouse handlers). After EEUIFSEPaint.cpp
// (0x1a3f10), before EEUIFSERBBrush.cpp (0x1a4c00). Unit flags: the exe module set.
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
#include "lle/FSPath.h"
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
#include "lle/GELine.h"
#include "lle/UITextLabel.h"
#include "lle/UIEditBox.h"
#include "lle/UIListBox.h"
#include "lle/GEFont.h"
#include "lle/GEText.h"
#include "lle/UILBString.h"
#include "lle/UIComboBox.h"
#include "lle/UIFileComboBox.h"
#include "lle/UIFormManager.h"
#include "lle/UResourceFile.h"
#include "lle/UISlider.h"
#include "lle/UICheckBox.h"

#include "lle/UIRadioButton.h"
#include "lle/UITriStateCheckBox.h"
#include <vector>
#include <deque>

#include "ee/EmpireEarth.h"
#include "ee/EEAction.h"
#include "ee/EEUIManager.h"
#include "ee/EEUserInterface.h"
#include "ee/EECivilization.h"
#include "ee/EEUIFScenarioEditor.h"
#include "ee/EEUIFSEMainGroup.h"
#include "ee/EESMEffect.h"
#include "ee/EESETSBaseObject.h"
#include "ee/EESETriggerScreen.h"
#include "ee/EESEEChangeEffectAction.h"
#include "ee/EESEEGCPlace.h"
#include "ee/EESEEPlace.h"
#include "ee/EESEEvents.h"
#include "lle/USingleton.h"
#include "ee/EECalamityClass.h"
#include "ee/EECalamityManager.h"
#include "ee/EEMedia.h"
#include "ee/EEGraphicEffect.h"
#include "ee/EEScenarioManager.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EESETSEScenarioComboBox.h"
#include "ee/EESETSUICBPlayer.h"
#include "ee/EESETSEffectControls.h"
#include "ee/EESETSEffectType.h"
#include "lle/GE3DPoint.h"
#include "lle/GE3DLine.h"
#include "lle/GETransformation.h"
#include "lle/GETerrainMesh.h"
#include "ee/EEMap.h"
#include "ee/EEMouse.h"
#include "ee/EEGraphics.h"
#include "ee/EEAScroll.h"
#include "ee/EEUIFSEBrushGroup.h"
#include "ee/EESMArea.h"
#include "ee/EEUnit.h"
#include "ee/EEUnitSetManager.h"
#include "ee/EESMObject.h"
#include "ee/EESEPlayerScreen.h"
#include "ee/EEMessage.h"
#include "ee/EESETSConditionControls.h"
#include "ee/EEALasso.h"
#include "ee/EEAMouseClick.h"
#include "ee/EESEALasso.h"
#include "ee/EESEAMouseClick.h"
#include "ee/EESETSAMouseClick.h"
#include "ee/EESETSALasso.h"
#include "ee/EEUICSelectUnits.h"
#include "ee/EESEUICTriggerSelectUnits.h"
#include "ee/EESETSOUnitList.h"
#include "ee/EESETSAComboBox.h"
#include "ee/EESETSAEditBox.h"
#include "ee/EESETSObject.h"
#include "lle/UIESliderChange.h"
#include "ee/EEUIFSEBrushGroup.h"
#include "ee/EEUIFSEPaint.h"
#include "lle/GE3DPlane.h"
#include "ee/EEUIFSEPaintElevation.h"

// the unit's constants (.rdata 0x4455f8..0x445618): the scales 1.2247449f (0x4455f8), 4.8989797f (0x4455fc) and
// 2.4494898f (0x445614) are float literals (__real COMDATs: the original multiplies the local by them, theT * k);
// only the level height is a named constant
extern const float kSEPELevelHeight = 0.20412414f;     // 0x445618 (EESEAPaintElevation.cpp reads it)

// FUNCTION: EXE 0x5a4470
// Creates the elevation tab's paint area, with no right-button drag in progress
EEUIFSEPaintElevation::EEUIFSEPaintElevation(EEAScroll* inScroll, EEUIFSEBrushGroup* inBrushes, unsigned long inId,
                                             const GE2DPoint& inLocation, unsigned long inWidth,
                                             unsigned long inHeight, UIForm* inForm, unsigned long inIndex)
    : EEUIFSEPaint(inScroll, inBrushes, inId, inLocation, inWidth, inHeight, inForm, inIndex), mTileX(0), mTileY(0),
      mLevel(0)
{
}

// SYNTHETIC: EXE 0x5a44c0
// (EEUIFSEPaintElevation's scalar deleting destructor)

// FUNCTION: EXE 0x5a44dc
// Destroys the elevation paint area
EEUIFSEPaintElevation::~EEUIFSEPaintElevation()
{
}

// FUNCTION: EXE 0x5a44e7
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
    EESEAPaint* theAction = new EESEAPaintElevation(theLevel);
    this->SetIsCentered(false);
    return theAction;
}

// FUNCTION: EXE 0x5a46d4
// Creates an elevation paint action for one level (EESEAPaintElevation's inline ctor, out of line after its first user)

// FUNCTION: EXE 0x5a4719
// Destroys an elevation paint action and frees it if asked (scalar deleting destructor, emitted with its vtable)

// FUNCTION: EXE 0x5a4735
// Destroys an elevation paint action (EESEAPaintElevation's inline dtor)

// FUNCTION: EXE 0x5a475d
// Stops painting; if the right button is still down, starts a right-button elevation drag again
long EEUIFSEPaintElevation::MouseLButtonUp()
{
    long theResult = EEUIFSEPaint::MouseLButtonUp();
    if (this->GetRightMouseButtonDown())
        theResult = this->MouseRButtonDown();
    return theResult;
}

// FUNCTION: EXE 0x5a4782
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
            EESEAPaintElevation* theAction = new EESEAPaintElevation((char)theNewLevel);
            if (theAction->SetArea(this->mTileX - this->mBrushSize, this->mTileY + this->mBrushSize, this->mTileX + this->mBrushSize, this->mTileY - this->mBrushSize))
                gUserInterface->PostAction(theAction);
            theAction->Dereference();
        }
    } else
        theResult = EEUIFSEPaint::MouseMove(inLocation);
    return theResult;
}

// FUNCTION: EXE 0x5a4a31
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

// FUNCTION: EXE 0x5a4b6b
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
