// C:\EEX\RTS Engine\Map\RTSTile.cpp (no __FILE__: no asserts). A map tile's engine side (RTTI RTSTile, vtable
// 0x441dd0). LLE round 8: the bodies are Rust (rust/crates/ee_game/src/rts/tile.rs: the checksum, Load, Save, Dump,
// the unit list's removals and search); the members forward, and the shims below are the C++ calls those bodies make
// (one statement each). The original's addresses and the bodies' history: git history before LLE round 8.
#include <windows.h>
#include <stdio.h>
#include <vector>

#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GETerrainMesh.h"

#include "ee/EEMap.h"
#include "rust/game/rts.h"

// Creates a tile at the given height with no units and terrain type 0
RTSTile::RTSTile(float inHeight)
{
    this->mHeight = inHeight;
    this->mUnits = NULL;
    this->mTerrainType = 0;
    this->mAIValue = false;
}

RTSTile::~RTSTile() {}
void RTSTile::FreeUnitNode(RTSTileUnitNode* inNode) { delete inNode; }
long RTSTile::GetChecksum(unsigned long inPlayerCount) const { return ee_rts_tile_checksum(this, inPlayerCount); }
long RTSTile::Load(unsigned long, unsigned long, FSFile& inFile, unsigned long inPlayerCount) { return ee_rts_tile_load(this, &inFile, inPlayerCount); }
void RTSTile::Dump(FILE* inFile, unsigned long inPlayerCount) const { ee_rts_tile_dump(this, inFile, inPlayerCount); }
void RTSTile::RemoveAllUnits() { ee_rts_tile_remove_all_units(this); }
void RTSTile::RemoveUnit(EEUnit* inUnit) { ee_rts_tile_remove_unit(this, inUnit); }
long RTSTile::Save(FSFile& outFile, unsigned long inPlayerCount) { return ee_rts_tile_save(this, &outFile, inPlayerCount); }
bool RTSTile::HasUnit(EEUnit* inUnit) { return ee_rts_tile_has_unit(this, inUnit); }
void RTSTile::SetTerrainType(long inValue, const EETileLocation&, RTSMap*) { this->mTerrainType = inValue; }

// The shims tile.rs calls (TileCpp)
extern "C" {
long ee_rtstcpp_terrain_type(const RTSTile* inTile) { return inTile->mTerrainType; }
const uint32_t* ee_rtstcpp_visibility(const RTSTile* inTile) { return inTile->mVisibility; }
float* ee_rtstcpp_height_slot(RTSTile* inTile) { return &inTile->mHeight; }
RTSTileUnitNode** ee_rtstcpp_units_slot(RTSTile* inTile) { return &inTile->mUnits; }
EEUnit* ee_rtstcpp_node_unit(RTSTileUnitNode* inNode) { return inNode->mUnit; }
RTSTileUnitNode** ee_rtstcpp_node_next_slot(RTSTileUnitNode* inNode) { return &inNode->mNext; }
void ee_rtstcpp_free_node(RTSTile* inTile, RTSTileUnitNode* inNode) { inTile->FreeUnitNode(inNode); }
long ee_rtstcpp_read_bytes(FSFile* inFile, unsigned char* outData, unsigned long inLength, unsigned long* outRead) { return inFile->Read(reinterpret_cast<char*>(outData), inLength, *outRead); }
long ee_rtstcpp_write_bytes(FSFile* inFile, const unsigned char* inData, unsigned long inLength, unsigned long* outWritten) { return inFile->Write(reinterpret_cast<const char*>(inData), inLength, *outWritten); }
void ee_rtstcpp_puts(FILE* inFile, const char* inText) { fputs(inText, inFile); }
}
