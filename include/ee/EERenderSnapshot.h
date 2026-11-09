// ee/EERenderSnapshot.h (port; RUST_PLAN R5a.3, PORT_LEARNINGS "R5a.3"). The render phase's view of the world: plain
// data taken under the world's lock at the start of every frame and read after the lock is released.
//
// A frame of the graphics thread (RTSGraphics::Render) is FrameStarting under the world's read lock (EEGraphics: lock
// mode 0, the only one the game uses), then SceneStarting, RenderScene and FrameDone with no lock while the physics tick
// runs. The capture is FrameStarting: EETakeRenderSnapshot (EEGraphics::FrameStarting) fills the world-wide part,
// each render object's FrameStarting its own (EEROTerrain: the coloured rectangles; EERenderObject::FillRenderLists:
// the render units; the effects' Queue: their draw state). After FrameStarting the render objects read only this
// snapshot, the render lists (ee/EERenderLists.h), the effects' own draw copies and the graphics-owned state the
// table in PORT_LEARNINGS lists. Requires lle/GETransformation.h, lle/U2DRectangle.h, ee/EEMap.h (RTSColoredRectangle),
// <vector>.
#ifndef EE_EERENDERSNAPSHOT_H
#define EE_EERENDERSNAPSHOT_H

#include <vector>

class GETerrainMesh;

struct EERenderSnapshot {
    EERenderSnapshot();

    // the camera (RTSWorld::mTransformation, mCameraYaw, mCameraPitch): the effects' billboards and sprites
    GETransformation mCamera;
    float mCameraYaw;
    float mCameraPitch;
    // EEWorld::mInCinematic: no rings, bars or labels
    bool mInCinematic;

    // the terrain (gWorld.mMap; NULL outside a game). The mesh is drawn, not copied: its render buffers (visible
    // tiles, fog of war, water animation) are built by the graphics thread itself in FrameStarting
    GETerrainMesh* mTerrainMesh;
    bool mIsSpaceMap;
    // EEROTerrain::FrameStarting: the map's coloured rectangles with an opacity, back to front (the order RenderScene
    // drew them in); the erase markers (opacity 0) are erased from the map there, as RenderScene did
    std::vector<RTSColoredRectangle> mColoredRectangles;
    // the scenario editor's brush highlight (EEUIFSEBrushGroup), in whole tiles
    bool mBrushHighlightShown;
    U2DRectangle<long> mBrushTiles;
};

// the frame's snapshot (graphics thread only)
extern EERenderSnapshot gRenderSnapshot;

// Takes the world-wide part of the snapshot; the caller holds the world's lock (EEGraphics::FrameStarting)
void EETakeRenderSnapshot();
// Takes EEROTerrain's part (the coloured rectangles, the editor's brush); its FrameStarting, in a game
void EETakeTerrainSnapshot();

#endif
