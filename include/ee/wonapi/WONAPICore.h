#ifndef EE_WONAPI_WONAPICORE_H
#define EE_WONAPI_WONAPICORE_H
#include "ee/wonapi/WONTimerThread.h"
namespace WONAPI {
// WON API core singleton (0xd0 bytes): socket, timer and pump threads; WONAPICoreEx adds the setters
class WONAPICore {
public:
    static void QueueOp(AsyncOp*); //2f2e60, SDK QueueInContainer
    static void AddTimerOp(AsyncOp*,unsigned long); //2f2ea0, SDK AddToTimerThread
    static void RemoveTimerOp(AsyncOp*); //2f2ed0, SDK RemoveFromTimerThread
    static const std::string& GetDefaultFileDirectory(); //2f2da0
    virtual ~WONAPICore(); //slot 0, 2f2980
    virtual void Startup();   //slot 4, 2f2b50
    virtual void Shutdown();  //slot 8, 2f2c80
    void Pump(unsigned long,int=0); //2f2d20, SDK argument types
    static WONAPICore* GetInstance() { return WONAPICore::mInstance; }
    static bool IsStarted(); //2f2d00
protected:
    friend class SocketOp;
    static void AddToSocketThread(SocketOp*); //2f2ef0, protected SDK dispatch
    static void RemoveFromSocketThread(SocketOp*); //2f2f20
    WONAPICore(const char*); //2f28d0, 0x173 bytes
    void Init(); //2f2750, 0x321 bytes, state initializer
    void WriteVersion(); //2f2a80
    static WONAPICore* mInstance; //external static 5661fc
    AsyncOpContainer mOpContainer; //04..3c, 0x38 bytes
    TimerThread mTimerThread; //3c..8c, 0x50 bytes
    SocketThread* mSocketThread; //8c owned rawpointer, virtual deletion
    bool mIsStarted; //90
    bool mDoSocketAndTimerThreads; //91
    bool mDoPumpThread; //92,naturalpad93
    static bool mHaveWrittenVersion; //external static 566200
    std::string mVersionString; //94..a4
    std::string mFileDirectory; //a4..b4
    class PumpThread : public Thread {
        virtual void ThreadFunc(); //slot 0x10, 2f2fb0
    };
    PumpThread mPumpThread; //b4..d0, 0x1c bytes, no extra fields
private:
    WONAPICore(WONAPICore&);
    WONAPICore& operator=(WONAPICore&);
};
class WONAPICoreEx : public WONAPICore {
public:
    WONAPICoreEx(); //2f2f40, implicit dtor 2f2f80
    void SetDefaultFileDirectory(const char*);
    void SetSocketThread(SocketThread*);
    void SetDoSocketAndTimerThreads(bool);
    void SetDoPumpThread(bool); //setter 2f2f90
};
}
#endif
