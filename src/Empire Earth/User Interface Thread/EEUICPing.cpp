// C:\EEX\Empire Earth\User Interface Thread\EEUICPing.cpp (file name inferred: no __FILE__; RTTI class EEUICPing).
// The lobby's ping meter: 0x258464..0x25861c (ctor, ??_G, dtor, DrawPolygons, SetPing; EEUICPortrait's ctor at
// 0x25861c). Unit flags: the exe module set.
#include <windows.h>
#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GEScreen.h"
#include "lle/GEViewport.h"
#include "lle/GEVertexArray.h"
#include "lle/GERasterizer.h"
#include "lle/GERhombus.h"
#include "lle/UIControl.h"
#include "ee/EmpireEarth.h"
#include "ee/EEUICPing.h"

// Creates the lobby's ping meter (60x10; its bars made, placed and drawn by ee_screens ping.rs and hud_draw.rs,
// which set the lit count: DrawPolygons and SetPing forward)
EEUICPing::EEUICPing(unsigned long inId, const GE2DPoint& inLocation, UIForm* inForm, unsigned long inIndex)
    : EEUIRustControl(inId, GERectangle(inLocation, 60.0f, 10.0f), inForm, inIndex, 0), EeScreensPingFields()
{
    ee_screens_ping_init(this, this->Host(), inLocation.GetX(), inLocation.GetY());
}
