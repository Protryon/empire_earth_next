// ne_queues_test stand-in: events and threads of the world
#pragma once
#include "q_world.h"
#include "TSCriticalSection.h"
#include "TSEvent.h"
#include "TSThread.h"
struct TSScheduler {
    static long CreateCriticalSection(TSCriticalSection*& outSection) { outSection = new TSCriticalSection; return 0; }
    static void DestroyCriticalSection(TSCriticalSection* inSection) { delete inSection; }
    static long CreateEvent(TSEvent*& outEvent) { outEvent = new TSEvent(gQ->NewEvent()); return 0; }
    static void DestroyEvent(TSEvent* inEvent)
    {
        if (inEvent == NULL)
            return;
        gQ->FreeEvent(inEvent->mID);
        delete inEvent;
    }
    static long CreateThread(TSThread*& outThread, unsigned long (*inFunction)(void*), void* inParameter,
                             unsigned long inStack, bool inSuspended)
    {
        outThread = new TSThread(gQ->NewThread([inFunction, inParameter] { return inFunction(inParameter); }));
        return 0;
    }
    static long DestroyThread(TSThread* inThread)
    {
        if (inThread == NULL)
            return 0;
        gQ->FreeThread(inThread->mID);
        delete inThread;
        return 0;
    }
};
