// C:\EEX\Low-Level Engine\Network Engine\NELagLossSimulator.h (RECONSTRUCTION; NELagLossSimulator.cpp
// 0x3bc2d..0x3e05c). Simulated lag and packet loss between the synchronous communications and the protocol:
// lagged messages wait in a list sorted by their release time and are delivered by a lag thread.
// Requires URandom, TSScheduler/TSThread/TSEvent/TSCriticalSection, NEMessage, NETimestampedMessage.
#ifndef LLE_NELAGLOSSSIMULATOR_H
#define LLE_NELAGLOSSSIMULATOR_H

#include <vector>

class LLE_API NELagLossSimulator {         // proven sizeof 0x58 (copy ctor 0x3d667)
public:
    NELagLossSimulator(void (*inReceiveCallbackFunction)(NEMessage*, void*),
                       long (*inSendCallbackFunction)(NEMessage*, void*), void* inCallbackData);
    ~NELagLossSimulator();
    // proven inline (exported COMDATs 0x3d5ff..0x3d667, declaration order)
    void DisableReceiveLoss() { this->mReceiveLossEnabled = false; }
    void DisableSendLoss() { this->mSendLossEnabled = false; }
    void EnableReceiveLoss(float inPercentage)
    {
        this->mReceiveLossPercentage = inPercentage;
        this->mReceiveLossEnabled = true;
    }
    void EnableSendLoss(float inPercentage)
    {
        this->mSendLossPercentage = inPercentage;
        this->mSendLossEnabled = true;
    }
    void GetLagRange(unsigned long& outMinimum, unsigned long& outMaximum)
    {
        outMinimum = this->mLagMinimum;
        outMaximum = this->mLagMaximum;
    }
    float GetReceiveLossPercentage() { return this->mReceiveLossPercentage; }
    float GetSendLossPercentage() { return this->mSendLossPercentage; }
    void SetLagRange(unsigned long inMinimum, unsigned long inMaximum)
    {
        this->mLagMinimum = inMinimum;
        this->mLagMaximum = inMaximum;
    }
    void SetReceiveLossPercentage(float inPercentage) { this->mReceiveLossPercentage = inPercentage; }
    void SetSendLossPercentage(float inPercentage) { this->mSendLossPercentage = inPercentage; }
    void EnableLag(unsigned long inMinimum, unsigned long inMaximum);
    void DisableLag(bool inFlush);
    long Process(NEMessage* inMessage);
    long Send(NEMessage* inMessage);

protected:
    static unsigned long LagThreadFunction(void* inSimulator);

    std::vector<NETimestampedMessage*> mLaggedMessageList;   // +0x00 sorted by release time
    URandom mRandom;                                         // +0x10 reseeded with TMGetTime()
    void (*mReceiveCallbackFunction)(NEMessage*, void*);     // +0x20 delivers a (lagged) message
    long (*mSendCallbackFunction)(NEMessage*, void*);        // +0x24
    TSThread* mLagThread;                                    // +0x28
    TSEvent* mLagQueueChanged;                               // +0x2c (assert texts name the four events)
    TSEvent* mLagThreadExit;                                 // +0x30
    TSEvent* mLagThreadStarted;                              // +0x34
    TSEvent* mLagThreadFinished;                             // +0x38
    TSCriticalSection* mLaggedMessageListLock;               // +0x3c (assert text)
    void* mCallbackData;                                     // +0x40
    float mReceiveLossPercentage;                            // +0x44
    float mSendLossPercentage;                               // +0x48
    unsigned long mLagMinimum;                               // +0x4c
    unsigned long mLagMaximum;                               // +0x50
    bool mLagEnabled;                                        // +0x54
    bool mReceiveLossEnabled;                                // +0x55
    bool mSendLossEnabled;                                   // +0x56
    bool mFlushOnDisable;                                    // +0x57 DisableLag's argument
};

#endif
