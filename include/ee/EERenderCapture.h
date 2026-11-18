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

struct EERenderFrameInfo;

// FillRenderLists, once the lists are filled (the world's lock still held): inSwayClock is the objects' sway clock
void EERenderCaptureFill(const EERenderFrameInfo& inInfo, uint32_t inSwayClock, float inUnitCount);

#endif
