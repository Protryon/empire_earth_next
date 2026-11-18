// C:\EEX\Low-Level Engine\Graphics Engine\Graphics Engine.h (RECONSTRUCTION). The Graphics Engine's start-up and
// pixel-format packing (GERasterizer.cpp: Graphics Engine.cpp was folded in R4.3 part 15).
#ifndef LLE_GRAPHICSENGINE_H
#define LLE_GRAPHICSENGINE_H

class FSFileSpec;
class GEColor;

LLE_API long GEInitialize(FSFileSpec& inDirectory, HINSTANCE__* const inInstance);
LLE_API unsigned long GEPackColor(unsigned long inFormat, const GEColor& inColor, float inAlpha);
LLE_API unsigned long GEPackColor(unsigned long inFormat, unsigned long inRed, unsigned long inGreen,
                                  unsigned long inBlue, unsigned long inAlpha);
LLE_API unsigned char GEUnpackRed(unsigned long inFormat, unsigned long inColor);
LLE_API unsigned char GEUnpackGreen(unsigned long inFormat, unsigned long inColor);
LLE_API unsigned char GEUnpackBlue(unsigned long inFormat, unsigned long inColor);

#endif
