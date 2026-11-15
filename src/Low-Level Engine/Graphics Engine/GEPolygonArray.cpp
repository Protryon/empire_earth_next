// C:\EEX\Low-Level Engine\Graphics Engine\GEPolygonArray.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// 0x10023209..0x10023219: the ctor and dtor of the abstract polygon array (the rasterizer plug-ins implement it).
// No init chains: the unit includes no header with static objects.
#include <windows.h>

#include "lle/LowLevelEngineCore.h"
#include "lle/GEPolygonArray.h"

// FUNCTION: LLE 0x10023209
// Creates the polygon array base (nothing to set up)
GEPolygonArray::GEPolygonArray()
{
}

// FUNCTION: LLE 0x10023212
// Destroys the polygon array base (nothing to free)
GEPolygonArray::~GEPolygonArray()
{
}
