// C:\EEX\Empire Earth\Graphics Thread\EEOrbital.cpp (file name inferred: no __FILE__; named after the class whose members
// it holds, RTTI EEOrbital, declared in ee/EEGraphicObject.h). 0x208c37..0x208e6b (unit map [C], after EESightRings.cpp,
// before EESavedGroup.cpp; edge: two globals chains): the orbiting-part name prefixes, EEOrbital::IsOrbitalModel,
// Init and Update. Unit flags: the exe's module set (provisional names).
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <vector>

#include "lle/LowLevelEngine.h"
#include "lle/GEModel.h"

#include "ee/EEPhysics.h"
#include "ee/EEGraphicObject.h"

#include "rust/render/gfx.h"
#include "rust/game/gamecpp.h"

// The name tests, the speed and the update are ee_gfx effect_orbit.rs' (the prefixes "orbital_cw_" and "orbital_ccw_",
// any case; the degrees between the prefix and the name's last '_')

// Attaches the orbiting part to a model, speed from the name in degrees per second (orbital_cw_<deg>_... clockwise,
// orbital_ccw_ counterclockwise)
void EEOrbital::Init(GEModel* inModel)
{
    ee_gamecpp_ov_orbital_init(this, inModel);   // gamecpp ro_misc.rs
}

// Advances the orbiting part's angle by its speed over the tick, wrapped to -pi..pi
void EEOrbital::Update()
{
    ee_gfx_orbital_update(&this->mAngle, this->mSpeed, gPhysics->GetTickSeconds());
}
