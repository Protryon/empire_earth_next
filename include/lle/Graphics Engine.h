// C:\EEX\Low-Level Engine\Graphics Engine\Graphics Engine.h (RECONSTRUCTION). The Graphics Engine's start-up and
// pixel-format packing (GERasterizer.cpp: Graphics Engine.cpp was folded in R4.3 part 15).
#ifndef LLE_GRAPHICSENGINE_H
#define LLE_GRAPHICSENGINE_H

#include "lle/LLEForward.h"
#include "lle/GEColor.h"
#include "rust/render/ge.h"

// Starts the graphics engine (the directory is not read; ee_render ge startup.rs)
inline long GEInitialize(FSFileSpec&, HINSTANCE__* const inInstance) { return ee_ge_initialize(inInstance); }
// The pixel formats (ee_ge pixel.rs): a colour and alpha (0..1 floats, or 8 bits each) packed into a pixel, a
// pixel's channels scaled to 8 bits
inline unsigned long GEPackColor(unsigned long inFormat, const GEColor& inColor, float inAlpha)
    { return ee_ge_pack_color_float(inFormat, inColor.GetRed(), inColor.GetGreen(), inColor.GetBlue(), inAlpha); }
inline unsigned long GEPackColor(unsigned long inFormat, unsigned long inRed, unsigned long inGreen,
                                 unsigned long inBlue, unsigned long inAlpha)
    { return ee_ge_pack_color(inFormat, inRed, inGreen, inBlue, inAlpha); }
inline unsigned char GEUnpackRed(unsigned long inFormat, unsigned long inColor) { return ee_ge_unpack_red(inFormat, inColor); }
inline unsigned char GEUnpackGreen(unsigned long inFormat, unsigned long inColor) { return ee_ge_unpack_green(inFormat, inColor); }
inline unsigned char GEUnpackBlue(unsigned long inFormat, unsigned long inColor) { return ee_ge_unpack_blue(inFormat, inColor); }

#endif
