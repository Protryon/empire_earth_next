// C:\EEX\Low-Level Engine\Graphics Engine\GEVertexArray.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// 0x100367fc..0x1003680c: the abstract vertex array's ctor and dtor (the rasterizer plug-ins implement the rest).
#include <windows.h>

#include "lle/LowLevelEngineCore.h"
#include "lle/GEVertexArray.h"

// FUNCTION: LLE 0x100367fc
// Creates the abstract vertex array (the rasterizer plug-ins implement it)
GEVertexArray::GEVertexArray()
{
}

// FUNCTION: LLE 0x10036805
// Destroys the abstract vertex array
GEVertexArray::~GEVertexArray()
{
}
