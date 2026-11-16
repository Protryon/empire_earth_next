// The db table loaders' common read (port): a counted table, the u32 count and then that many records of one size,
// each handed to the table's copy (the record layouts and copies are Rust's: rust/crates/ee_data tables.rs).
// Requires lle/FSFileSpec.h.
#ifndef EE_EEDBRECORDS_H
#define EE_EEDBRECORDS_H

#include <vector>

// Opens the file, reads the count, calls inBegin(count) (false: no records are read), then inEach(index, bytes) for
// every record. A failed read still hands the buffer on, as the loaders did; returns the last read status.
template <class B, class E> long EEDbReadRecords(FSFileSpec* inFile, size_t inSize, B inBegin, E inEach)
{
    std::vector<unsigned char> theRecord(inSize, 0);
    unsigned long theCount = 0;
    unsigned long theRead = 0;
    inFile->Open(kFileAccessRead);
    long theResult = inFile->Read(theCount);
    if (theResult == kErrorOK && inBegin(theCount)) {
        for (unsigned long i = 0; i < theCount; i++) {
            theResult = inFile->Read(theRecord.data(), (unsigned long)inSize, theRead);
            inEach(i, theRecord.data());
        }
    }
    inFile->Close();
    return theResult;
}

#endif
