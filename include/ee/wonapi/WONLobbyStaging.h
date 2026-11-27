// StagingLogic: game staging (join requests, ready state, pings, start, kicks); 0x5c bytes in the original
#ifndef EE_WONAPI_WONLOBBYSTAGING_H
#define EE_WONAPI_WONLOBBYSTAGING_H
#include "ee/wonapi/WONLobbyLogic.h"
#include "ee/wonapi/WONLobbyModel.h"
namespace WONAPI {
// RTTI: StagingLogic > LobbyLogic > RefCount; vtable 0x855ae0 with six slots; constructor 0x7ef160.
// A Rust object since round 20 (rust/crates/ee_won_lobby/src/stagingobj.rs): its methods and scalar state (the join
// status, the staging and ready flags, the last all-ready time) are Rust's, its calls out the host's ops
// (WONLobby/LobbyLogicHost.cpp). Since R6 WON round 36 it is the RustLogic of kind Staging
// (ee/wonapi/WONLobbyLogic.h): its methods are the shell's Call(EeWonStagingCall, ..), the SmartPtrs the lobby reads
// the shell's slots, named here.
enum { kStagingGame, kStagingMyClient, kStagingJoinOp }; // mSlot: mGame (0x40), mMyClient (0x44), mJoinOp (0x48)
enum { kStagingCreateGame }; // mDialog: mCreateGameDialog (0x58)
// The room message from the buffer's position (ee_won reads its type and fixed fields); LobbyLogicHost.cpp
void StagingHandleMessage(RustLogic* inStaging, ReadBuffer& inBuffer, LobbyClient* inClient);
// The same over a buffer of the caller's bytes
inline void StagingHandleBytes(RustLogic* inStaging, ReadBuffer inBuffer, LobbyClient* inClient) { StagingHandleMessage(inStaging, inBuffer, inClient); }
// A host case's read: its result, or Threw for a read past the buffer's end
template <class F> intptr_t LobbyReadGuard(F inRead)
{
    try { return (intptr_t)inRead(); } catch (ReadBufferException&) { return (intptr_t)EeWonLobbyResult::Threw; }
}
// A message op's message: the C++ ByteBuffer, or one over the bytes Rust built (held by outHold), or none
const ByteBuffer* LobbyHostMessage(void* inBuffer, intptr_t inBytes, intptr_t inLength, ByteBufferPtr& outHold);
}
#endif
