// C:\EEX\Empire Earth\Actions\EEACenterCamera.cpp (file name inferred: no __FILE__; RTTI class
// EEACenterCamera, vtable 0x442ba0). The center-camera action: 0x1de1..0x2086 (EEACalamity's ??_G ends at 0x1de1,
// EEAJoinOrHostAGame's ctor starts at 0x2086). Virtual4, SetLocation, SetUnit. The std::exception/logic_error COMDATs
// that follow (0x1fc4..0x2086: their first copies in link order) are not emitted by these three functions, nor by
// anything their headers make this unit compile: they belong to another object (a unit whose own code is folded or
// inline elsewhere) or to code of this one not found yet; the unit is registered as 0x1de1..0x1fc4.
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

#include "ee/EmpireEarth.h"
#include "ee/EEUserInterface.h"
#include "ee/RTSWorld.h"
#include "ee/EECameraControl.h"
#include "ee/EEWorld.h"
#include "ee/EEMap.h"
#include "ee/EEUnit.h"
#include "ee/EEAction.h"
#include "ee/EEACenterCamera.h"

// FUNCTION: EXE 0x401de1
// Centres the camera on the stored location, or follows the stored unit (a camera object takes over the camera)
void EEACenterCamera::Virtual4()
{
    if (this->mUnitID == -1) {
        ::gWorld.mCameraControl.SetLocation(this->mLocation, false, true);
        ::gWorld.mCameraControl.SetZoom(::gWorld.mCameraControl.GetZoom());
        ::gWorld.mCameraControl.SetChanged();
    } else {
        EEUnit* theUnit = ::gWorld.GetUnit(this->mUnitID);
        GE3DPoint theNoLocation(-10000.0f, 0.0f, 0.0f);
        if (theUnit && theUnit->mTileNode && theUnit->mComplexUnit->mUnitClass->mUnitType == kUnitTypeCameraObject) {
            ::gWorld.mCameraControl.SetZoom(0.0f);
            ::gWorld.mCameraControl.SetMode(3);
            ::gWorld.mCameraControl.MoveTo(theUnit->mComplexUnit, theNoLocation, false, 0.0f,
                                             theUnit->mComplexUnit, theNoLocation, false, 0.0f, 0.0f, 0.0f);
            ::gWorld.mCameraControl.SetChanged();
            ::gWorld.mCameraControl.Update(0.0f);
            ::gWorld.mCameraControl.SetMode(2);
        }
    }
}

// FUNCTION: EXE 0x401ee0
// Stores a map tile's centre as the camera target and posts the action (ignored off the map)
void EEACenterCamera::SetLocation(long inX, long inY)
{
    EEMapTile* theTile = ::gWorld.mMap->GetTile(inX, inY);
    if (!theTile)
        return;
    this->mUnitID = -1;
    this->mLocation.SetX(inX + 0.5f);
    this->mLocation.SetY(inY + 0.5f);
    this->mLocation.SetZ(theTile->mHeight);
    gUserInterface->PostAction(this);
}

// FUNCTION: EXE 0x401f43
// Stores a unit as the camera target (a camera object by id, else its outermost container's position) and posts it
void EEACenterCamera::SetUnit(EEUnit* inUnit)
{
    if (!inUnit)
        return;
    EEComplexUnit* theUnit = inUnit->mComplexUnit;
    if (theUnit) {
        if (theUnit->mUnitClass->mUnitType == kUnitTypeCameraObject) {
            this->mUnitID = theUnit->mUnitID;
        } else {
            this->mUnitID = -1;
            while (theUnit->mContainer)
                theUnit = theUnit->mContainer;
            theUnit->mPhysicalModel.GetLocation().GetPosition(this->mLocation);
        }
    } else {
        this->mLocation.SetX(inUnit->GetTileX() + 0.5f);
        this->mLocation.SetY(inUnit->GetTileY() + 0.5f);
        this->mLocation.SetZ(inUnit->mTile->mHeight);
    }
    gUserInterface->PostAction(this);
}

// ICF-FOLDED: EXE 0x5720c6
// (no address of its own: folded into the shared `ret` body the vtable slot points at)
void EEACenterCamera::Virtual8()
{
}
