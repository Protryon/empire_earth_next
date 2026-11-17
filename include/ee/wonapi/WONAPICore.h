#ifndef EE_WONAPI_WONAPICORE_H
#define EE_WONAPI_WONAPICORE_H
#include "ee/wonapi/WONTimerThread.h"
#include "rust/won_net/apicore.h"
#include "rust/won_net/hostnative.h"
namespace WONAPI {
// the core's C++ calls: the socket thread's virtuals, its deletion, the default one (AsyncOp.cpp)
const EeWonApiHooks* WonApiHooks();
// WON API core singleton (0xd0 bytes in the original: 2f2750..2f2f3f): socket, timer and pump threads;
// WONAPICoreEx adds the setters. A Rust object since R6 WONLobby round 12 (rust/crates/ee_won_net/src/apicore.rs
// EeWonApiCore: the op container, the timer thread, the pump thread, the flags, the version, the WON directory's
// rule, and Startup, Shutdown, Pump, WriteVersion and the static calls); WONAPI.cpp is deleted. This class keeps the
// vtable (the destructor, Startup, Shutdown) and the calls the ops make; the socket thread stays a C++
// SocketThread* the game may replace, reached through WonApiHooks().
class WONAPICore {
public:
    static void QueueOp(AsyncOp* op) { ee_won_api_queue_op(static_cast<void*>(op)); } //2f2e60, SDK QueueInContainer
    static void AddTimerOp(AsyncOp* op,unsigned long t) //2f2ea0, SDK AddToTimerThread
    { ee_won_api_add_timer_op(static_cast<void*>(op),static_cast<uint32_t>(t)); }
    static void RemoveTimerOp(AsyncOp* op) { ee_won_api_remove_timer_op(static_cast<void*>(op)); } //2f2ed0
    // 2f2da0: the directory set on the core, else the data root (a copy per thread, valid until its next call)
    static const std::string& GetDefaultFileDirectory()
    {
        static thread_local std::string sDirectory;
        ee_won_api_file_directory(WonStringSink,&sDirectory);
        return sDirectory;
    }
    virtual ~WONAPICore() { ee_won_api_free(this->mCore); } //slot 0, 2f2980
    virtual void Startup() { ee_won_api_startup(this->mCore); }   //slot 4, 2f2b50
    virtual void Shutdown() { ee_won_api_shutdown(this->mCore); }  //slot 8, 2f2c80
    void Pump(unsigned long ms,int maxOps=0) { ee_won_api_pump(this->mCore,static_cast<uint32_t>(ms),maxOps); } //2f2d20
    static WONAPICore* GetInstance() { return static_cast<WONAPICore*>(ee_won_api_instance()); }
    static bool IsStarted() { return ee_won_api_is_started(); } //2f2d00
protected:
    friend class SocketOp;
    static void AddToSocketThread(SocketOp* op) { ee_won_api_socket_op(static_cast<void*>(op),true); } //2f2ef0
    static void RemoveFromSocketThread(SocketOp* op) { ee_won_api_socket_op(static_cast<void*>(op),false); } //2f2f20
    // 2f28d0 (Init 2f2750): version "wonapi121401", socket and timer threads on, no pump thread; Winsock started
    WONAPICore(const char*) : mCore(ee_won_api_new(static_cast<void*>(this),WonCore(),WonApiHooks())) {}
    EeWonApiCore* mCore; // owned
private:
    WONAPICore(WONAPICore&);
    WONAPICore& operator=(WONAPICore&);
};
class WONAPICoreEx : public WONAPICore {
public:
    WONAPICoreEx() : WONAPICore("") {} //2f2f40, implicit dtor 2f2f80
    // each before start-up only
    void SetDefaultFileDirectory(const char* inDirectory) { ee_won_api_set_directory(this->mCore,inDirectory); }
    void SetSocketThread(SocketThread* inThread) { ee_won_api_set_socket_thread(this->mCore,static_cast<void*>(inThread)); }
    void SetDoSocketAndTimerThreads(bool inOn) { ee_won_api_set(this->mCore,1,inOn); }
    void SetDoPumpThread(bool inOn) { ee_won_api_set(this->mCore,0,inOn); } //setter 2f2f90
};
}
#endif
