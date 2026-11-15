// C:\EEX\Low-Level Engine\Graphics Engine\GEVertexArray.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// 0x100367fc..0x1003680c: the abstract vertex array's ctor and dtor (the rasterizer plug-ins implement the rest).
#include <windows.h>

#include "lle/LowLevelEngineCore.h"
#include "lle/GEVertexArray.h"

// Creates the abstract vertex array (the rasterizer plug-ins implement it)
GEVertexArray::GEVertexArray()
{
}

// Destroys the abstract vertex array
GEVertexArray::~GEVertexArray()
{
}
