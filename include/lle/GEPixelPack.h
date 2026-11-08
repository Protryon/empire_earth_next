// port: packs 32-bit B, G, R, x pixels (in memory order) into a surface row of a GEPixelFormatID (16 or 32 bits), for
// the CPU frame buffers copied to the back buffer: the movie player's decoded frames (DX7Movie.cpp) and the WON
// lobby's composited frame (RTSGraphics::RenderLobby).
#ifndef LLE_GEPIXELPACK_H
#define LLE_GEPIXELPACK_H

#include <stdint.h>
#include "lle/GERenderState.h"

// Packs inWidth pixels (4 bytes each: B, G, R, ignored) into outRow in the pixel format inFormat
inline void GEPackBGRXRow(const unsigned char* inBGRX, unsigned char* outRow, unsigned long inWidth,
                          unsigned long inFormat)
{
    unsigned short* the16 = reinterpret_cast<unsigned short*>(outRow);
    uint32_t* the32 = reinterpret_cast<uint32_t*>(outRow);
    for (unsigned long x = 0; x < inWidth; x++, inBGRX += 4) {
        uint32_t b = inBGRX[0], g = inBGRX[1], r = inBGRX[2];
        switch (inFormat) {
        case kPixelFormatA8R8G8B8:
        case kPixelFormatX8R8G8B8:
            the32[x] = 0xff000000u | r << 16 | g << 8 | b;
            break;
        case kPixelFormatA4R4G4B4:
            the16[x] = (unsigned short)(0xf000 | (r >> 4) << 8 | (g >> 4) << 4 | b >> 4);
            break;
        case kPixelFormatA1R5G5B5:
        case kPixelFormatX1R5G5B5:
            the16[x] = (unsigned short)(0x8000 | (r >> 3) << 10 | (g >> 3) << 5 | b >> 3);
            break;
        case kPixelFormatR5G5B5A1:
        case kPixelFormatR5G5B5X1:
            the16[x] = (unsigned short)((r >> 3) << 11 | (g >> 3) << 6 | (b >> 3) << 1 | 1);
            break;
        default:   // kPixelFormatR5G6B5
            the16[x] = (unsigned short)((r >> 3) << 11 | (g >> 2) << 5 | b >> 3);
            break;
        }
    }
}

#endif
