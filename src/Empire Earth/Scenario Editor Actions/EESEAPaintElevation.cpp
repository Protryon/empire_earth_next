// C:\EEX\Empire Earth\Scenario Editor Actions\EESEAPaintElevation.cpp (file name inferred: no __FILE__; RTTI class
// EESEAPaintElevation; its vtable/ctor/??_G are emitted by EEUIFSEPaintElevation.cpp). The elevation painting action:
// 0x1a830a..0x1a86ff (Virtual4, SetArea, the deque<EESEAPaintRegion> back/_Growmap COMDATs). After EESEAPaintArea.cpp,
// before EESEAPaintMoutains.cpp.
#include <windows.h>
#include <mbstring.h>
#include <stdio.h>
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
#include "lle/GE2DPoint.h"
#include "lle/GE3DPoint.h"
#include "lle/GETransformation.h"
#include "lle/GETerrainMesh.h"
#include <vector>
#include <deque>

#include "ee/EmpireEarth.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEMap.h"
#include "ee/EEPlayer.h"
#include "ee/EEHumanPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEAction.h"
#include "ee/EEUIManager.h"
#include "ee/EEUserInterface.h"
#include "ee/EEGraphics.h"
#include "ee/RTSGraphics.h"
#include "ee/EEMiniMap.h"
#include "ee/EEUIMiniMap.h"
#include "ee/EEAIContinentManager.h"
#include "ee/EEScenarioManager.h"
#include "ee/EEEnvironmentManager.h"
#include "ee/EEGameOptions.h"
#include "lle/UIButton.h"
#include "ee/EEDatabaseTables.h"
#include "ee/EERandomMapClass.h"
#include "ee/EERandomMapGenerators.h"
#include "lle/GE3DPlane.h"
#include "lle/UIRadioButton.h"
#include "ee/EEEffectManager.h"
#include "ee/EEUIFScenarioEditor.h"
#include "ee/EEUIFSEPaint.h"
#include "ee/EESEAPaint.h"
#include "ee/EEUIFSEPaintElevation.h"

// 0x445618: the height of one elevation level, defined in EEUIFSEPaintElevation.cpp (its MouseMove reads the
// same word)
extern const float kSEPELevelHeight;
static const float kSEAPEHillHeight = 0.81649655f;   // 0x44598c (provisional name)
static const float kSEAPEHillLimit = 1.6329931f;     // 0x445988 (provisional name)

// FUNCTION: EXE 0x5a830a
// Raises each queued area as the elevation tab's buttons say (a plateau at the level, rough hills at 4x, or flat
// ground), fixes its cliffs and redraws the minimap; tells the effects about the last area only
void EESEAPaintElevation::Virtual4()
{
    EEMap* theMap = gWorld.mMap;
    EESEAPaintRegion theRegion;
    float theHeight;
    float thePlateau;
    float theSlope;
    float theRoughness;
    bool theRaise;

    gB529066 = false;
    gCameraFollowing = false;
    gB529068 = false;
    if (static_cast<UIRadioButton*>(gUIManager->mScenarioEditorForm->GetControlById(kSEElevationRaiseButton))->GetIsChecked() == true) {
        theHeight = this->mLevel * kSEPELevelHeight;
        theRaise = true;
        theSlope = 0.40824829f;
        thePlateau = 0.0f;
        theRoughness = 1.0f;
    } else if (static_cast<UIRadioButton*>(gUIManager->mScenarioEditorForm->GetControlById(kSEElevationLowerButton))->GetIsChecked() == true) {
        theHeight = this->mLevel * kSEAPEHillHeight;
        theRaise = true;
        theSlope = 1.6329931f;
        thePlateau = 0.0f;
        if (theHeight < kSEAPEHillLimit)
            theRoughness = 0.4f;
        else
            theRoughness = 0.26f;
    } else {
        theRaise = false;
        theHeight = 0.0f;
        thePlateau = 0.40824829f;
        theSlope = 1.6329931f;
        theRoughness = 1.0f;
    }
    while (this->mPendingAreas.size() != 0) {
        theRegion = this->mPendingAreas.back();
        this->mPendingAreas.pop_back();
        this->mPaintedAreas.push_back(theRegion);
        theMap->Raise(&theRegion.mLeft, &theRegion.mRight, &theRegion.mTop, &theRegion.mBottom, theHeight, thePlateau,
                      theSlope, theRoughness, theRaise);
        theMap->RemoveFlatCliffs(theRegion.mLeft, theRegion.mRight + 1, theRegion.mBottom, theRegion.mTop + 1);
        theMap->UpdateCliffs(theRegion.mLeft - 2, theRegion.mRight + 3, theRegion.mBottom - 2, theRegion.mTop + 3);
        gMiniMap->RedrawRect(theRegion.mLeft, theRegion.mRight, theRegion.mTop, theRegion.mBottom);
    }
    static_cast<EEUIFSEPaint*>(gUIManager->mScenarioEditorForm->GetControlById(kSEElevationPaint))->StoreMouseTile();
    gEffectManager->ElevationChanged(theRegion.mLeft, theRegion.mTop, theRegion.mRight, theRegion.mBottom);
    gMiniMap->Refresh();
}

// FUNCTION: EXE 0x5a8507
// Queues the area unless every point already sits at the level's height; returns whether it queued
bool EESEAPaintElevation::SetArea(long inLeft, long inTop, long inRight, long inBottom)
{
    EESEAPaintRegion theRegion;
    bool theChanged = false;
    long x;
    long y;
    // the height Virtual4 paints for this level (plateau, hills at 4x, or flat ground), in world units like the
    // vertex heights
    float theHeight;
    if (static_cast<UIRadioButton*>(gUIManager->mScenarioEditorForm->GetControlById(kSEElevationRaiseButton))->GetIsChecked() == true)
        theHeight = this->mLevel * kSEPELevelHeight;
    else if (static_cast<UIRadioButton*>(gUIManager->mScenarioEditorForm->GetControlById(kSEElevationLowerButton))->GetIsChecked() == true)
        theHeight = this->mLevel * kSEAPEHillHeight;
    else
        theHeight = 0.0f;
    for (y = inTop + 1; y >= inBottom; y--) {
        for (x = inLeft; x <= inRight + 1; x++) {
            if (gWorld.mMap->mTerrainMesh.GetIsValidPoint(x, y) && gWorld.mMap->mTerrainMesh.GetZ(x, y) != theHeight)
                theChanged = true;
        }
    }
    if (theChanged) {
        theRegion.mLeft = inLeft;
        theRegion.mTop = inTop;
        theRegion.mRight = inRight;
        theRegion.mBottom = inBottom;
        this->mPendingAreas.push_back(theRegion);
    }
    return theChanged;
}

// FUNCTION: EXE 0x5a85d2
// Returns the last queued elevation area (deque<EESEAPaintRegion>::back)

// FUNCTION: EXE 0x5a85f1
// Grows the block map of the elevation area queue (deque<EESEAPaintRegion>::_Growmap; ICF-shared with
// deque<EEComplexUnitWaypoint>'s)
