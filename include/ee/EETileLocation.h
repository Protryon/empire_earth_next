// EETileLocation.h (RECONSTRUCTION; file name inferred; provisional name). A map tile
// location of the game side = U2DPoint<long>: EECPILocation's tile goes to both
// EEMap::GetTileAtLocation and U2DConvexHull<long>::Contains and is assigned from U2DPoint<long> results; its Load/Save were the
// ICF twins of U2DPoint<long>'s 0x12e04 / 0x12e38. The earlier separate class existed only to avoid the U2DPoint key
// chains
// which the exe view (LLE_EXE_HEADERS) no longer has. Measured:
// EEPlayer.cpp's code is unchanged, only manglings move. Requires lle/LowLevelEngine.h, lle/FSFile.h.
#ifndef EE_EETILELOCATION_H
#define EE_EETILELOCATION_H


typedef U2DPoint<long> EETileLocation;

#endif
