// EEMapDimensions.h (RECONSTRUCTION; file name inferred; provisional names). Two unsigned globals
// just before gWorld (0x530d40): the map's width and height in tiles. Tile coordinates are compared against them
// unsigned (`cmp x, [0x530d34]; jae`: EECPILocation 0x80a57; 19 functions read them).
#ifndef EE_EEMAPDIMENSIONS_H
#define EE_EEMAPDIMENSIONS_H

extern unsigned long& gMapWidth;             // 0x530d34
extern unsigned long& gMapHeight;            // 0x530d38

#endif
