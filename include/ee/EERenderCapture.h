// New (port), TEST ONLY: RFIL, the render fill's capture (the object plan's Lane R gate; tools/rfil.py). With
// EE_RENDER_CAPTURE=<absolute file> every FillRenderLists that fills appends one record: the frame's keys (the frame
// counter, the physics tick, the sway clock, the camera), then a digest of every field of every EERenderUnit and
// EERenderShadow it filled (pointers as their content's name and their order of first use in the record, never their
// address), the six texture maps in their order, the selected and picked lists, the ghost records, the ground circles
// and the frame's counters. Off (the default), one cached test per frame. Reads; writes nothing to the game.
//
// A record: u32 tag 'RFIL', u32 payload size, the payload (little-endian; tools/rfil.py reads it).
#ifndef EE_EERENDERCAPTURE_H
#define EE_EERENDERCAPTURE_H

#include <stdint.h>
#include <vector>

#include "rust/render/gfx.h"

struct EERenderFrameInfo;

// The render lists as ee_gfx reads them in place (rfil.rs, units_lists.rs: EEROUnits draws from them): the pools
// and vectors, the multimaps flattened into ioPairs, which must outlive the frame's use; the keys are left 0
void EERenderListsView(EeGfxRfilFrame& outFrame, std::vector<EeGfxRfilPair> (&ioPairs)[6]);

// FillRenderLists, once the lists are filled (the world's lock still held): inSwayClock is the objects' sway clock.
// The renderer calls it through gEERenderCaptureFill (EEGlobals.cpp), which EERenderCapture.cpp sets at static
// initialization: the renderer names no symbol of the capture's record
void EERenderCaptureFill(const EERenderFrameInfo& inInfo, uint32_t inSwayClock, float inUnitCount);
extern void (*gEERenderCaptureFill)(const EERenderFrameInfo& inInfo, uint32_t inSwayClock, float inUnitCount);

#endif
