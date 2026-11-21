// New (port), TEST ONLY: the area effects' records of the simulation capture (ee/EEAECapture.h; tools/sim_capture.py,
// every profile): AETP and AEFX. port (round 10, lane C): written by gamecpp ae_capture.rs; this unit sets the hook
// EEAEManager::Tick's walk calls (EEAreaEffect.cpp's LcAeCaptureTick case). Reads the world; writes nothing to it.
#include <stdint.h>

#include "ee/EEAECapture.h"
#include "rust/game/gamecpp.h"

static void EEAECaptureTickImpl(float inElapsed)
{
    ee_gamecpp_ae_capture_tick(inElapsed);
}

static const bool sCaptureHooksSet = (EEAECaptureTick = EEAECaptureTickImpl, true);
