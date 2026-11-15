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

// Creates the lobby's ping meter (60x10; its bars placed by ee_screens ping.rs, which draws them and sets the lit
// count: DrawPolygons and SetPing forward)
EEUICPing::EEUICPing(unsigned long inId, const GE2DPoint& inLocation, UIForm* inForm, unsigned long inIndex)
    : EEUIRustControl(inId, GERectangle(inLocation, 60.0f, 10.0f), inForm, inIndex, 0), EeScreensPingFields()
{
    ee_screens_ping_init(this, this->Host(), inLocation.GetX(), inLocation.GetY());
}

// The bars' ops (EE_SCREENS_PING_*): a bar placed (green) or drawn
int64_t EEUICPing::RustOp(uint32_t inOp, int64_t inA, int64_t inB, float* ioF)
{
    if (inOp == EE_SCREENS_PING_DRAW)
        return this->GetRasterizer()->Draw(&this->mBars[inA], gScreenIndex);
    this->mBars[inA].SetRectangle(GERectangle(ioF[0], ioF[1], ioF[2], ioF[3]));
    this->mBars[inA].SetColor(GEColor(0.0f, 1.0f, 0.0f));
    return 0;
}
