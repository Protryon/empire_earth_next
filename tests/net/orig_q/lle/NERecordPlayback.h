// C:\EEX\Low-Level Engine\Network Engine\NERecordPlayback.h (RECONSTRUCTION; NERecordPlayback.cpp 0x45535..0x45c00).
// Records the multiplayer message stream to a file (timestamped) or plays it back through the message callback
// on its own thread, honouring the recorded timing (slow mode stretches it 45x). Inline members are exported
// COMDATs emitted by this unit's own inline block (0x45539..0x4555d).
#ifndef LLE_NERECORDPLAYBACK_H
#define LLE_NERECORDPLAYBACK_H

class FSFileSpec;
class NEMessage;
class TSEvent;
class TSThread;

enum NERecordPlaybackMode {                 // mMode (+0x28); provisional names except knerpmPlaying (assert text)
    kRecordModePassing = 1,                      // messages go straight to the callback
    kRecordModeRecording = 2,
    kRecordModePlaying = 3,
    knerpmPlaying = kRecordModePlaying   // spelled this way in the original's assert strings
};

class LLE_API NERecordPlayback {           // proven sizeof 0x2c (implicit operator= copies 0xb dwords)
public:
    NERecordPlayback(void (*inCallbackFunction)(NEMessage*, void*), void* inCallbackData);
    ~NERecordPlayback();
    long PassMode();
    bool PlaybackComplete();
    long PlaybackMode(FSFileSpec* inPlaybackFile);
    long PlaybackMode(UString& inAddress, unsigned long& inPort);
    void Process(NEMessage* inMessage);
    long RecordMode(FSFileSpec* inRecordFile);
    long RecordMode(UString& inAddress, unsigned long& inPort);
    // inline block order (0x45535 GetMode, ICF-folded with a message getter; 0x45539; 0x45541)
    unsigned char GetMode() { return this->mMode; }
    void SetSlowPlayback() { this->mPlaybackThreadSlowModeToggleEvent->Set(); }
    void SetNormalPlayback() { this->mPlaybackThreadNormalModeToggleEvent->Set(); }
private:
    static unsigned long PlaybackThreadFunction(void* inArgument);
    void SwitchModes(unsigned char inMode);

    void (*mCallbackFunction)(NEMessage*, void*);   // +0x00
    TSThread* mPlaybackThread;              // +0x04
    TSEvent* mPlaybackThreadQuitEvent;      // +0x08
    TSEvent* mPlaybackThreadExiting;        // +0x0c
    TSEvent* mPlaybackThreadSlowModeToggleEvent;     // +0x10
    TSEvent* mPlaybackThreadNormalModeToggleEvent;   // +0x14
    FSFileSpec* mRecordFile;                // +0x18
    FSFileSpec* mPlaybackFile;              // +0x1c
    void* mCallbackData;                    // +0x20
    unsigned long mStartTime;               // +0x24 record start (TMGetTime), the file's first dword
    unsigned char mMode;                    // +0x28 NERecordPlaybackMode
    unsigned char mRecordMode;              // +0x29 1 = file, 2 = network (not implemented)
    unsigned char mPlaybackMode;            // +0x2a 1 = file, 2 = network (not implemented)
};

// NERecordPlayback::mRecordMode / mPlaybackMode: which RecordMode/PlaybackMode overload set it
enum NERecordPlaybackTarget {
    kRecordTargetFile = 1,   // set by RecordMode(FSFileSpec*) / PlaybackMode(FSFileSpec*) (provisional name)
    kRecordTargetNetwork = 2,  // set by RecordMode/PlaybackMode(UString& address, port): Functionality not implemented yet (provisional name)
};

#endif
