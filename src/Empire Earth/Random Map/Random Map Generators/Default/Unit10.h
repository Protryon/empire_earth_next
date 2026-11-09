// Unit10.cpp's entry points (the .rmv script reader). Names PROVISIONAL; signatures proven by codegen.
// Requires lle/LowLevelEngine.h and ee/EERandomMap.h.
#ifndef DEFAULT_UNIT10_H
#define DEFAULT_UNIT10_H

// 0xd378: reset the parser state, read <script directory>\<inScriptName>.rmv into ioMap, then rescale and
// validate the values read. Always returns 0. (EERMPIMSetRandomMapValues forwards both arguments.)
long SetRandomMapValues(EERandomMap* ioMap, const UString& inScriptName);

// Empties the neutral placement order (the Rust generator's, kept by Default.cpp) before a script is read.
void ResetPlayerOrder();

#endif
