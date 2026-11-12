// New (port), TEST ONLY: the simulation capture (docs/RUST_PLAN.md R5b, the leaves' golden gates). With
// EE_SIM_CAPTURE=<absolute file> a game appends one binary record per call of a captured leaf (formations, ...): the
// call's plain-data inputs and its outputs, so that a tool can replay the inputs through another implementation and
// compare the outputs (tools/sim_capture.py). Off (the default), Active() is one cached test.
//
// A record: u32 tag, u32 payload size, the payload (little-endian u32/i32/f32 fields, the tag's layout). Records are
// written whole under a lock.
#ifndef EE_EESIMCAPTURE_H
#define EE_EESIMCAPTURE_H

#include <vector>

class EESimCaptureRecord {
public:
    static bool Active();                   // EE_SIM_CAPTURE is set and the file opened
    static void Flush();                    // before the process ends (EE-SIM's Quit)
    // While set (not 0), this thread's records of any other tag are dropped: a sweep that calls a leaf records only
    // that leaf, not the leaves it reaches (Only(0) clears it)
    static void Only(unsigned int inTag);

    explicit EESimCaptureRecord(unsigned int inTag) : mTag(inTag) {}
    void U32(unsigned int inValue) { this->Bytes(&inValue, 4); }
    void I32(int inValue) { this->Bytes(&inValue, 4); }
    void F32(float inValue) { this->Bytes(&inValue, 4); }
    void F32s(const float* inValues, unsigned int inCount) { this->Bytes(inValues, inCount * 4); }
    void Bytes(const void* inData, unsigned int inSize)
    {
        const unsigned char* theBytes = (const unsigned char*)inData;
        this->mData.insert(this->mData.end(), theBytes, theBytes + inSize);
    }
    void Commit();                          // appends the record to the capture file

private:
    unsigned int mTag;
    std::vector<unsigned char> mData;
};

#endif
