// C:\EEX\Empire Earth\Graphics Thread\EEROEffects.cpp (no __FILE__; name from the RTTI class). The effects' render
// object (vtable 0x4413d0): 0x111d3d..0x111dae; EEROGroundShadows's ctor opens the next unit. Unit flags: the exe's
// module set.
#include <windows.h>

#include "lle/LowLevelEngine.h"
#include "lle/GE2DPoint.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GETransformation.h"
#include "lle/GESurface.h"
#include "lle/GETextureManager.h"
#include "lle/GETexture.h"
#include "lle/GEScreen.h"
#include "lle/GEViewport.h"
#include "lle/GEVertexArray.h"
#include "lle/GERasterizer.h"

#include "ee/EmpireEarth.h"
#include "ee/RTSGraphics.h"
#include "ee/EERenderObject.h"
#include "ee/EEROEffects.h"

extern GEViewport gU4E8F08;                 // 0x4e8f08 (EERenderObject.cpp; the render viewport)


// FUNCTION: EXE 0x511d3d
// Creates the effects' render object
EEROEffects::EEROEffects()
{
}

// SYNTHETIC: EXE 0x511d4f
// (the scalar deleting destructor, emitted with the vtable; slot EEROEffects+0x10)

// FUNCTION: EXE 0x511d6b
// Destroys the effects' render object
EEROEffects::~EEROEffects()
{
}

// FUNCTION: EXE 0x511d76
// Sorts the visible effects into the render queues for this frame
void EEROEffects::FrameStarting()
{
    this->QueueEffects();
}

// FUNCTION: EXE 0x511d7b
// Clears the z buffer of the game view before the effects draw
void EEROEffects::SceneStarting()
{
    GERectangle theRectangle = gU4E8F08.GetViewRectangle();
    gRasterizer->ClearZBuffer(1.0f, theRectangle, gScreenIndex);
}

// Virtual08 and Virtual0C: one empty body, ICF-folded into 0xe123d (no address of their own here)
void EEROEffects::RenderScene()
{
}

void EEROEffects::FrameDone()
{
}
