// New (port), TEST ONLY: the capture of the continent manager's analysis (docs/RUST_PLAN.md R5b.1; ee/EESimCapture.h,
// tools/sim_capture.py). With EE_SIM_CAPTURE set, EEAIContinentManager::BuildRegions appends one record (tag CNTB,
// before it frees its work grids) and every Rebuild one record (tag CNTR) of the whole analysis as the game reads it:
// the manager's counters, the per-tile continent and sub sector ids and tile values, the continent adjacency matrix,
// every continent, path area, path sector, sub sector, choke point, hill, forest and rectangle area with its tiles,
// ids and links, and the shore tiles. The capture reads only: games with it on keep their traces. The golden digests
// (tools/sim_capture/golden_cpp*.txt) come from the C++ analysis before any of it moved to Rust.
#ifndef EE_EEAICONTINENTCAPTURE_H
#define EE_EEAICONTINENTCAPTURE_H

// the records' tags
enum {
    kContinentCaptureBuild = 0x42544e43,    // "CNTB": after BuildRegions
    kContinentCaptureRebuild = 0x52544e43,  // "CNTR": after Rebuild
    kContinentCaptureNextTo = 0x4e544e43,   // "CNTN": every GetTilesNextTo (x, y, id, result, the tiles)
};

// One record of gContinentManager's state under inTag; nothing unless the capture is on
void EEAIContinentCapture(unsigned long inTag);

#endif
