// C:\EEX\Low-Level Engine\Utilities\UReferenceCounter.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// 0x1008104b..0x10081d93: the sReferenceDebugInfo init chain (guarded atexit dtor), then the reference-tracking
// members (DumpAllLeakedReferences, DereferenceWithoutDecrementingCount, Dereference, Reference, GetBucketMap), then
// the map COMDATs. The inline members are emitted by File System.cpp (0x1000436c..). URegistry.cpp starts at
// 0x10081d93.
#define LLE_UREFERENCECOUNTER_CPP   // sReferenceDebugInfo is visible here only (see lle/LowLevelEngine.h) (provisional name)
#include <windows.h>
#include <stdio.h>
#include <map>

#include "lle/LowLevelEngine.h"

UReferenceCounter::LineMap UReferenceCounter::sReferenceDebugInfo;

// FUNCTION: LLE 0x1008109c
// Writes every tracked reference (by line, object and file, with counts) to the file as a leak report
void UReferenceCounter::DumpAllLeakedReferences(FSFileSpec& inFile)
{
    LineMap::iterator theLine;
    CounterMap::iterator theCounter;
    FileCountMap::iterator theFile;
    fprintf(inFile, "REFERENCE COUNTED MEMORY LEAKS\n\n");
    for (theLine = UReferenceCounter::sReferenceDebugInfo.begin(); theLine != UReferenceCounter::sReferenceDebugInfo.end(); theLine++) {
        fprintf(inFile, "The following Unique ID %d Has Memory Leaks:\n", theLine->first);
        for (theCounter = theLine->second->begin(); theCounter != theLine->second->end(); theCounter++) {
            fprintf(inFile, "\tThe following Pointer Address %p Has Memory Leaks:\n", (void*)theCounter->first);
            for (theFile = theCounter->second->begin(); theFile != theCounter->second->end(); theFile++)
                fprintf(inFile,
                        "\t\tThe following %s with a count of %d (Real reference count of %d)is a memory leak.\n",
                        theFile->first, theFile->second, theCounter->first->mReferenceCount);
        }
    }
}

// FUNCTION: LLE 0x10081189
// Takes one tracked reference of this object off the debug record for the line and file, keeping the real count
void UReferenceCounter::DereferenceWithoutDecrementingCount(unsigned long inLine, const char* inFile)
{
    LineMap::iterator theLine = UReferenceCounter::sReferenceDebugInfo.find(inLine);
    if (theLine == UReferenceCounter::sReferenceDebugInfo.end())
        return;
    CounterMap::iterator theCounter = theLine->second->find(this);
    if (theCounter == theLine->second->end())
        return;
    FileCountMap::iterator theFile = theCounter->second->find(inFile);
    if (theFile == theCounter->second->end())
        return;
    (*theCounter->second)[inFile] = theFile->second - 1;
    if (theFile->second != 0)
        return;
    theCounter->second->erase(theFile);
    if (theCounter->second->size() != 0)
        return;
    delete theCounter->second;
    theLine->second->erase(theCounter);
    if (theLine->second->size() == 0) {
        delete theLine->second;
        UReferenceCounter::sReferenceDebugInfo.erase(theLine);
    }
}

// FUNCTION: LLE 0x1008127f
// Takes one tracked reference off the debug record for the line and file, then dereferences the object
void UReferenceCounter::Dereference(unsigned long inLine, const char* inFile)
{
    LineMap::iterator theLine = UReferenceCounter::sReferenceDebugInfo.find(inLine);
    if (theLine != UReferenceCounter::sReferenceDebugInfo.end()) {
        CounterMap::iterator theCounter = theLine->second->find(this);
        if (theCounter != theLine->second->end()) {
            FileCountMap::iterator theFile = theCounter->second->find(inFile);
            if (theFile != theCounter->second->end()) {
                (*theCounter->second)[inFile] = theFile->second - 1;
                if (theFile->second == 0) {
                    theCounter->second->erase(theFile);
                    if (theCounter->second->size() == 0) {
                        delete theCounter->second;
                        theLine->second->erase(theCounter);
                        if (theLine->second->size() == 0) {
                            delete theLine->second;
                            UReferenceCounter::sReferenceDebugInfo.erase(theLine);
                        }
                    }
                }
            }
        }
    }
    this->Dereference();
}

// FUNCTION: LLE 0x10081380
// Records one reference of this object for the line and file in the debug record, then increments its count
void UReferenceCounter::Reference(unsigned long inLine, const char* inFile)
{
    LineMap::iterator theLine = UReferenceCounter::sReferenceDebugInfo.find(inLine);
    if (theLine != UReferenceCounter::sReferenceDebugInfo.end()) {
        CounterMap::iterator theCounter = theLine->second->find(this);
        if (theCounter != theLine->second->end()) {
            FileCountMap::iterator theFile = theCounter->second->find(inFile);
            if (theFile != theCounter->second->end())
                (*theCounter->second)[inFile] = theFile->second + 1;
            else
                (*theCounter->second)[inFile] = 1;
        } else {
            FileCountMap* theFiles = new FileCountMap;
            (*theLine->second)[this] = theFiles;
            (*theFiles)[inFile] = 1;
        }
    } else {
        CounterMap* theCounters = new CounterMap;
        FileCountMap* theFiles = new FileCountMap;
        UReferenceCounter::sReferenceDebugInfo[inLine] = theCounters;
        (*theCounters)[this] = theFiles;
        (*theFiles)[inFile] = 1;
    }
    this->mReferenceCount++;
}

// FUNCTION: LLE 0x100814f0
// Looks up the debug record for a line but always returns NULL
UReferenceCounter::CounterMap* UReferenceCounter::GetBucketMap(unsigned long inLine)
{
    LineMap::iterator theLine = UReferenceCounter::sReferenceDebugInfo.find(inLine);
    return NULL;
}

// Compiler-generated for this unit (names in analysis/lle/names.txt):
// TEMPLATE: LLE 0x1008150a
// TEMPLATE: LLE 0x10081532
// TEMPLATE: LLE 0x1008155d
// TEMPLATE: LLE 0x10081585
// TEMPLATE: LLE 0x100815b0
// TEMPLATE: LLE 0x100815d8
// TEMPLATE: LLE 0x10081603
// TEMPLATE: LLE 0x100818b1
// TEMPLATE: LLE 0x100818eb
// TEMPLATE: LLE 0x1008195c
// TEMPLATE: LLE 0x100819cd
// TEMPLATE: LLE 0x10081a83
// TEMPLATE: LLE 0x10081af4
// TEMPLATE: LLE 0x10081b26
// TEMPLATE: LLE 0x10081b58
// TEMPLATE: LLE 0x10081b8d
// TEMPLATE: LLE 0x10081bbf
// TEMPLATE: LLE 0x10081d53
// TEMPLATE: LLE 0x10081d7c
// SYNTHETIC: LLE 0x1008104b
// SYNTHETIC: LLE 0x10081055
// SYNTHETIC: LLE 0x10081075
// SYNTHETIC: LLE 0x10081081
