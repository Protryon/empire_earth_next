// C:\EEX\RTS Engine\Physics\RTSPhysics.cpp (__FILE__ 0x4c26c4). The simulation thread's engine side (RTTI
// RTSPhysics, vtable 0x442988). LLE round 8: the bodies are Rust (rust/crates/ee_game/src/rts/physics.rs: the
// construction, the teardown, the thread's loop, the messages, Load, Save, SetSpeed, the integer draw); the members
// forward, and the shims below are the C++ calls those bodies make (one statement each). The original's addresses
// and the bodies' history: git history before LLE round 8.
#include <windows.h>
#include <deque>

#include "lle/LowLevelEngine.h"
#include "lle/TMTimer.h"
#include "lle/NEMessage.h"
#include "lle/NESynchronousServer.h"
#include "platform/PlatSim.h"

#include "ee/RTSGraphics.h"
#include "ee/EEPhysics.h"
#include "ee/EECommunications.h"
#include "rust/game/rts.h"

UString gRTSPhysicsDataKey("RTSPhysics Data");

// The shims physics.rs calls (PhysicsCpp)
extern "C" {
EeRtsPhysicsFields* ee_rtsphyscpp_fields(RTSPhysics* inPhysics) { return inPhysics; }
void** ee_rtsphyscpp_thread_slot(RTSPhysics* inPhysics) { return reinterpret_cast<void**>(&inPhysics->mThread); }
void** ee_rtsphyscpp_created_slot(RTSPhysics* inPhysics) { return reinterpret_cast<void**>(&inPhysics->mThreadCreated); }
void** ee_rtsphyscpp_start_slot(RTSPhysics* inPhysics) { return reinterpret_cast<void**>(&inPhysics->mStartThreadMainLoop); }
void** ee_rtsphyscpp_done_slot(RTSPhysics* inPhysics) { return reinterpret_cast<void**>(&inPhysics->mThreadDone); }
void** ee_rtsphyscpp_tick_slot(RTSPhysics* inPhysics) { return reinterpret_cast<void**>(&inPhysics->mTimerTick); }
void** ee_rtsphyscpp_lock_slot(RTSPhysics* inPhysics) { return reinterpret_cast<void**>(&inPhysics->mServerMessageQueueLock); }
void ee_rtsphyscpp_set_graphics(RTSPhysics* inPhysics, RTSGraphics* inGraphics) { inPhysics->mGraphics = inGraphics; }
void* ee_rtsphyscpp_render_now(RTSPhysics* inPhysics) { return inPhysics->mGraphics->mRenderNow; }
void ee_rtsphyscpp_set_timer(RTSPhysics* inPhysics, TMTimer* inTimer) { inPhysics->mTimer = inTimer; }
void* ee_rtsphyscpp_timer(RTSPhysics* inPhysics) { return inPhysics->mTimer->Rust(); }
const char* ee_rtsphyscpp_thread_name() { return gPhysicsThreadName; }
uint32_t ee_rtsphyscpp_random_seed(RTSPhysics* inPhysics) { return inPhysics->mRandom.GetSeed(); }
void ee_rtsphyscpp_set_random_seed(RTSPhysics* inPhysics, uint32_t inSeed) { inPhysics->mRandom.SetSeed(inSeed); }
float ee_rtsphyscpp_random(RTSPhysics* inPhysics) { return inPhysics->mRandom.Generate(); }
size_t ee_rtsphyscpp_messages_size(RTSPhysics* inPhysics) { return inPhysics->mServerMessages.size(); }
NEMessage* ee_rtsphyscpp_messages_front(RTSPhysics* inPhysics) { return inPhysics->mServerMessages.front(); }
void ee_rtsphyscpp_messages_pop_front(RTSPhysics* inPhysics) { inPhysics->mServerMessages.pop_front(); }
void ee_rtsphyscpp_message_dereference(NEMessage* inMessage) { inMessage->Dereference(); }
void ee_rtsphyscpp_handle_message(RTSPhysics* inPhysics, unsigned long inMessage, void* inFirst, void* inSecond) { inPhysics->HandleMessage(inMessage, inFirst, inSecond); }
void ee_rtsphyscpp_lock(RTSPhysics* inPhysics) { inPhysics->Lock(); }
void ee_rtsphyscpp_unlock(RTSPhysics* inPhysics) { inPhysics->Unlock(); }
void ee_rtsphyscpp_tick(RTSPhysics* inPhysics) { inPhysics->Tick(); }
void ee_rtsphyscpp_idle(RTSPhysics* inPhysics) { inPhysics->Idle(); }
void ee_rtsphyscpp_update_timing(RTSPhysics* inPhysics) { inPhysics->UpdateTiming(); }
EEPhysics* ee_rtsphyscpp_ee_physics(RTSPhysics* inPhysics) { return dynamic_cast<EEPhysics*>(inPhysics); }
unsigned long ee_rtsphyscpp_ticks_to_process(EEPhysics* inPhysics) { return inPhysics->mTicksToProcess; }
NESynchronousServer* ee_rtsphyscpp_sim_server() { return dynamic_cast<NESynchronousServer*>(gCommunications); }
void ee_rtsphyscpp_sim_communications_tick(NESynchronousServer* inServer) { inServer->SimCommunicationsTick(); }
void (*ee_rtsphyscpp_after_tick_hook())() { return gPhysicsAfterTickHook; }
unsigned long ee_rtsphyscpp_cts_processed() { return gCommunications->GetNumberOfCTsProcessedInPhysics(); }
unsigned long ee_rtsphyscpp_current_ct() { return gCommunications->GetCurrentCommunicationsTick(); }
void ee_rtsphyscpp_debug_out(const char* inText) { ESDebugOut("%s", inText); }

// an FSFile's typed reads and writes (the RTS Engine's Rust bodies)
long ee_rtscpp_read_ulong(FSFile* inFile, unsigned long* outValue) { return inFile->Read(*outValue); }
long ee_rtscpp_read_bool(FSFile* inFile, bool* outValue) { return inFile->Read(*outValue); }
long ee_rtscpp_read_float(FSFile* inFile, float* outValue) { return inFile->Read(*outValue); }
long ee_rtscpp_write_ulong(FSFile* inFile, unsigned long inValue) { return inFile->Write(inValue); }
long ee_rtscpp_write_bool(FSFile* inFile, bool inValue) { return inFile->Write(inValue); }
long ee_rtscpp_write_float(FSFile* inFile, float inValue) { return inFile->Write(inValue); }
}
