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

// NMDC, the Execute decisions' records, are Rust (ee_sim netmsg decisions.rs: ee_netmsg_capture_decision, and
// ee_netmsg_capture_decision_sweep over the world table's trade rates)

#endif
