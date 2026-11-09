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

extern "C" {
FILE* PlatFOpen(const char* inPath, const char* inMode) { return fopen(inPath, inMode); }
void PlatFindClose(PlatFind* inFind) {}
int PlatStrICmp(const char* a, const char* b) { return _stricmp(a, b); }
int PlatStrNICmp(const char* a, const char* b, size_t n) { return _strnicmp(a, b, n); }
int PlatMbsCmp(const unsigned char* a, const unsigned char* b) { return _mbscmp(a, b); }
int PlatMbsICmp(const unsigned char* a, const unsigned char* b) { return _mbsicmp(a, b); }
int PlatMbsNBCmp(const unsigned char* a, const unsigned char* b, size_t n) { return _mbsnbcmp(a, b, n); }
int PlatMbsNBICmp(const unsigned char* a, const unsigned char* b, size_t n) { return _mbsnbicmp(a, b, n); }
unsigned char* PlatMbsLwr(unsigned char* s) { return _mbslwr(s); }
unsigned char* PlatMbsUpr(unsigned char* s) { return _mbsupr(s); }
void PlatSleep(uint32_t) {}
}
unsigned long TMGetTime() { return 0; }
void ESDebugOut(const char*, ...) {}
void TSCriticalSection::Enter() {}
void TSCriticalSection::Leave() {}
unsigned long TSThread::GetExitCode() { return 0; }
void TSThread::SetName(UString&) {}
UString gStartupVolume;
UString gStartupDirectory;
