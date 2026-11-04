// TEST TOOL ONLY. The test profile compiles it into the exe in place of the Miles mixer unit (tools/build.py
// TEST_SWAPS); never a reconstructed unit. It registers as the same plug-in (gPIMPluginMilesMixer).
#include <windows.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "lle/LowLevelEngine.h"
#include "lle/FSFileSpec.h"
#include "lle/SESample.h"

enum { OK=0, FAILED=1, BUSY=2, NO_SAMPLE=3, PLAYING=5, IDLE=6, BAD=10, OTHER=13, MAX_CHANNELS=64 };
struct Wave { unsigned long refs, bytes, milliseconds; };
struct Channel { SESample* sample; DWORD tick; double position; bool playing; };
static Channel channels[MAX_CHANNELS];
static unsigned long count;
static bool active;
static float volume=1.0f;
static CRITICAL_SECTION lock;
static UString name("TEST ONLY Silent Mixer");
struct Guard { Guard(){EnterCriticalSection(&lock);} ~Guard(){LeaveCriticalSection(&lock);} };

static double duration(SESample* s) { return s && s->GetData() ? ((Wave*)s->GetData())->milliseconds : 0; }
static void update(Channel& c) {
    DWORD now=GetTickCount();
    if(c.playing && c.sample) {
        double length=duration(c.sample);
        c.position+=(DWORD)(now-c.tick)*c.sample->GetPitch();
        if(length<=0) c.playing=false;
        else if(c.position>=length) {
            if(c.sample->GetIsLooping()) c.position=fmod(c.position,length);
            else {c.position=length;c.playing=false;}
        }
    }
    c.tick=now;
}
static void detach(unsigned long i) {
    if(channels[i].sample) channels[i].sample->SetMixerChannel((unsigned long)-1);
    memset(&channels[i],0,sizeof channels[i]);
}
static Channel* bound(SESample* s) {
    if(!s || s->GetMixerChannel()>=count) return NULL;
    Channel* c=&channels[s->GetMixerChannel()];
    return c->sample==s ? c : NULL;
}
static struct LockInit { LockInit(){InitializeCriticalSection(&lock);} } sLockInit;   // (was DllMain)
extern "C" const UString* PIMGetName(){return &name;}
extern "C" unsigned long PIMGetType(){return 0x20736e64;}
extern "C" bool PIMIsValid(){return true;}
extern "C" void SPIMInitializeMixer(void*){}
extern "C" long SPIMActivateMixer(){Guard g;if(!active){active=true;count=16;}return OK;}
extern "C" long SPIMDestroyChannels(){Guard g;for(unsigned long i=0;i<count;i++)detach(i);count=0;return OK;}
extern "C" long SPIMDeactivateMixer(){Guard g;SPIMDestroyChannels();active=false;return OK;}
extern "C" long SPIMCreateChannels(unsigned long n,void*){
    Guard g;if(!active || !n)return BAD;if(count)return BUSY;count=n>MAX_CHANNELS?MAX_CHANNELS:n;return OK;
}
extern "C" unsigned long SPIMGetChannelCount(){Guard g;return count;}
extern "C" unsigned long SPIMGetMaximumChannelCount(){Guard g;return active?MAX_CHANNELS:0;}
extern "C" float SPIMGetCPUUsage(){return 0;}
extern "C" void* SPIMGetDirectSoundInterface(){return NULL;} // Music must be disabled; no fake COM object.
extern "C" unsigned long SPIMGetHighestTimerDelay(){return 0;}
extern "C" float SPIMGetVolume(){Guard g;return volume;}
extern "C" long SPIMSetVolume(float v){Guard g;if(!(v>=0 && v<=1))return BAD;volume=v;return OK;}
extern "C" long SPIMSetLatency(unsigned long){return OK;}
extern "C" bool SPIMProcessMessage(unsigned long,void*,void*){return true;}
extern "C" long SPIMMixAhead(){Guard g;for(unsigned long i=0;i<count;i++)update(channels[i]);return OK;}
extern "C" long SPIMIsChannelBusyPlaying(unsigned long i){
    Guard g;if(i>=count)return BAD;update(channels[i]);return channels[i].playing?PLAYING:IDLE;
}
extern "C" long SPIMStopChannel(unsigned long i){Guard g;if(i>=count)return BAD;if(!channels[i].sample)return BUSY;detach(i);return OK;}
extern "C" long SPIMStopAllChannels(){Guard g;for(unsigned long i=0;i<count;i++)detach(i);return OK;}
extern "C" long SPIMSetChannelSample(unsigned long i,SESample* s){
    Guard g;if(i>=count)return BAD;update(channels[i]);if(channels[i].playing)return PLAYING;
    if(s && s->GetMixerChannel()!=(unsigned long)-1 && s->GetMixerChannel()!=i)return OTHER;
    detach(i);channels[i].sample=s;if(s)s->SetMixerChannel(i);return OK;
}
extern "C" long SPIMPlayChannel(unsigned long i){
    Guard g;if(i>=count)return BAD;Channel& c=channels[i];update(c);
    if(!c.sample)return NO_SAMPLE;if(c.playing)return BUSY;
    c.position=0;c.tick=GetTickCount();c.playing=true;return OK;
}
extern "C" void SPIMDeleteWaveData(SESample* s){
    Guard g;if(!s)return;Channel* c=bound(s);if(c)detach(s->GetMixerChannel());
    Wave* w=(Wave*)s->GetData();if(w && !--w->refs)free(w);s->SetData(NULL);
}
extern "C" long SPIMDuplicateSample(SESample* s,SESample** out){
    Guard g;if(!s || !s->GetData() || !out)return BAD;
    SESample* copy=new SESample(*s);if(!copy)return FAILED;
    Wave* w=(Wave*)s->GetData();w->refs++;copy->SetData(w);*out=copy;return OK;
}
static unsigned long u32(const unsigned char* p){unsigned long v;memcpy(&v,p,4);return v;}
static unsigned short u16(const unsigned char* p){unsigned short v;memcpy(&v,p,2);return v;}
// RIFF metadata only: PCM/ADPCM can be timed without decoding or retaining samples.
static bool wave_info(const unsigned char* p,unsigned long n,unsigned long& rate,unsigned long& block,
                      unsigned long& size,unsigned long& ms){
    if(n<12 || memcmp(p,"RIFF",4) || memcmp(p+8,"WAVE",4))return false;
    unsigned long declared=u32(p+4);if(declared<4 || declared>n-8)return false;n=declared+8;
    unsigned long avg=0;bool fmt=false,data=false;
    for(unsigned long at=12;at<=n-8;){
        unsigned long len=u32(p+at+4),body=at+8;if(len>n-body)return false;
        if(!memcmp(p+at,"fmt ",4) && len>=16){
            unsigned short kind=u16(p+body);if(kind!=1 && kind!=2 && kind!=17)return false;
            rate=u32(p+body+4);avg=u32(p+body+8);block=u16(p+body+12);fmt=true;
        }
        if(!memcmp(p+at,"data",4)){size=len;data=true;}
        if(len==0xffffffff || len+(len&1)>n-body)break;
        at=body+len+(len&1);
    }
    if(!fmt || !data || !rate || !avg || !block || !size)return false;
    double length=(double)size*1000/avg;if(length>2147483647.0)return false;
    ms=(unsigned long)length;if(!ms)ms=1;return true;
}
extern "C" long SPIMLoadWaveData(FSFileSpec* spec,SESample* s){
    Guard g;if(!spec || !s || s->GetData())return BAD;
    bool opened=!spec->GetIsOpen();if(opened && spec->Open(1)!=0)return FAILED;
    unsigned long n=0,got=0,rate=0,block=0,size=0,ms=0;long old=0;
    spec->GetPosition(old);
    bool valid=spec->GetLength(n)==0 && n<=64*1024*1024 && n>=12;
    unsigned char* bytes=valid?(unsigned char*)malloc(n):NULL;
    valid=bytes && spec->SetPosition(0,0,false)==0 && spec->Read(bytes,n,got)==0 && got==n
          && wave_info(bytes,n,rate,block,size,ms);
    free(bytes);if(opened)spec->Close();else spec->SetPosition(old,0,false);
    if(!valid)return FAILED;
    Wave* w=(Wave*)malloc(sizeof(Wave));if(!w)return FAILED;
    w->refs=1;w->bytes=size;w->milliseconds=ms;
    s->SetFilename(spec->GetFile());s->SetData(w);s->SetLengthInBytes(size);
    s->SetSampleBits(block);s->SetLengthInSamples(size/block);s->SetSampleRate(rate);return OK;
}
extern "C" long SPIMGetSamplePlayBackTime(SESample* s,long* total,long* left){
    Guard g;if(!s || !total || !left)return BAD;Channel* c=bound(s);
    if(!c){*total=*left=0;return FAILED;}update(*c);*total=(long)duration(s);*left=(long)(duration(s)-c->position);return OK;
}
extern "C" unsigned long SPIMGetSampleOffset(SESample* s){
    Guard g;Channel* c=bound(s);if(!c)return 0;update(*c);
    double d=duration(s);return d>0?(unsigned long)(c->position*s->GetLengthInBytes()/d):0;
}
extern "C" long SPIMSetSampleOffset(SESample* s,unsigned long offset){
    Guard g;if(!s || offset>s->GetLengthInBytes())return BAD;Channel* c=bound(s);
    if(c){update(*c);c->position=s->GetLengthInBytes()?duration(s)*offset/s->GetLengthInBytes():0;}return OK;
}
extern "C" void SPIMSetSampleLooping(SESample* s,bool b){Guard g;if(s){Channel* c=bound(s);if(c)update(*c);s->SetTheIsLooping(b);}}
extern "C" long SPIMSetSamplePan(SESample* s,float p){Guard g;if(!s || !(p>=-1 && p<=1))return BAD;s->SetThePan(p);return OK;}
extern "C" long SPIMSetSamplePitch(SESample* s,float p){Guard g;if(!s || !(p>0 && p<=16))return BAD;Channel* c=bound(s);if(c)update(*c);s->SetThePitch(p);return OK;}
extern "C" long SPIMSetSampleVolume(SESample* s,float v){Guard g;if(!s || !(v>=0 && v<=1))return BAD;s->SetTheVolume(v);return OK;}

#include "lle/PIMModule.h"

static const PIMEntry sEntries[] = {
    PIM_ENTRY(PIMGetName),
    PIM_ENTRY(PIMGetType),
    PIM_ENTRY(PIMIsValid),
    PIM_ENTRY(SPIMActivateMixer),
    PIM_ENTRY(SPIMCreateChannels),
    PIM_ENTRY(SPIMDeactivateMixer),
    PIM_ENTRY(SPIMDeleteWaveData),
    PIM_ENTRY(SPIMDestroyChannels),
    PIM_ENTRY(SPIMDuplicateSample),
    PIM_ENTRY(SPIMGetCPUUsage),
    PIM_ENTRY(SPIMGetChannelCount),
    PIM_ENTRY(SPIMGetDirectSoundInterface),
    PIM_ENTRY(SPIMGetHighestTimerDelay),
    PIM_ENTRY(SPIMGetMaximumChannelCount),
    PIM_ENTRY(SPIMGetSampleOffset),
    PIM_ENTRY(SPIMGetSamplePlayBackTime),
    PIM_ENTRY(SPIMGetVolume),
    PIM_ENTRY(SPIMInitializeMixer),
    PIM_ENTRY(SPIMIsChannelBusyPlaying),
    PIM_ENTRY(SPIMLoadWaveData),
    PIM_ENTRY(SPIMMixAhead),
    PIM_ENTRY(SPIMPlayChannel),
    PIM_ENTRY(SPIMProcessMessage),
    PIM_ENTRY(SPIMSetChannelSample),
    PIM_ENTRY(SPIMSetLatency),
    PIM_ENTRY(SPIMSetSampleLooping),
    PIM_ENTRY(SPIMSetSampleOffset),
    PIM_ENTRY(SPIMSetSamplePan),
    PIM_ENTRY(SPIMSetSamplePitch),
    PIM_ENTRY(SPIMSetSampleVolume),
    PIM_ENTRY(SPIMSetVolume),
    PIM_ENTRY(SPIMStopAllChannels),
    PIM_ENTRY(SPIMStopChannel),
    { NULL, NULL },
};
extern const PIMPlugin gPIMPluginMilesMixer = { "Miles Sound System Mixer.dll", sEntries };
