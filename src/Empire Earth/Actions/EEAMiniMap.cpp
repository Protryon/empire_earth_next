// C:\EEX\Empire Earth\Actions\EEAMiniMap.cpp (file name inferred: no __FILE__; RTTI class EEAMiniMap, vtable
// 0x44b94c). The mini-map click action: 0x2a84..0x2fb0 (EEALasso's ??_G ends at 0x2a84, EEAMouseClick's ctor starts
// at 0x2fb0). The mini-map transformation's init chain, Virtual4, SendGroup with EEMSetRallyGoal's inline ctor and
// EEGroup::GetFirstUnit after it (their first user), CenterCamera, then EEMap::GetTile's out-of-line copy.
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
#include "lle/GE2DPoint.h"
#include "lle/GE3DPoint.h"
#include "lle/GETransformation.h"
#include "lle/GETerrainMesh.h"
#include "lle/GE3DLine.h"
#include "lle/GEPhysicalModel.h"
#include "lle/NEMessage.h"
#include "lle/NESynchronousCommunications.h"

#include "ee/EmpireEarth.h"
#include "ee/EECommunications.h"
#include "ee/RTSWorld.h"
#include "ee/EECameraControl.h"
#include "ee/EEWorld.h"
#include "ee/EEMap.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEMessage.h"
#include "ee/EEAction.h"
#include "ee/EEAMiniMap.h"

GETransformation gMiniMapTransformation;
// SYNTHETIC: EXE 0x402a84   (wrapper: a bare jmp, GETransformation has no destructor)
// SYNTHETIC: EXE 0x402a89   (ctor)

// FUNCTION: EXE 0x402a94
// Runs the mini-map click: centres the camera (mode 0) or sends the selected group there (mode 1)
void EEAMiniMap::Virtual4()
{
    switch (this->mMode) {
    case 0:
        this->CenterCamera();
        break;
    case 1:
        this->SendGroup();
        break;
    }
}

// FUNCTION: EXE 0x402aaa
// Sends the selected group (or sets the rally point) to the clicked mini-map location, queued with Shift
void EEAMiniMap::SendGroup()
{
    GE2DPoint theCenter;
    GE2DPoint theLocation;
    GE3DPoint theTile;
    GERectangle theBounds;
    bool theQueue = this->mModifiers & kModifierShift;
    EESelectedGroup* theGroup = ::gWorld.GetCurrentPlayer()->mSelectedGroup;
    bool theRun = (this->mModifiers >> 1) & 1;
    theGroup->GetBounds(theBounds);
    theBounds.GetCenter(theCenter);
    gMiniMapTransformation.Apply(this->mPoint, theLocation);
    long theX = theLocation.GetX();
    long theY = theLocation.GetY();
    if (::gWorld.mMap->GetTile(theX, theY) == NULL) {
        theTile = GE3DPoint(theLocation.GetX(), theLocation.GetY(), 0.0f);
        ::gWorld.mMap->GetClosestTile(&theTile, &theX, &theY);
    }
    float theAngle = UMArcTan2(-(theLocation.GetX() - theCenter.GetX()), theLocation.GetY() - theCenter.GetY());
    long theCount = theGroup->CountUnitsOfKind(2);
    if (theCount == theGroup->GetNumUnits() && (theGroup->mKindFlags & kUnitKindBuilding)) {
        gCommunications->ProcessLocalUserInput(
            new EEMSetRallyGoal(theGroup->GetFirstUnit()->GetUnitID(), GE2DPoint(theX + 0.5f, theY + 0.5f), -1, theQueue),
            kSlotNone);
    } else {
        gCommunications->ProcessLocalUserInput(
            new EEMPathTo(theX + 0.5f, theY + 0.5f, theAngle, !theRun, theQueue), kSlotNone);
    }
}

// FUNCTION: EXE 0x402ca6
// Creates the message that sets a building's rally goal
// (EEMSetRallyGoal's inline ctor, ee/EEMessage.h)

// TEMPLATE: EXE 0x402cf6
// Returns the set-rally-goal message's label
// (EEMSetRallyGoal::GetMessageLabel, inline in ee/EEMessage.h)

// FUNCTION: EXE 0x402cfc
// Returns the group's first unit, or NULL when the group is empty
// (EEGroup::GetFirstUnit: out of line after its first user) slot 0x18 then slot 0x14(0)
EEComplexUnit* EEGroup::GetFirstUnit()
{
    if (this->GetNumUnits())
        return this->GetUnit(0);
    return NULL;
}

// FUNCTION: EXE 0x402d17
// Centres the camera on the clicked mini-map point, clamping points off the map to the nearest tile
void EEAMiniMap::CenterCamera()
{
    GE2DPoint theLocation;
    GE3DPoint theTarget;
    GE3DPoint theTile;
    GE3DLine theLine;
    float theT;
    EEMap* theMap = ::gWorld.mMap;
    gMiniMapTransformation.Apply(this->mPoint, theLocation);
    long theY = UMFloor(theLocation.GetY());
    long theX = UMFloor(theLocation.GetX());
    if (::gWorld.mMap->GetTile(theX, theY) == NULL) {
        long theTileX;
        long theTileY;
        theTile = GE3DPoint(theLocation.GetX(), theLocation.GetY(), 0.0f);
        ::gWorld.mMap->GetClosestTile(&theTile, &theTileX, &theTileY);
        theLocation.SetX((float)theTileX);
        theLocation.SetY((float)theTileY);
    }
    theTarget.SetX(theLocation.GetX() + 0.5f);
    theTarget.SetY(theLocation.GetY() + 0.5f);
    theTarget.SetZ(::gWorld.mMap->mTerrainMesh.GetMeshZ(theLocation.GetX(), theLocation.GetY()));
    if (theMap) {
        if (theMap->mTerrainMesh.GetLeftPlane().IsPointOnOrBelowPlane(theTarget)) {
            theLine.SetStart(theTarget);
            theLine.SetDirection(theMap->mTerrainMesh.GetLeftPlane().GetNormal());
            if (theLine.Intersects(theMap->mTerrainMesh.GetLeftPlane(), theT))
                theLine.GetPointAt(theT, theTarget);
        } else if (theMap->mTerrainMesh.GetRightPlane().IsPointOnOrBelowPlane(theTarget)) {
            theLine.SetStart(theTarget);
            theLine.SetDirection(theMap->mTerrainMesh.GetRightPlane().GetNormal());
            if (theLine.Intersects(theMap->mTerrainMesh.GetRightPlane(), theT))
                theLine.GetPointAt(theT, theTarget);
        }
        if (theMap->mTerrainMesh.GetFrontPlane().IsPointOnOrBelowPlane(theTarget)) {
            theLine.SetStart(theTarget);
            theLine.SetDirection(theMap->mTerrainMesh.GetFrontPlane().GetNormal());
            if (theLine.Intersects(theMap->mTerrainMesh.GetFrontPlane(), theT))
                theLine.GetPointAt(theT, theTarget);
        } else if (theMap->mTerrainMesh.GetBackPlane().IsPointOnOrBelowPlane(theTarget)) {
            theLine.SetStart(theTarget);
            theLine.SetDirection(theMap->mTerrainMesh.GetBackPlane().GetNormal());
            if (theLine.Intersects(theMap->mTerrainMesh.GetBackPlane(), theT))
                theLine.GetPointAt(theT, theTarget);
        }
    }
    ::gWorld.mCameraControl.SetLocation(theTarget, false, true);
    ::gWorld.mCameraControl.SetZoom(::gWorld.mCameraControl.GetZoom());
    ::gWorld.mCameraControl.SetChanged();
}

// FUNCTION: EXE 0x402f85
// Returns the map tile at a tile position, or NULL off the map
// (EEMap::GetTile, inline in ee/EEMap.h: its out-of-line copy, first called here)

// ICF-FOLDED: EXE 0x5720c6
// (no address of its own: folded into the shared `ret` body the vtable slot points at)
void EEAMiniMap::Virtual8()
{
}
