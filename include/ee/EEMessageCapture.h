// New (port), TEST ONLY: the game messages' records of the simulation capture (EEMessageCapture.cpp; ee/EESimCapture.h;
// tools/sim_capture.py NM**; docs/RUST_PLAN.md R5b.4). Off unless EE_SIM_CAPTURE is set.
#ifndef EE_EEMESSAGECAPTURE_H
#define EE_EEMESSAGECAPTURE_H

#include <vector>

class NEMessage;

// one NMIM record (NMSW inside the sweep) for a message the game factory just built from inData
void EEMessageCaptureImport(NEMessage* inMessage, const unsigned char* inData);
// every game message type over edge payloads, and the box selection's packing (NMSW, NMCU); once per game
void EEMessageCaptureSweep();
// EEMessage.cpp's CompressUnits then DecompressUnits over its bytes (the box selection's packing); the byte count
unsigned char EEMessageCaptureCompress(std::vector<unsigned long>& inUnits, unsigned long inOwner, unsigned char* outBuffer,
                                       std::vector<long>& outUnits);

#endif
