// C:\EEX\Empire Earth\Actions\EEAPlayerEvent.cpp (file name inferred: no __FILE__; RTTI class EEAPlayerEvent,
// vtable 0x446ae0). The player-event hot key's action: 0x4352..0x4413 (EEAMouseClick.cpp's inline COMDATs end at
// 0x4352, EEARemovePlayer's Virtual4 starts at 0x4413). Virtual4 only (ctor/??_G inline elsewhere, Virtual8 the shared
// empty slot): the camera jumps to the next player event's source tile.
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
#include "ee/EETileLocation.h"
#include "ee/EEPPlayerEvent.h"

#include "ee/EmpireEarth.h"
#include "ee/RTSWorld.h"
#include "ee/EECameraControl.h"
#include "ee/EEWorld.h"
#include "ee/EEMap.h"
#include "ee/EEPlayer.h"
#include "ee/EEMiniMap.h"
#include "ee/EEAction.h"
#include "ee/EEAPlayerEvent.h"

// FUNCTION: EXE 0x404352
// Moves the camera to the source tile of the current player's next player event
void EEAPlayerEvent::Virtual4()
{
    GE3DPoint theLocation;
    EEPPlayerEvent* theEvent = ::gWorld.GetCurrentPlayer()->mEvents.GetNextEvent();
    if (!theEvent)
        return;
    EETileLocation theSource = theEvent->mSource;
    if (theSource.GetX() == -10000 || theSource.GetY() == -10000)
        return;
    float theTileX = theSource.GetX() + 0.5f;
    float theTileY = theSource.GetY() + 0.5f;
    theLocation = GE3DPoint(theTileX, theTileY, ::gWorld.mMap->mTerrainMesh.GetMeshZ(theTileX, theTileY));
    ::gWorld.mCameraControl.SetChanged();
    ::gWorld.mCameraControl.SetLocation(theLocation, false, true);
    gMiniMap->Refresh();
    gCameraFollowing = false;
}

// ICF-FOLDED: EXE 0x5720c6
// (no address of its own: folded into the shared `ret` body the vtable slot points at)
void EEAPlayerEvent::Virtual8()
{
}
