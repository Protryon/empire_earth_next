// The db table loaders' common read (port): a counted table, the u32 count and then that many records of one size,
// each handed to the table's copy (the record layouts and copies are Rust's: rust/crates/ee_data tables.rs). A record
// may follow lead strings (dbterrain's texture file, dbsounds' sample, dbgraphics' 32 names): the lead callback reads
// them before each record. Requires lle/FSFileSpec.h.
#ifndef EE_EEDBRECORDS_H
#define EE_EEDBRECORDS_H

#include <vector>

#include "lle/UString.h"
#include "rust/data/tables.h"

// Opens the file, reads the count, calls inBegin(count) (false: no records are read), then for every record
// inLead(index) (its lead strings) and inEach(index, bytes). A failed read still hands the buffer on, as the loaders
// did; with inStopOnError the loop ends after that record. Returns the last read status.
template <class F, class B, class L, class E>
long EEDbReadLeadRecords(F* inFile, size_t inSize, B inBegin, L inLead, E inEach, bool inStopOnError = false)
{
    std::vector<unsigned char> theRecord(inSize, 0);
    unsigned long theCount = 0;
    unsigned long theRead = 0;
    inFile->Open(kFileAccessRead);
    long theResult = inFile->Read(theCount);
    if (theResult == kErrorOK && inBegin(theCount)) {
        for (unsigned long i = 0; i < theCount; i++) {
            inLead(i);
            theResult = inFile->Read(theRecord.data(), (unsigned long)inSize, theRead);
            inEach(i, theRecord.data());
            if (inStopOnError && theResult != kErrorOK)
                break;
        }
    }
    inFile->Close();
    return theResult;
}

// EEDbReadLeadRecords for records without lead strings.
template <class F, class B, class E>
long EEDbReadRecords(F* inFile, size_t inSize, B inBegin, E inEach, bool inStopOnError = false)
{
    return EEDbReadLeadRecords(inFile, inSize, inBegin, [](unsigned long) {}, inEach, inStopOnError);
}

// Loads a table held by ee_data (ee/EEDbList.h) from its file; returns the last read status
template <class F> long EEDbLoadList(F* inFile, EeDataList inList)
{
    return EEDbReadRecords(inFile, ee_data_list_record_size(inList),
        [inList](unsigned long inCount) { ee_data_list_begin(inList, inCount); return true; },
        [inList](unsigned long inIndex, const unsigned char* inRecord) { ee_data_list_load(inList, inIndex, inRecord); });
}

// A record's name field (ee_data's EeDataName: up to its first NUL) copied into a UString.
inline void EEDbCopyName(UString& outName, const EeDataName& inName)
{
    outName.Copy(inName.text, 0, (unsigned long)inName.length, 0, true);
}

#endif
