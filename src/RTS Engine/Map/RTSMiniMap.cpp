// C:\EEX\RTS Engine\Map\RTSMiniMap.cpp (__FILE__ 0x4bf138). The mini map's engine side (RTTI RTSMiniMap, vtable
// 0x441dc4). LLE round 8: the bodies are Rust (rust/crates/ee_game/src/rts/minimap.rs: the textures, the unit layer,
// the locks, the battle-flag grid, Ping); the members forward, and the shims below are the C++ calls those bodies
// make (one statement each). The original's addresses and the bodies' history: git history before LLE round 8.
#include <windows.h>
#include <list>
#include <string.h>

#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GESurface.h"
#include "lle/GETextureManager.h"
#include "lle/GETexture.h"
#include "lle/GEScreen.h"

#include "ee/EEMiniMap.h"

// the layers' clear colour
static GEColor sBlack(0.0f, 0.0f, 0.0f);

// The shims minimap.rs calls (MiniMapCpp)
extern "C" {
void ee_rtsmmcpp_set_viewing_player(RTSMiniMap* inMap, RTSPlayer* inPlayer) { inMap->mViewingPlayer = inPlayer; }
void** ee_rtsmmcpp_terrain_slot(RTSMiniMap* inMap) { return reinterpret_cast<void**>(&inMap->mTerrainTexture); }
void** ee_rtsmmcpp_world_slot(RTSMiniMap* inMap) { return reinterpret_cast<void**>(&inMap->mWorldTexture); }
void** ee_rtsmmcpp_unit_slot(RTSMiniMap* inMap) { return reinterpret_cast<void**>(&inMap->mUnitSurface); }
void** ee_rtsmmcpp_terrain_lock_slot(RTSMiniMap* inMap) { return reinterpret_cast<void**>(&inMap->mTerrainCriticalSection); }
void** ee_rtsmmcpp_unit_lock_slot(RTSMiniMap* inMap) { return reinterpret_cast<void**>(&inMap->mUnitCriticalSection); }
unsigned char** ee_rtsmmcpp_flags_slot(RTSMiniMap* inMap) { return &inMap->mBattleFlags; }
long* ee_rtsmmcpp_flags_width_slot(RTSMiniMap* inMap) { return &inMap->mBattleFlagsWidth; }
long* ee_rtsmmcpp_flags_height_slot(RTSMiniMap* inMap) { return &inMap->mBattleFlagsHeight; }
unsigned char* ee_rtsmmcpp_flags_new() { return new unsigned char[0x1000]; }
void ee_rtsmmcpp_flags_delete(unsigned char* inFlags) { delete[] inFlags; }
void ee_rtsmmcpp_map_to_cell(RTSMiniMap* inMap, const EeGe2DPoint* inPoint, EeGe2DPoint* outPoint) { inMap->mMapToCell.Apply(*reinterpret_cast<const GE2DPoint*>(inPoint), *reinterpret_cast<GE2DPoint*>(outPoint)); }
void ee_rtsmmcpp_cells_push(RTSMiniMap* inMap, unsigned char inX, unsigned char inY) { inMap->mBattleCells.push_back(RTSMiniMapCell{inX, inY}); }
GETexture* ee_rtsmmcpp_make_texture(GEScreen* inScreen, GESurface* inSurface) { return inScreen->GetTextureManager()->MakeTexture(inSurface); }
void ee_rtsmmcpp_texture_reference(GETexture* inTexture) { inTexture->Reference(); }
void ee_rtsmmcpp_texture_dereference(GETexture* inTexture) { inTexture->Dereference(); }
void ee_rtsmmcpp_texture_min_filter(GETexture* inTexture, unsigned long inMode) { inTexture->SetStageMinFilterMode(inMode, 0xffffffff); }
void ee_rtsmmcpp_texture_mag_filter(GETexture* inTexture, unsigned long inMode) { inTexture->SetStageMagFilterMode(inMode, 0xffffffff); }
void ee_rtsmmcpp_texture_mip_filter(GETexture* inTexture, unsigned long inMode) { inTexture->SetStageMipFilterMode(inMode, 0xffffffff); }
void ee_rtsmmcpp_texture_addressing(GETexture* inTexture, unsigned long inMode) { inTexture->SetStageTextureAddressingMode(inMode, 0xffffffff); }
void ee_rtsmmcpp_surf_clear_black(GESurface* inSurface) { inSurface->Clear(sBlack, 1.0f); }
}
