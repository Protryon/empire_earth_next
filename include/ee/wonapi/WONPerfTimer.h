// PerfTimer static API (2a3820..2a3990).
#ifndef EE_WONAPI_WONPERFTIMER_H
#define EE_WONAPI_WONPERFTIMER_H
#include <string>
// The instance members below are from the WON SDK;
// the statics have original addresses,
// the instance methods have no original address identified.
namespace WONAPI {
class PerfTimer {
protected:
    bool mStarted;
    const char* mName;
public:
    PerfTimer(const char* theName=NULL);
    ~PerfTimer();
    void Start();
    void Stop();
    static void DumpTimesToStr(std::string&);
    static void DumpTimesToFile(const char*);
    static void GlobalStart(const char* name=NULL); //2a3940, cdecl char pointer, ret 0
    static void GlobalStop(const char* name=NULL); //2a3990, cdecl char pointer, ret 0
    static double GetCountedTime(const char* name=NULL); //2a3820, cdecl, FPU result
    static double GetElapsedTime(const char* name=NULL); //2a3900, cdecl, FPU result
    static double GetPercentTime(const char* name=NULL); //2a3910, cdecl, FPU result
};
}
#endif
