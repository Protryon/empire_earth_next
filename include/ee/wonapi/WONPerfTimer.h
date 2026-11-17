// PerfTimer static API (2a3820..2a3990): ee_won's (rust/crates/ee_won_net/src/perftimer.rs, net round 26: PerfTimer.cpp
// is deleted). The instance members are from the WON SDK (no original address identified); a named timer does
// nothing.
#ifndef EE_WONAPI_WONPERFTIMER_H
#define EE_WONAPI_WONPERFTIMER_H
#include <string>
#include "rust/ee_won.h"
namespace WONAPI {
class PerfTimer {
protected:
    bool mStarted;
    const char* mName;
public:
    PerfTimer(const char* theName=NULL) : mStarted(false), mName(theName) { this->Start(); }
    ~PerfTimer() { this->Stop(); }
    void Start() { ee_won_perf_start(this->mName!=NULL,&this->mStarted); } // the outermost timer notes its tick
    void Stop() { ee_won_perf_stop(this->mName!=NULL,&this->mStarted); } // the last one in a run adds its time
    static void DumpTimesToStr(std::string&) {}
    static void DumpTimesToFile(const char*) {}
    static void GlobalStart(const char* =NULL) { ee_won_perf_global_start(); } //2a3940: the counted time cleared
    static void GlobalStop(const char* =NULL) { ee_won_perf_global_stop(); } //2a3990: the run's milliseconds
    static double GetCountedTime(const char* =NULL) { return ee_won_perf_counted_time(); } //2a3820
    static double GetElapsedTime(const char* =NULL) { return ee_won_perf_elapsed_time(); } //2a3900
    static double GetPercentTime(const char* =NULL) { return ee_won_perf_percent_time(); } //2a3910
};
}
#endif
