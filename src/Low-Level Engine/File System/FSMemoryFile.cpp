// C:\EEX\Low-Level Engine\File System\FSMemoryFile.cpp
// Unit flags (docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
// The class forwards to Rust (rust/crates/ee_fs/src/memfile.rs): this file keeps the C++ surface (the FSFile
// overloads, the stdio temporary file, the FSFile&-to-table glue).
#include <windows.h>
#include "platform/PlatFile.h"
#include <stdio.h>
#include <string.h>

#include "lle/LowLevelEngineCore.h"
#include "lle/UWideString.h"
#include "lle/UString.h"
#include "lle/FSMemoryFile.h"
#include "rust/ee_rust.h"
#include "FSRustOps.h"

static int32_t FileReadU32(void* inContext, uint32_t* ioValue)
{
    return (int32_t)static_cast<FSFile*>(inContext)->Read(*ioValue);
}

static int32_t FileReadBytes(void* inContext, uint8_t* outData, size_t inLength, uint32_t* outRead)
{
    unsigned long theRead = 0;
    int32_t theResult = (int32_t)static_cast<FSFile*>(inContext)->Read(outData, (unsigned long)inLength, theRead);
    *outRead = (uint32_t)theRead;
    return theResult;
}

static int32_t FileWriteU32(void* inContext, uint32_t inValue)
{
    return (int32_t)static_cast<FSFile*>(inContext)->Write(inValue);
}

static int32_t FileWriteBytes(void* inContext, const uint8_t* inData, size_t inLength, uint32_t* outWritten)
{
    unsigned long theWritten = 0;
    int32_t theResult = (int32_t)static_cast<FSFile*>(inContext)->Write(inData, (unsigned long)inLength, theWritten);
    *outWritten = (uint32_t)theWritten;
    return theResult;
}

void FSBuildFileOps(FSFile& inFile, EeFsFileOps& outOps)
{
    outOps.context = &inFile;
    outOps.read_u32 = FileReadU32;
    outOps.read_bytes = FileReadBytes;
    outOps.write_u32 = FileWriteU32;
    outOps.write_bytes = FileWriteBytes;
}

// FUNCTION: LLE 0x10009ec5
// Creates an open, empty memory file with room for inBlockCount blocks, growing by blocks or by doubling
FSMemoryFile::FSMemoryFile(unsigned long inBlockSize, bool inGrowByDoubling, unsigned long inBlockCount)
    : mState(ee_fs_mem_new(inBlockSize, inGrowByDoubling, inBlockCount)), mOwnsState(true)
{
    this->mTemporaryFileName = "";
}

// FUNCTION: LLE 0x10009f30
// Creates an open memory file holding the caller's buffer, which is copied and freed (the caller gave it up)
FSMemoryFile::FSMemoryFile(unsigned char* inData, unsigned long inLength, unsigned long inBlockSize,
                           bool inGrowByDoubling)
    : mState(ee_fs_mem_new_over(inData, inLength, inBlockSize, inGrowByDoubling)), mOwnsState(true)
{
    this->mTemporaryFileName = "";
    delete[] inData;
}

// Wraps a state made by Rust (an archive member, a chunk's payload); inOwns: the destructor frees it
FSMemoryFile::FSMemoryFile(EeFsMemFile* inState, bool inOwns)
    : mState(inState), mOwnsState(inOwns)
{
    this->mTemporaryFileName = "";
}

// FUNCTION: LLE 0x10009f91
// Destroys the memory file, closing it and freeing its buffer
FSMemoryFile::~FSMemoryFile()
{
    if (this->mOwnsState) {
        this->Close();
        ee_fs_mem_free(this->mState);
    }
    this->mState = NULL;
}

// FUNCTION: LLE 0x10009fbe
// Marks the memory file closed and closes (deletes) its temporary disk copy
long FSMemoryFile::Close()
{
    long theResult = ee_fs_mem_close(this->mState);
    if (!this->mStream)
        return theResult;
    fclose(this->mStream);
    // Fixed: the stream pointer was kept, so a second Close (the destructor's) closed the same FILE again
    this->mStream = NULL;
    if (this->mTemporaryFileName.GetLength() > 0)
        PlatDeleteFile(this->mTemporaryFileName);
    return theResult;
}

// FUNCTION: LLE 0x10009ff6
// Shrinks the buffer to the written length and rewinds to the start; kErrorNotOpen when closed
long FSMemoryFile::Compact()
{
    return ee_fs_mem_compact(this->mState);
}

// FUNCTION: LLE 0x1000a05e
// Compresses the contents in place (only above 12 bytes) and closes the file; outLength is the new length
// (an incompressible result, error 0x16, keeps the data uncompressed and succeeds)
long FSMemoryFile::Compress(unsigned long inLevel, unsigned long& outLength)
{
    uint32_t theLength = 0;
    long theResult = ee_fs_mem_compress(this->mState, (uint32_t)inLevel, &theLength);
    outLength = theLength;
    if (theResult == 0x15)
        ESVERIFY_AT(NULL == "PKWARE implode() error!", 140);
    return theResult;
}

// FUNCTION: LLE 0x1000a176
// Writes the contents to a disk file; kErrorAlreadyExists if inFailIfExists and it exists
long FSMemoryFile::Copy(const UString& inDestination, bool inFailIfExists, bool inKeepAttributes)
{
    long theResult;
    FILE* theFile;
    if (inFailIfExists && (theFile = PlatFOpen(inDestination, "r")) != NULL)
        theResult = kErrorAlreadyExists;
    else if ((theFile = PlatFOpen(inDestination, "wb")) != NULL) {
        theResult = this->Compact();
        if (theResult == kErrorOK) {
            uint32_t theLength = ee_fs_mem_length(this->mState);
            if (theLength > 0)
                fwrite(this->GetData(), 1, theLength, theFile);
        }
    } else
        theResult = kErrorNativeError;
    ee_fs_mem_set_position(this->mState, 0, kFileOriginBegin);
    if (theFile)
        fclose(theFile);
    return theResult;
}

// FUNCTION: LLE 0x1000a20b
// Decompresses the contents in place when they are compressed and closes the file; outLength is the new length
long FSMemoryFile::Decompress(unsigned long& outLength)
{
    uint32_t theLength = 0;
    long theResult = ee_fs_mem_decompress(this->mState, &theLength);
    outLength = theLength;
    return theResult;
}

// FUNCTION: LLE 0x1000a315
// Frees the buffer and leaves the memory file empty and closed
long FSMemoryFile::Delete()
{
    return ee_fs_mem_delete(this->mState);
}

// FUNCTION: LLE 0x1000a343
// Copies the contents to a new temporary disk file and reopens it as a stdio stream with the given mode
long FSMemoryFile::FOpen(char* inMode)
{
    char theDirectory[PLAT_MAX_PATH];
    char theName[PLAT_MAX_PATH];
    char thePrefix[] = "EE_";
    bool theOpened = false;
    long theResult = kErrorOK;
    ee_fs_mem_set_open(this->mState, true);
    if (PlatGetTempDirectory(theDirectory, 256) && PlatCreateTempFile(theDirectory, thePrefix, theName)) {   // 256: _MAX_FNAME
        this->mTemporaryFileName = UWideString(theName);
        this->mStream = PlatFOpen(this->mTemporaryFileName, "w+b");
        if (this->mStream) {
            theResult = this->Compact();
            if (theResult == kErrorOK) {
                uint32_t theLength = ee_fs_mem_length(this->mState);
                if (theLength > 0)
                    fwrite(this->GetData(), 1, theLength, this->mStream);
                // the C++ left the position at the end after writing the contents out
                ee_fs_mem_set_position(this->mState, theLength, kFileOriginBegin);
            }
            fclose(this->mStream);
            this->mStream = PlatFOpen(this->mTemporaryFileName, inMode);
            if (this->mStream)
                theOpened = true;
            else
                ee_fs_mem_set_open(this->mState, false);
        } else {
            this->mTemporaryFileName = "";
            ee_fs_mem_set_open(this->mState, false);
        }
    }
    if (!theOpened) {
        theResult = kErrorNativeError;
        ESRecordNativeError(PlatGetLastError());
    }
    return theResult;
}

// FUNCTION: LLE 0x1000a45b
// Returns the written length
long FSMemoryFile::GetLength(unsigned long& outLength) const
{
    outLength = ee_fs_mem_length(this->mState);
    return kErrorOK;
}

// FUNCTION: LLE 0x1000a46c
// Returns the current position, or -1 when closed or empty
long FSMemoryFile::GetPosition(long& outPosition) const
{
    outPosition = ee_fs_mem_position(this->mState);
    return kErrorOK;
}

const unsigned char* FSMemoryFile::GetData() const
{
    return ee_fs_mem_data(this->mState);
}

long FSMemoryFile::Create()
{
    return this->GetData() ? kErrorOK : kErrorFailed;
}

// FUNCTION: LLE 0x1000a4e9
// Replaces the contents with inLength bytes read from another file; on failure leaves it empty and closed
long FSMemoryFile::Load(FSFile& inFile, unsigned long inLength)
{
    EeFsFileOps theOps;
    FSBuildFileOps(inFile, theOps);
    return ee_fs_mem_load(this->mState, &theOps, inLength);
}

// FUNCTION: LLE 0x1000a55c
// Replaces the contents with a length-prefixed block read from another file
long FSMemoryFile::Load(FSFile& inFile)
{
    EeFsFileOps theOps;
    FSBuildFileOps(inFile, theOps);
    return ee_fs_mem_load_prefixed(this->mState, &theOps);
}

// FUNCTION: LLE 0x1000a58b
// Reopens the memory file at its start, allocating one block if it has no buffer
long FSMemoryFile::Open(unsigned long inAccess)
{
    return ee_fs_mem_open(this->mState);
}

// FUNCTION: LLE 0x1000a5de
// Copies inLength bytes from the current position; kErrorEndOfFile (nothing read) if fewer are left
long FSMemoryFile::Read(unsigned char* outData, unsigned long inLength, unsigned long& outRead)
{
    uint32_t theRead = (uint32_t)outRead;
    long theResult = ee_fs_mem_read(this->mState, outData, inLength, &theRead);
    outRead = theRead;
    return theResult;
}

// FUNCTION: LLE 0x1000a624
// Reads raw characters from the current position
long FSMemoryFile::Read(char* outData, unsigned long inLength, unsigned long& outRead)
{
    return this->Read(reinterpret_cast<unsigned char*>(outData), inLength, outRead);
}

// FUNCTION: LLE 0x1000a638
// Reads a float from the current position
long FSMemoryFile::Read(float& outValue)
{
    unsigned long theRead;
    return this->Read(reinterpret_cast<unsigned char*>(&outValue), sizeof(outValue), theRead);
}

long FSMemoryFile::Read(int32_t& outValue)
{
    unsigned long theRead;
    return this->Read(reinterpret_cast<unsigned char*>(&outValue), sizeof(outValue), theRead);
}

long FSMemoryFile::Read(uint32_t& outValue)
{
    unsigned long theRead;
    return this->Read(reinterpret_cast<unsigned char*>(&outValue), sizeof(outValue), theRead);
}

// FUNCTION: LLE 0x1000a64d
// Reads a short from the current position
long FSMemoryFile::Read(short& outValue)
{
    unsigned long theRead;
    return this->Read(reinterpret_cast<unsigned char*>(&outValue), sizeof(outValue), theRead);
}

long FSMemoryFile::Read(unsigned short& outValue)
{
    unsigned long theRead;
    return this->Read(reinterpret_cast<unsigned char*>(&outValue), sizeof(outValue), theRead);
}

// FUNCTION: LLE 0x1000a662
// Reads a signed byte from the current position
long FSMemoryFile::Read(signed char& outValue)
{
    unsigned long theRead;
    return this->Read(reinterpret_cast<unsigned char*>(&outValue), sizeof(outValue), theRead);
}

long FSMemoryFile::Read(unsigned char& outValue)
{
    unsigned long theRead;
    return this->Read(static_cast<unsigned char*>(&outValue), sizeof(outValue), theRead);
}

long FSMemoryFile::Read(bool& outValue)
{
    unsigned long theRead;
    return this->Read(reinterpret_cast<unsigned char*>(&outValue), sizeof(outValue), theRead);
}

// FUNCTION: LLE 0x1000a677
// Reads a double from the current position
long FSMemoryFile::Read(double& outValue)
{
    unsigned long theRead;
    return this->Read(reinterpret_cast<unsigned char*>(&outValue), sizeof(outValue), theRead);
}

// FUNCTION: LLE 0x1000a68c
// Writes the first inLength bytes of the contents to another file
long FSMemoryFile::Save(FSFile& outFile, unsigned long inLength)
{
    EeFsFileOps theOps;
    FSBuildFileOps(outFile, theOps);
    return ee_fs_mem_save(this->mState, &theOps, inLength);
}

// FUNCTION: LLE 0x1000a6ab
// Writes the length, then the compacted contents to another file; returns only the second write's result
long FSMemoryFile::Save(FSFile& outFile)
{
    EeFsFileOps theOps;
    FSBuildFileOps(outFile, theOps);
    return ee_fs_mem_save_prefixed(this->mState, &theOps);
}

// FUNCTION: LLE 0x1000a6ec
// Moves the position within the written data (from the end it counts backwards); never past the end
long FSMemoryFile::SetPosition(long inOffset, unsigned long inOrigin, bool inExtend)
{
    return ee_fs_mem_set_position(this->mState, inOffset, inOrigin);
}

// FUNCTION: LLE 0x1000a754
// Writes bytes at the current position, growing the buffer by a block or by doubling; kErrorNotOpen when closed
long FSMemoryFile::Write(const unsigned char* inData, unsigned long inLength, unsigned long& outWritten)
{
    uint32_t theWritten = 0;
    long theResult = ee_fs_mem_write(this->mState, inData, inLength, &theWritten);
    outWritten = theWritten;
    return theResult;
}

// FUNCTION: LLE 0x1000a7cc
// Writes raw characters at the current position
long FSMemoryFile::Write(const char* inData, unsigned long inLength, unsigned long& outWritten)
{
    return this->Write(reinterpret_cast<const unsigned char*>(inData), inLength, outWritten);
}

// FUNCTION: LLE 0x1000a7e3
// Writes a float at the current position
long FSMemoryFile::Write(const float& inValue)
{
    unsigned long theWritten;
    return this->Write(reinterpret_cast<const unsigned char*>(&inValue), sizeof(inValue), theWritten);
}

long FSMemoryFile::Write(const int32_t& inValue)
{
    unsigned long theWritten;
    return this->Write(reinterpret_cast<const unsigned char*>(&inValue), sizeof(inValue), theWritten);
}

long FSMemoryFile::Write(const uint32_t& inValue)
{
    unsigned long theWritten;
    return this->Write(reinterpret_cast<const unsigned char*>(&inValue), sizeof(inValue), theWritten);
}

// FUNCTION: LLE 0x1000a7fb
// Writes a short at the current position
long FSMemoryFile::Write(const short& inValue)
{
    unsigned long theWritten;
    return this->Write(reinterpret_cast<const unsigned char*>(&inValue), sizeof(inValue), theWritten);
}

long FSMemoryFile::Write(const unsigned short& inValue)
{
    unsigned long theWritten;
    return this->Write(reinterpret_cast<const unsigned char*>(&inValue), sizeof(inValue), theWritten);
}

// FUNCTION: LLE 0x1000a813
// Writes a signed byte at the current position
long FSMemoryFile::Write(const signed char& inValue)
{
    unsigned long theWritten;
    return this->Write(reinterpret_cast<const unsigned char*>(&inValue), sizeof(inValue), theWritten);
}

long FSMemoryFile::Write(const unsigned char& inValue)
{
    unsigned long theWritten;
    return this->Write(static_cast<const unsigned char*>(&inValue), sizeof(inValue), theWritten);
}

long FSMemoryFile::Write(const bool& inValue)
{
    unsigned long theWritten;
    return this->Write(reinterpret_cast<const unsigned char*>(&inValue), sizeof(inValue), theWritten);
}

// FUNCTION: LLE 0x1000a82b
// Writes a double at the current position
long FSMemoryFile::Write(const double& inValue)
{
    unsigned long theWritten;
    return this->Write(reinterpret_cast<const unsigned char*>(&inValue), sizeof(inValue), theWritten);
}
