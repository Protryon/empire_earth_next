// New (port), TEST ONLY: the app and world lane's records of the simulation capture (tools/sim_capture.py; RUST_PLAN
// R6 "app and world"). Reads the objects; writes nothing.
#ifndef EE_EEAPPCAPTURE_H
#define EE_EEAPPCAPTURE_H

class EECameraControl;

// CAMU: the camera's whole state (the poses, the view, the target, the followed units' ids, the move's numbers, the
// mode and flags, every key of the path) and the viewport's clipping distances, at each Update's start (inPhase 0,
// with its elapsed time) and end (1)
void EEAppCaptureCamera(const EECameraControl& inCamera, unsigned int inPhase, float inElapsedTime);

#endif
