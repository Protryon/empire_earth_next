// port: the render phase's snapshot of the world (ee/EERenderSnapshot.h; RUST_PLAN R5a.3, PORT_LEARNINGS "R5a.3").
// No original address: the original renderer read these from the live world after FrameStarting had released the
// world's lock. Taken under that lock, so the physics tick is untouched: the snapshot only reads (and, for the
// coloured rectangles, erases the map's erase markers, which EEROTerrain::RenderScene did a moment later).
#include <windows.h>

#include "lle/LowLevelEngine.h"
#include "lle/GE2DPoint.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GE3DPlane.h"
#include "lle/GE3DLine.h"
#include "lle/GEOrientation.h"
#include "lle/GETransformation.h"
#include "lle/GETerrainMesh.h"
#include "lle/GESurface.h"
#include "lle/GETextureManager.h"
#include "lle/GETexture.h"
#include "lle/GEScreen.h"
#include "lle/GEViewport.h"
#include "lle/GEVertexArray.h"
#include "lle/GERasterizer.h"
#include "lle/GERhombus.h"
#include "lle/TSCriticalSection.h"
#include "lle/U2DRectangle.h"
#include "lle/UIControl.h"
#include "lle/UIEvent.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"

#include "ee/EmpireEarth.h"
#include "ee/EEMap.h"
#include "ee/EEUnit.h"
#include "ee/EEPlayer.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEUIManager.h"
#include "ee/EEUIFScenarioEditor.h"
#include "ee/EEUIFSEBrushGroup.h"
#include "ee/EERenderSnapshot.h"

EERenderSnapshot gRenderSnapshot;

EERenderSnapshot::EERenderSnapshot()
    : mCameraYaw(0.0f), mCameraPitch(0.0f), mInCinematic(false), mTerrainMesh(NULL), mIsSpaceMap(false),
      mBrushHighlightShown(false)
{
}

void EETakeRenderSnapshot()
{
    gRenderSnapshot.mCamera = gWorld.mTransformation;
    gRenderSnapshot.mCameraYaw = gWorld.GetCameraYaw();
    gRenderSnapshot.mCameraPitch = gWorld.GetCameraPitch();
    gRenderSnapshot.mInCinematic = gWorld.mInCinematic;
    EEMap* theMap = gWorld.mMap;
    gRenderSnapshot.mTerrainMesh = theMap != NULL ? &theMap->mTerrainMesh : NULL;
    gRenderSnapshot.mIsSpaceMap = theMap != NULL && theMap->mTerrainMesh.GetIsSpaceMap();
}

// EEROTerrain's part (its FrameStarting): what its RenderScene drew from the map and the editor's brush
void EETakeTerrainSnapshot()
{
    EEMap* theMap = gWorld.mMap;
    gRenderSnapshot.mColoredRectangles.clear();
    // the map's coloured rectangles, back to front under their lock: a zero opacity erases the element (the walk
    // EEROTerrain::RenderScene made while drawing; the drawn ones are kept in that order)
    if (theMap->mColoredRectangles.size() > 0) {
        theMap->mCriticalSectionForColoredRectangles->Enter();
        for (long theRect = theMap->mColoredRectangles.size() - 1; theRect >= 0; theRect--) {
            RTSColoredRectangle& theColored = theMap->mColoredRectangles[theRect];
            if (theColored.mOpacity != 0.0f)
                gRenderSnapshot.mColoredRectangles.push_back(theColored);
            else
                theMap->GetColoredRectangles().erase(theMap->GetColoredRectangles().begin() + theRect);
        }
        theMap->mCriticalSectionForColoredRectangles->Leave();
    }
    gRenderSnapshot.mBrushHighlightShown = false;
    if (gUIManager->mScenarioEditorForm != NULL) {
        const GERectangle& theRectangle = gUIManager->mScenarioEditorForm->mBrushGroup->mRectangle;
        if (gUIManager->mScenarioEditorForm->mBrushGroup->mIsHighlightShown) {
            gRenderSnapshot.mBrushHighlightShown = true;
            gRenderSnapshot.mBrushTiles.mLeft = UMFloor(theRectangle.GetLeft());
            gRenderSnapshot.mBrushTiles.mTop = UMFloor(theRectangle.GetTop());
            gRenderSnapshot.mBrushTiles.mRight = UMFloor(theRectangle.GetRight());
            gRenderSnapshot.mBrushTiles.mBottom = UMFloor(theRectangle.GetBottom());
        }
    }
}
