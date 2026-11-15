// (RECONSTRUCTION; file name inferred; provisional struct and member names.) The
// display devices Empire Earth.cpp enumerates at start-up (the vector at 0x5319f8; 0x143ece, 0x1482ee.., 0x148514
// fill and free it): every rasterizer with its screens, every screen with its video modes. The game settings form
// lists them (0x1d7a54, 0x1d8a51, 0x1d8c79). Layout from the element strides (0x28, 0x28, 0x2c) and the reads.
// Requires <vector>, lle/UWideString.h, lle/GEScreen.h (GEVideoMode).
#ifndef EE_EEDISPLAYDEVICES_H
#define EE_EEDISPLAYDEVICES_H

struct EEDisplayMode {                      // sizeof 0x2c
    UWideString mName;                      // +0x00
    GEVideoMode mMode;                      // +0x14 (the settings collect these)
    unsigned long mIndex;                   // +0x28
};

struct EEDisplayScreen {                    // sizeof 0x28
    UWideString mName;                      // +0x00 (the screen combo box's lines)
    std::vector<EEDisplayMode> mModes;      // +0x14
    unsigned long mIndex;                   // +0x24 the GERasterizer screen index
};

struct EEDisplayRasterizer {                // sizeof 0x28
    UWideString mName;                      // +0x00 (the rasterizer list's lines; compared with "Rasterizer Name")
    std::vector<EEDisplayScreen> mScreens;  // +0x14
    unsigned long mIndex;                   // +0x24 the GERasterizer index
};

extern std::vector<EEDisplayRasterizer> gDisplayRasterizers;   // 0x5319f8

#endif
