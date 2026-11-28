// New (port), TEST ONLY: the simulation capture (docs/RUST_PLAN.md R5b, the leaves' golden gates). With
// EE_SIM_CAPTURE=<absolute file> a game appends one binary record per call of a captured leaf (formations, ...): the
// call's plain-data inputs and its outputs, so that a tool can replay the inputs through another implementation and
// compare the outputs (tools/sim_capture.py). Off (the default), Active() is one cached test.
//
// A record: u32 tag, u32 payload size, the payload (little-endian u32/i32/f32 fields, the tag's layout). Records are
// written whole under a lock (the writer is Rust: ee_sim app capture.rs).
#ifndef EE_EESIMCAPTURE_H
#define EE_EESIMCAPTURE_H

#include <stdint.h>
#include <vector>

#include "rust/sim/app.h"   // the writer (ee_sim app capture.rs)

class EESimCaptureRecord {
public:
};

#endif
