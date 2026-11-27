// C:\EEX\Empire Earth\Random Map\... (RECONSTRUCTION; file name inferred; function names are provisional). The
// game's list of random map generators: EERandomMap.cpp's sRandomMapGenerators (a vector of 0x38-byte entries at
// 0x565570; assert "theGeneratorIndex < EERandomMap::sRandomMapGenerat..." at 0x28655a). Kept
// out of ee/EERandomMap.h (the generator PIMs' view, which needs the U2D headers whose keys cost `$E` chains).
#ifndef EE_EERANDOMMAPGENERATORS_H
#define EE_EERANDOMMAPGENERATORS_H

#include "rust/sim/rmap.h"   // the list is ee_sim rmap object.rs' (initialize_generators)

inline unsigned long EERMGetNumberOfGenerators() { return ee_rmap_generator_count(); }   // 0x286714 (size of the vector)
// 0x28672f (entry +0x14, a UWideString; by value: Rust's)
inline UWideString EERMGetGeneratorDisplayName(unsigned long inIndex) { return UWideString(ee_rmap_generator_text(inIndex, true)); }
inline UWideString EERMGetGeneratorName(unsigned long inIndex) { return UWideString(ee_rmap_generator_text(inIndex, false)); }   // 0x2867a8 (entry +0x00)
// 0x2867b6: the index of the generator named inName (entry +0x00); 3 when there is none
inline long EERMFindGenerator(const UWideString& inName, unsigned long& outIndex) { return ee_rmap_find_generator(inName, &outIndex); }
// 0x286741: the display name (entry +0x14) of the generator named inName, else inName
inline UWideString EERMGetGeneratorDisplayName(const UWideString& inName) { return UWideString(ee_rmap_generator_display(inName)); }

#endif
