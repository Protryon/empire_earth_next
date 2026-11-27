// New (port), TEST ONLY: the game messages' records of the simulation capture (gamecpp msg_capture.rs; ee/EESimCapture.h;
// tools/sim_capture.py NM**; docs/RUST_PLAN.md R5b.4). Off unless EE_SIM_CAPTURE is set.
#ifndef EE_EEMESSAGECAPTURE_H
#define EE_EEMESSAGECAPTURE_H

#include <stdint.h>

class NEMessage;

// the records are Rust's (ee_game gamecpp msg_capture.rs; declared here as rust/game/gamecpp.h does)
extern "C" void ee_gamecpp_msgcap_import(void* message, const uint8_t* data);
extern "C" void ee_gamecpp_msgcap_executed(void* message);
extern "C" void ee_gamecpp_msgcap_sweep();


// NMDC, the Execute decisions' records, are Rust (ee_sim netmsg decisions.rs: ee_netmsg_capture_decision, and
// ee_netmsg_capture_decision_sweep over the world table's trade rates)

#endif
