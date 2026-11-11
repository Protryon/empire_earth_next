// New (port), TEST ONLY: the game messages' records of the simulation capture (EEMessageCapture.cpp; ee/EESimCapture.h;
// tools/sim_capture.py NM**; docs/RUST_PLAN.md R5b.4). Off unless EE_SIM_CAPTURE is set.
#ifndef EE_EEMESSAGECAPTURE_H
#define EE_EEMESSAGECAPTURE_H

#include <vector>

class NEMessage;

// one NMIM record (NMSW inside the sweep) for a message the game factory just built from inData
void EEMessageCaptureImport(NEMessage* inMessage, const unsigned char* inData);
// one NMEX record for a game message the physics executes (its members and its bytes), then the message built again
// from its bytes (an NMIM record): in a single-player game the messages reach the physics unencoded
void EEMessageCaptureExecuted(NEMessage* inMessage);
// every game message type over edge payloads, and the box selection's packing (NMSW, NMCU); once per game
void EEMessageCaptureSweep();
// EEMessage.cpp's CompressUnits then DecompressUnits over its bytes (the box selection's packing); the byte count
unsigned char EEMessageCaptureCompress(std::vector<unsigned long>& inUnits, unsigned long inOwner, unsigned char* outBuffer,
                                       std::vector<long>& outUnits);

// the Execute decisions' kinds (ee/EEMessageExecute.h): one NMDC record per call, the kind and the call's values
// (inputs, then outputs) as 64-bit integers (a float's bits)
enum EEMessageDecisionKind {
    kDecisionSelectionMarksLocal = 1,
    kDecisionSelectionCopies,
    kDecisionCommandRuns,
    kDecisionSelectionPasses,
    kDecisionAddRemove,
    kDecisionTradePrice,
    kDecisionExchangeStep,
    kDecisionTributeOrder,
    kDecisionTributeViewer,
    kDecisionDisbandRefund
};
void EEMessageCaptureDecision(unsigned int inKind, const long long* inValues, unsigned int inCount);
// the decisions over plain edge values (once) and over the real players and units (every call): NMDC records
void EEMessageCaptureDecisionSweep();

#endif
