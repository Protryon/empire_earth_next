// ee/EEAIKindIDs.h (RECONSTRUCTION). Named integer constants; each comment gives the evidence for its name;
// (provisional name) marks an inferred one.
#ifndef EE_EEAIKINDIDS_H
#define EE_EEAIKINDIDS_H

// EEAIArea::mAreaType (+0x64): the area class each creator stores; EECPILocation::mType uses the same ids (Update 0x480a57 re-finds the area of that class at the tile)
enum EEAIAreaKind {
    kAIAreaNone = 0,         // the EEAIArea ctor 0x4110cb stores 0; EECPILocation::Update 0x480a57 stores 0 when no area is found (provisional name)
    kAIAreaTown = 5,         // RTTI EETown: the EETown ctor stores 5; EECPILocation::Update: the player's town containing the tile
    kAIAreaHill = 7,         // RTTI EEAIHill: EEAIAStarHillGoal::CreateHill stores 7; EECPILocation::Update: GetHillAt; EEEMStorm::Begin dynamic_casts GetRandomArea(7) to EEAIHill
    kAIAreaRectangleArea = 8,  // RTTI EEAIRectangleArea: EEAIContinentManager::CreateScenarioAreas stores 8; EECPILocation::Update: GetRectangleAreaAt
    kAIAreaHerd = 9,         // RTTI EEHerd: EEHerd::Clear stores 9
};

// EEAIContinent::mCategory (+0x11a): the terrain class of a continent; EEAIAStarContinentGoal::AcceptTile 0x41575c takes it from the fill's first tile (also the path-area/sector goals' copies and the manager's category filters)
enum EEAIContinentCategory {
    kContinentCategoryNone = 0,   // unset (EEAIAStarContinentGoal ctor 0x4155c0 / CreateContinent 0x4156b2 reset; AcceptTile 0x41575c classifies the first tile while 0); as a filter any category (GetLargestContinent 0x422952; GetRandomContinent 0x422dc8: any but the map edge) (provisional name)
    kContinentCategoryLand = 1,   // setrilo ai_continents.h:215 '1 land'; AcceptTile 0x41575c: any terrain but 1, 9, 0xc
    kContinentCategoryWater = 2,  // setrilo ai_continents.h:215 '2 deep water'; AcceptTile 0x41575c: terrain 1; CreateContinents counts it into mNumWaterTiles
    kContinentCategoryCliff = 3,  // setrilo ai_continents.h:215 '3 cliff'; AcceptTile 0x41575c: terrain 9 (the cliff terrain EEMap::UpdateCliffs sets)
    kContinentCategoryMapEdge = 5,  // AcceptTile 0x41575c: EEMap::IsEdgeTile tiles (setrilo's base-game rim); EEHerd 0x4a4aa2 leaves the map through a random one (provisional name)
};

// The region ids a path area carries (EEAIPathArea +0x84..+0x8a): EEAIContinentManager::CreatePathAreaKind 0x4204ea grows one kind per pass (EEAIAStarPathAreaGoal::mMode); GetContinent 0x422b86, GetAdjacentAreaID 0x421a18 and EEAIContinent::GetShoreInRegions select the id by it
enum EEAIPathKind {
    kAIPathLandMass = 2,   // setrilo ai_continents.h:153; GetContinent 0x422b86: mLandMassID; CreatePathAreaKind 0x4204ea: not terrain 1 / 0xc
    kAIPathNonSpace = 4,   // GetContinent 0x422b86: mW84; CreatePathAreaKind 0x4204ea and GetAdjacentAreaID 0x421a18: tiles not terrain 0xc (and not on the map's diagonal edges) (provisional name)
};

// The AI's kind of a tile and of the path area / sub-sector grown from it (EEAIGetTileKind 0x41ed2c, EEAIAStarPathAreaGoal::AcceptTile, EEAIAStarPathSectorGoal::ClassifyTile; EEAIPathArea::mKind, EEAIPathSubSector::mKind); shore / near-shore by the manager's tile value (UpdateTileValues 0x4231d7: < 0 a shore tile, > 0 next to shore tiles)
enum EEAITileKind {
    kAITileForest = 4,           // setrilo ai_continents.h:149 '4 forest'; EEAIAStarPathAreaGoal::CreateAreas 0x417510 makes an EEAIForest for it; EEAIPathSubSector::GetBlockingUnit 0x42bf45: a tree
};

#endif // EE_EEAIKINDIDS_H
