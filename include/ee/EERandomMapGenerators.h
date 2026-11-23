// C:\EEX\Empire Earth\Random Map\... (RECONSTRUCTION; file name inferred; function names are provisional). The
// game's list of random map generators: EERandomMap.cpp's sRandomMapGenerators (a vector of 0x38-byte entries at
// 0x565570; assert "theGeneratorIndex < EERandomMap::sRandomMapGenerat..." at 0x28655a). Kept
// out of ee/EERandomMap.h (the generator PIMs' view, which needs the U2D headers whose keys cost `$E` chains).
#ifndef EE_EERANDOMMAPGENERATORS_H
#define EE_EERANDOMMAPGENERATORS_H

unsigned long EERMGetNumberOfGenerators();                          // 0x286714 (size of the vector)
UWideString EERMGetGeneratorDisplayName(unsigned long inIndex);  // 0x28672f (entry +0x14, a UWideString; by value: Rust's)
UWideString EERMGetGeneratorName(unsigned long inIndex);     // 0x2867a8 (entry +0x00)
// 0x2867b6: the index of the generator named inName (entry +0x00); 3 when there is none
long EERMFindGenerator(const UWideString& inName, unsigned long& outIndex);
// 0x286741: the display name (entry +0x14) of the generator named inName, else inName
UWideString EERMGetGeneratorDisplayName(const UWideString& inName);

#endif
