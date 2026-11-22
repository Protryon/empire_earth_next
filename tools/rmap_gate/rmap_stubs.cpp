// The little of the engine the generator, the reader and the geometry units reference that the dump tool does not
// compile for real: a console tool needs no scheduler, debugger output or platform layer; URandom is the two lines
// of the engine's own definition (in GEBitmap.cpp there).
#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <mbstring.h>
#include "lle/LowLevelEngine.h"
#include "platform/PlatFile.h"

unsigned long URandom::NextInSeries()
{
    this->mSeed = (this->mMultiplier * this->mSeed + this->mIncrement) % this->mModulus;
    return this->mSeed;
}
float URandom::Generate()
{
    return (float)((double)this->NextInSeries() / (double)this->mModulus);
}

// (TMGetTime and ESDebugOut are header inlines over Rust now)
UString gStartupVolume;
UString gStartupDirectory;
