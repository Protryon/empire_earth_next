// C:\EEX\Empire Earth\AI Thread\EEAutoPathRequest.h (RECONSTRUCTION; file name inferred).
// A queued path request (RTTI EEAutoPathRequest > UReferenceCounter; vtable 0x43c430, one slot: the deleting dtor;
// ctor 0xa5c31 first called from 0xa6948 in EEPathManager.cpp; the manager's gPathManager.CancelAutoPathRequest releases them).
// port (the paths lane, round 2): a request is a Rust record (ee_sim::astar request.rs: the tiles of its found path,
// its walker flags, ends, player, id, length and done flag, reference counted as the UReferenceCounter was); the C++
// keeps the opaque handle and reads it through rust/sim/astar.h's ee_path_request_* (done, tiles, Reference,
// Dereference). The saved record (save 0.277) and the owners waiting for a loaded request (0.278) are Rust too.
#ifndef EE_EEAUTOPATHREQUEST_H
#define EE_EEAUTOPATHREQUEST_H

class EEAutoPathRequest;   // the Rust request's handle (request.rs)

#endif
