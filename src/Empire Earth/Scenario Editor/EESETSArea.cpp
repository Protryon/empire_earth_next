// C:\EEX\Empire Earth\Scenario Editor\EESETSArea.cpp (file name inferred: no __FILE__; RTTI class EESETSArea).
// The trigger screen's area sub-screen: 0x17ccf3..0x17daa8 (ctor, ??_G, dtor, the slots in vtable order of their
// bodies with the COMDATs of their first uses: EESMArea::SetRectangle/SetAnchor, EEUIFScenarioEditor::SetPaintArea,
// EESEAPaintArea's ctor, EEScenarioManager::GetArea, then the ??_G of the base and of the two events). After
// EESETriggerScreen.cpp, before the condition controls' unit (EESETSCComboBox.cpp 0x17daa8). Unit flags: the exe
// module set.
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
#include "ee/EESEAPaintArea.h"
#include "ee/EESMArea.h"
#include "ee/EESETSArea.h"
#include "ee/EEStringIDs.h"
// FUNCTION: EXE 0x57ccf3
// Creates the area sub-screen: the red 'no area selected' label, the area selection mode combo
// and the center camera button, in rectangle mode with no area drawn
EESETSArea::EESETSArea(GEFont* inFont, UIForm* inForm, unsigned long& ioIndex)
{
    std::vector<UWideString> theNames;
    UWideString theText;
    GE2DPoint thePosition(329.0f, 580.0f);
    gResource->GetString(IDS_NO_AREA_SELECTED, theText);
    this->mLabel = new UITextLabel(theText, inFont, 0x89b, GERectangle(thePosition, 250.0f, 20.0f), inForm, 0,
                             ioIndex, 0);
    this->mLabel->SetBgOpacity(0.4f);
    this->mLabel->SetAlignment(kTextAlignCenter);
    GEText* theGEText = this->mLabel->GetGEText();
    theGEText->SetFgColor(GEColor(1.0f, 0.0f, 0.0f));
    theGEText->SetIsTransparent(true);
    inForm->AddControl(this->mLabel, 11);
    ioIndex++;
    theNames.clear();
    for (long i = IDS_SELECT_RECTANGULAR_AREA; i <= IDS_SELECT_TOWN_TILE; i++) {
        gResource->GetString(i, theText);
        theNames.push_back(theText);
    }
    thePosition.SetY(504.0f);
    this->mModeComboBox = new UIComboBox(0x898, 1, theNames, inFont, GERectangle(thePosition, 250.0f, 20.0f),
                                   100, inForm, ioIndex, 0x100, 0);
    this->mModeComboBox->SetBgOpacity(1.0f);
    this->mModeComboBox->SetTextBoxBgOpacity(0.4f);
    this->mModeComboBox->SetListBoxAlwaysDirty(true);
    this->mModeComboBox->GetGEText()->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
    this->mModeComboBox->GetGEText()->SetIsTransparent(true);
    UIEvent* theEvent = new EESEEAreaCheck;
    this->mModeComboBox->SetTriggerEvent(kControlTriggerSelectionChanged, theEvent);
    theEvent->Dereference();
    inForm->AddControl(this->mModeComboBox, 11);
    ioIndex++;
    gResource->GetString(IDS_CENTER_CAMERA_ON_AREA, theText);
    thePosition.SetY(580.0f);
    this->mCenterButton = new UIButton(0x89a, NULL, NULL, NULL, GERectangle(thePosition, 250.0f, 20.0f), inForm,
                                 ioIndex, 2);
    this->mCenterButton->SetBgOpacity(0.4f);
    theEvent = new EESEECenterCameraOnArea;
    this->mCenterButton->SetTriggerEvent(kControlTriggerButtonUp, theEvent);
    theEvent->Dereference();
    this->mCenterButton->SetText(theText, inFont);
    theGEText = this->mCenterButton->GetGEText();
    theGEText->SetFgColor(GEColor(254.0f / 255.0f, 247.0f / 255.0f, 32.0f / 255.0f));
    theGEText->SetIsTransparent(true);
    inForm->AddControl(this->mCenterButton, 11);
    ioIndex++;
    this->mRectangleMode = true;
    this->mHasRectangle = false;
    this->mHasPoint = false;
    this->mPaintAction = NULL;
}

// SYNTHETIC: EXE 0x57d07f
// (EESETSArea's scalar deleting destructor)

// FUNCTION: EXE 0x57d09b
// Destroys the area sub-screen's controls and releases a rectangle still being drawn
EESETSArea::~EESETSArea()
{
    delete this->mModeComboBox;
    delete this->mCenterButton;
    delete this->mLabel;
    if (this->mPaintAction != NULL)
        this->mPaintAction->Dereference();
}

// FUNCTION: EXE 0x57d0e0
// Sorts the scenario's areas by name
void EESETSArea::Sort()
{
    gWorld.mScenarioManager->SortAreas();
}

// FUNCTION: EXE 0x57d0eb
// Drags the corner of the rectangle being drawn to the tile under the mouse (or the ground point)
void EESETSArea::Update()
{
    GE3DPoint theLocation;
    GE3DLine theLine;
    long theX;
    long theY;
    if (this->mRectangleMode != true || this->mPaintAction == NULL || !gViewport.GetViewRectangle().Contains(gMouse->mPosition))
        return;
    if (gWorld.mMap->PickScreenTile(gMouse->mPosition, theX, theY, true) == NULL) {
        EETile* theTile = gWorld.mMap->GetTileAt(gMouse->mPosition, theX, theY);
        gWorld.mMap->GetScreenLine(gMouse->mPosition, theLine);
        float theT;
        theLine.SolveForZ(theTile->GetHeight(), theT, theLocation);
        theX = (long)theLocation.GetX();
        theY = (long)theLocation.GetY();
    }
    this->mPaintAction->SetCorner(theX, theY);
}

// FUNCTION: EXE 0x57d1d5
// Moves the selected area one place down the list
void EESETSArea::MoveDown()
{
    gWorld.mScenarioManager->MoveAreaDown(this->mSelection + 2);
}

// FUNCTION: EXE 0x57d1e7
// Moves the selected area one place up the list
void EESETSArea::MoveUp()
{
    gWorld.mScenarioManager->MoveAreaUp(this->mSelection + 2);
}

// FUNCTION: EXE 0x57d1f9
// Deletes the selected area and hides the area highlight
void EESETSArea::Delete()
{
    EEUIFScenarioEditor* theEditor = static_cast<EEUIFScenarioEditor*>(this->mCenterButton->GetForm());
    gWorld.mScenarioManager->DeleteArea(this->mSelection + 2);
    theEditor->GetBrushGroup()->SetIsHighlightShown(false);
}

// FUNCTION: EXE 0x57d21d
// Hides the area highlight when the sub-screen is left
void EESETSArea::Deactivate()
{
    static_cast<EEUIFScenarioEditor*>(this->mCenterButton->GetForm())->mBrushGroup->mIsHighlightShown = false;
}

// FUNCTION: EXE 0x57d22e
// Duplicates the selected area
void EESETSArea::Duplicate()
{
    gWorld.mScenarioManager->DuplicateArea(this->mSelection + 2);
}

// FUNCTION: EXE 0x57d240
// Hides the trigger screen's text box and shows the alphabetic sort button
void EESETSArea::Activate()
{
    gUIManager->mScenarioEditorForm->GetTriggerScreen()->HideText();
    gUIManager->mScenarioEditorForm->GetTriggerScreen()->ShowAlphaSort();
}

// NEAR: `mSelection + 2` as `add eax, 2` in the original (ours inc/inc)
// FUNCTION: EXE 0x57d272
// Stores the name, text, selection mode and the drawn rectangle or anchor into the selected area
void EESETSArea::Store()
{
    UWideString theText;
    EESMArea* theArea = gWorld.mScenarioManager->GetArea(this->mSelection + 2);
    theArea->mName = UWideString(gUIManager->mScenarioEditorForm->GetTriggerScreen()->GetName());
    gUIManager->mScenarioEditorForm->GetTriggerScreen()->GetTextEditBox()->GetAllText(theText);
    theArea->mText = theText;
    this->mModeComboBox->GetCurrentSelection(theArea->mSelectMode);
    if (this->mHasRectangle)
        theArea->SetRectangle(this->mRectangle);
    if (this->mHasPoint) {
        U2DPoint<long> theAnchor((long)this->mPoint.GetX(), (long)this->mPoint.GetY());
        theArea->SetAnchor(theAnchor);
    }
}

// FUNCTION: EXE 0x57d34c
// Sets the area's rectangle and marks it as having one (EESMArea::SetRectangle, inline)

// FUNCTION: EXE 0x57d381
// Sets the area's anchor tile and marks it as having one (EESMArea::SetAnchor, inline)

// FUNCTION: EXE 0x57d3aa
// Returns the center of the drawn rectangle (or the anchor point in point mode), false if none
bool EESETSArea::GetCenter(GE2DPoint& outCenter)
{
    bool theResult = false;
    if (this->mRectangleMode) {
        if (this->mHasRectangle) {
            theResult = true;
            this->mRectangle.GetCenter(outCenter);
        }
    } else if (this->mHasPoint) {
        theResult = true;
        outCenter = this->mPoint;
    }
    return theResult;
}

// FUNCTION: EXE 0x57d3e6
// Returns the names of the scenario's user areas
void EESETSArea::GetNames(std::vector<UWideString>& outNames)
{
    gWorld.mScenarioManager->GetUserAreaNames(outNames);
}

// FUNCTION: EXE 0x57d3f8
// Hides the label, mode combo and center camera button
void EESETSArea::Hide()
{
    this->mLabel->SetDisplayMode(kDisplayModeHidden);
    this->mModeComboBox->SetDisplayMode(kDisplayModeHidden);
    this->mCenterButton->SetDisplayMode(kDisplayModeHidden);
}

// NEAR: inline budget (the original expands GetArea and the three GetTriggerScreen calls with their vector calls
// refused; ours calls GetArea out of line)
// FUNCTION: EXE 0x57d41b
// Shows the selected area: name, text, mode, and its rectangle or anchor highlighted on the map (or the
// 'no area selected' label)
void EESETSArea::Load()
{
    EEUIFScenarioEditor* theEditor = static_cast<EEUIFScenarioEditor*>(this->mCenterButton->GetForm());
    EESMArea* theArea = gWorld.mScenarioManager->GetArea(this->mSelection + 2);
    if (theArea == NULL)
        return;
    gUIManager->GetScenarioEditorForm()->GetTriggerScreen()->ShowText();
    gUIManager->GetScenarioEditorForm()->GetTriggerScreen()->GetNameEditBox()->SetText(theArea->GetName());
    gUIManager->GetScenarioEditorForm()->GetTriggerScreen()->GetTextEditBox()->SetText(theArea->GetText());
    this->mRectangle = theArea->GetRect();
    this->mPoint = GE2DPoint((float)theArea->GetAnchor().GetX(), (float)theArea->GetAnchor().GetY());
    this->mHasRectangle = theArea->GetHasRectangle();
    this->mHasPoint = theArea->GetHasAnchor();
    this->mModeComboBox->SetCurrentSelection(theArea->GetSelectMode());
    if (this->GetHasRectangle()) {
        theEditor->GetBrushGroup()->SetIsHighlightShown(true);
        theEditor->SetPaintArea(this->mRectangle);
        this->mLabel->SetDisplayMode(kDisplayModeHidden);
        this->mCenterButton->SetDisplayMode(kDisplayModeShown);
    } else if (this->mHasPoint) {
        theEditor->GetBrushGroup()->SetIsHighlightShown(true);
        theEditor->SetPaintArea(GERectangle(this->mPoint, this->mPoint));
        this->mLabel->SetDisplayMode(kDisplayModeHidden);
        this->mCenterButton->SetDisplayMode(kDisplayModeShown);
    } else {
        theEditor->mBrushGroup->mIsHighlightShown = false;
        this->mLabel->SetDisplayMode(kDisplayModeShown);
        this->mCenterButton->SetDisplayMode(kDisplayModeHidden);
    }
}

// FUNCTION: EXE 0x57d597
// Sets the map rectangle the editor highlights
// (defined in this unit: every caller calls it out of line)
void EEUIFScenarioEditor::SetPaintArea(const GERectangle& inRectangle)
{
    this->mBrushGroup->mRectangle = inRectangle;
}

// FUNCTION: EXE 0x57d5ba
// Adds a new area to the scenario
void EESETSArea::New()
{
    gWorld.mScenarioManager->NewArea();
}

// FUNCTION: EXE 0x57d5c5
// Switches between rectangle and point mode, highlighting the area drawn in that mode and showing
// the label or center button unless the editor UI is hidden
void EESETSArea::SetAreaMode(bool inRectangle)
{
    EEUIFScenarioEditor* theEditor = static_cast<EEUIFScenarioEditor*>(this->mCenterButton->GetForm());
    this->mRectangleMode = inRectangle;
    if (inRectangle) {
        if (this->mHasRectangle) {
            theEditor->mBrushGroup->mIsHighlightShown = true;
            theEditor->SetPaintArea(this->mRectangle);
            if (!theEditor->mIsUIHidden) {
                this->mLabel->SetDisplayMode(kDisplayModeHidden);
                this->mCenterButton->SetDisplayMode(kDisplayModeShown);
            }
        } else {
            theEditor->mBrushGroup->mIsHighlightShown = false;
            if (!theEditor->mIsUIHidden) {
                this->mLabel->SetDisplayMode(kDisplayModeShown);
                this->mCenterButton->SetDisplayMode(kDisplayModeHidden);
            }
        }
    } else {
        if (this->mHasPoint) {
            theEditor->mBrushGroup->mIsHighlightShown = true;
            theEditor->SetPaintArea(GERectangle(this->mPoint, this->mPoint));
            if (!theEditor->mIsUIHidden) {
                this->mLabel->SetDisplayMode(kDisplayModeHidden);
                this->mCenterButton->SetDisplayMode(kDisplayModeShown);
            }
        } else {
            theEditor->mBrushGroup->mIsHighlightShown = false;
            if (!theEditor->mIsUIHidden) {
                this->mLabel->SetDisplayMode(kDisplayModeShown);
                this->mCenterButton->SetDisplayMode(kDisplayModeHidden);
            }
        }
    }
}

// FUNCTION: EXE 0x57d667
// Shows the mode combo plus the 'no area selected' label or, once an area is drawn, the center button
void EESETSArea::Show()
{
    if (!this->mHasRectangle && !this->mHasPoint) {
        this->mLabel->SetDisplayMode(kDisplayModeShown);
        this->mCenterButton->SetDisplayMode(kDisplayModeHidden);
    } else {
        this->mLabel->SetDisplayMode(kDisplayModeHidden);
        this->mCenterButton->SetDisplayMode(kDisplayModeShown);
    }
    this->mModeComboBox->SetDisplayMode(kDisplayModeShown);
}

// NEAR: inline budget (the original calls GERectangle's operator= for the first edge rectangle, ours expands it)
// and register choice
// FUNCTION: EXE 0x57d6a9
// Draws the area with the left button: drags a rectangle (mouse confined to the view) or sets the
// anchor point on release, then highlights it
void EESETSArea::MouseMove(const GE2DPoint& inLocation)
{
    GE3DPoint theLocation;
    GE3DLine theLine;
    long theX;
    long theY;
    float theT;
    EEUIFScenarioEditor* theEditor;
    if (!gViewport.GetViewRectangle().Contains(inLocation))
        return;
    EETile* theTile = gWorld.mMap->PickScreenTile(inLocation, theX, theY, true);
    if (this->mRectangleMode) {
        if (this->mPaintAction != NULL) {
            if (UIManager::GetLMouseButton()->GetValue() > 0) {
                if (theTile == NULL) {
                    theTile = gWorld.mMap->GetTileAt(inLocation, theX, theY);
                    gWorld.mMap->GetScreenLine(inLocation, theLine);
                    theLine.SolveForZ(theTile->GetHeight(), theT, theLocation);
                    theX = (long)theLocation.GetX();
                    theY = (long)theLocation.GetY();
                }
                this->mPaintAction->SetCorner(theX, theY);
            } else if (UIManager::GetLMouseButton()->JustUnpressed()) {
                theEditor = static_cast<EEUIFScenarioEditor*>(this->mCenterButton->GetForm());
                gMouse->SetBounds(GERectangle(0.0f, 0.0f, (float)gGameWindow->GetWidth(),
                                              (float)gGameWindow->GetHeight()));
                theEditor->mEdgeScroll->mEdges = GERectangle(0.0f, 0.0f, (float)gGameWindow->GetWidth() - 1.0f,
                                                       (float)gGameWindow->GetHeight() - 1.0f);
                this->mHasRectangle = true;
                this->mRectangle = this->mPaintAction->mR0C;
                this->mPaintAction->Dereference();
                this->mPaintAction = NULL;
            }
            return;
        }
        if (!UIManager::GetLMouseButton()->JustPressed() || theTile == NULL)
            return;
        theEditor = static_cast<EEUIFScenarioEditor*>(this->mCenterButton->GetForm());
        this->mPaintAction = new EESEAPaintArea(theX, theY);
        gMouse->SetBounds(gViewport.GetViewRectangle());
        theEditor->mEdgeScroll->mEdges = GERectangle(gViewport.GetViewRectangle().GetLeft(),
                                               gViewport.GetViewRectangle().GetTop(),
                                               gViewport.GetViewRectangle().GetRight() - 1.0f,
                                               gViewport.GetViewRectangle().GetBottom() - 1.0f);
    } else {
        if (!UIManager::GetLMouseButton()->JustUnpressed() || theTile == NULL)
            return;
        this->mHasPoint = true;
        this->mPoint = GE2DPoint((float)theX, (float)theY);
        theEditor = static_cast<EEUIFScenarioEditor*>(this->mCenterButton->GetForm());
        theEditor->SetPaintArea(GERectangle(this->mPoint, this->mPoint));
    }
    theEditor->mBrushGroup->mIsHighlightShown = true;
    if (!theEditor->mIsUIHidden) {
        this->mLabel->SetDisplayMode(kDisplayModeHidden);
        this->mCenterButton->SetDisplayMode(kDisplayModeShown);
    }
}

// FUNCTION: EXE 0x57d991
// Creates and posts the rectangle drawing action, starting as a single point at the given tile
// (the rectangle from the four coordinates: the original converts y once into two memory slots)
// (the action's ctor, defined after its first user)
EESEAPaintArea::EESEAPaintArea(long inX, long inY) : mStartX(inX), mStartY(inY), mEndX(inX), mEndY(inY)
{
    this->mR0C = GERectangle((float)inX, (float)inY, (float)inX, (float)inY);
    gUserInterface->PostAction(this);
}

// FUNCTION: EXE 0x57da08
// Returns the scenario's area by index (EEScenarioManager::GetArea, inline)

// SYNTHETIC: EXE 0x57da35
// (EESETSBaseObject's scalar deleting destructor)

// SYNTHETIC: EXE 0x57da52
// (EESEEAreaCheck's scalar deleting destructor)

// SYNTHETIC: EXE 0x57da7d
// (EESEECenterCameraOnArea's scalar deleting destructor)
