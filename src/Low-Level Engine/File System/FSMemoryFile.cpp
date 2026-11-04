// C:\EEX\Low-Level Engine\File System\FSMemoryFile.cpp
// Unit flags (docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
#include <windows.h>
#include <stdio.h>
#include <string.h>

#include "lle/LowLevelEngineCore.h"
#include "lle/UWideString.h"
#include "lle/UString.h"
#include "lle/FSMemoryFile.h"
#include "lle/UDataCompression.h"

// FUNCTION: LLE 0x10009ec5
// Creates an open, empty memory file with room for inBlockCount blocks, growing by blocks or by doubling
FSMemoryFile::FSMemoryFile(unsigned long inBlockSize, bool inGrowByDoubling, unsigned long inBlockCount)
{
    this->mTemporaryFileName = "";
    if (inBlockSize == 0)
        inBlockSize = 1;
    this->mBlockSize = inBlockSize;
    this->mGrowByDoubling = inGrowByDoubling;
    this->mLimit = NULL;
    this->mStart = NULL;
    this->mAllocatedLength = 0;
    this->mBuffer = NULL;
    this->mEnd = NULL;
    this->mCurrent = NULL;
    this->IncreaseFileSize(inBlockSize * inBlockCount);
    if (this->mBuffer)
        this->mIsOpen = true;
}

// FUNCTION: LLE 0x10009f30
// Creates an open memory file over the caller's buffer (which it then owns and frees)
FSMemoryFile::FSMemoryFile(unsigned char* inData, unsigned long inLength, unsigned long inBlockSize,
                           bool inGrowByDoubling)
{
    this->mTemporaryFileName = "";
    if (inBlockSize == 0)
        inBlockSize = 1;
    this->mIsOpen = true;
    this->mBlockSize = inBlockSize;
    this->mGrowByDoubling = inGrowByDoubling;
    this->mAllocatedLength = inLength;
    this->mLimit = inData + inLength;
    this->mEnd = inData + inLength;
    this->mBuffer = inData;
    this->mStart = inData;
    this->mCurrent = inData;
}

// FUNCTION: LLE 0x10009f91
// Destroys the memory file, closing it and freeing its buffer
FSMemoryFile::~FSMemoryFile()
{
    this->Close();
    if (this->mBuffer)
        delete[] this->mBuffer;
}

// FUNCTION: LLE 0x10009fbe
// Marks the memory file closed and closes (deletes) its temporary disk copy
long FSMemoryFile::Close()
{
    long theResult = kErrorOK;
    if (this->mIsOpen)
        this->mIsOpen = false;
    else
        theResult = kErrorNotOpen;
    if (!this->mStream)
        return theResult;
    fclose(this->mStream);
    // Fixed: the stream pointer was kept, so a second Close (the destructor's) closed the same FILE again
    this->mStream = NULL;
    if (this->mTemporaryFileName.GetLength() > 0)
        DeleteFileA(this->mTemporaryFileName);
    return theResult;
}

// FUNCTION: LLE 0x10009ff6
// Shrinks the buffer to the written length and rewinds to the start; kErrorNotOpen when closed
long FSMemoryFile::Compact()
{
    long theResult = kErrorOK;
    if (this->mIsOpen) {
        if (this->mBuffer) {
            if (this->mLimit > this->mEnd) {
                unsigned long theLength = this->mEnd - this->mStart;
                unsigned char* theBuffer = new unsigned char[theLength];
                this->mLimit = theBuffer + theLength;
                this->mStart = theBuffer;
                this->mCurrent = theBuffer;
                this->mEnd = theBuffer + theLength;
                memcpy(theBuffer, this->mBuffer, theLength);
                delete[] this->mBuffer;
                this->mBuffer = theBuffer;
                this->mAllocatedLength = theLength;
            } else
                this->mCurrent = this->mStart;
        } else
            theResult = kErrorFailed;
    } else
        theResult = kErrorNotOpen;
    return theResult;
}

// FUNCTION: LLE 0x1000a05e
// Compresses the contents in place (only above 12 bytes) and closes the file; outLength is the new length
// (an incompressible result, error 0x16, keeps the data uncompressed and succeeds)
long FSMemoryFile::Compress(unsigned long inLevel, unsigned long& outLength)
{
    unsigned long theLength;
    unsigned long theCompressedLength;
    UDataCompression* theCompressor;
    unsigned char* theBuffer;
    outLength = 0;
    long theResult = this->Compact();
    if (theResult == kErrorOK) {
        theResult = this->GetLength(theLength);
        if (theResult == kErrorOK) {
            if (theLength > 12) {
                theCompressor = new UDataCompression;
                theBuffer = new unsigned char[theLength];
                theResult = theCompressor->Compress(reinterpret_cast<const char*>(this->GetData()), theLength, reinterpret_cast<char*>(theBuffer), theLength,
                                                    theCompressedLength, inLevel);
                if (theResult == kErrorOK) {
                    delete[] this->mBuffer;
                    this->mBuffer = new unsigned char[theCompressedLength];
                    this->mStart = this->mBuffer;
                    this->mCurrent = this->mBuffer;
                    this->mEnd = this->mBuffer + theCompressedLength;
                    this->mLimit = this->mBuffer + theCompressedLength;
                    this->mAllocatedLength = theCompressedLength;
                    memcpy(this->mBuffer, theBuffer, theCompressedLength);
                    this->Close();
                    outLength = theCompressedLength;
                    theResult = kErrorOK;
                } else if (theResult == 0x16) {
                    this->Close();
                    this->GetLength(outLength);
                    theResult = kErrorOK;
                }
                delete[] theBuffer;
                delete theCompressor;
            } else {
                this->Close();
                this->GetLength(outLength);
            }
        }
    }
    return theResult;
}

// FUNCTION: LLE 0x1000a176
// Writes the contents to a disk file byte by byte; kErrorAlreadyExists if inFailIfExists and it exists
long FSMemoryFile::Copy(const UString& inDestination, bool inFailIfExists, bool inKeepAttributes)
{
    long theResult;
    FILE* theFile;
    if (inFailIfExists && (theFile = fopen(inDestination, "r")) != NULL)
        theResult = kErrorAlreadyExists;
    else if ((theFile = fopen(inDestination, "wb")) != NULL) {
        theResult = this->Compact();
        if (theResult == kErrorOK) {
            this->mCurrent = this->mStart;
            for (unsigned long i = 0; i < this->mAllocatedLength; i++) {
                fwrite(this->mCurrent, 1, 1, theFile);
                this->mCurrent++;
            }
        }
    } else
        theResult = kErrorNativeError;
    this->mCurrent = this->mStart;
    if (theFile)
        fclose(theFile);
    return theResult;
}

// FUNCTION: LLE 0x1000a20b
// Decompresses the contents in place when they are compressed and closes the file; outLength is the new length
long FSMemoryFile::Decompress(unsigned long& outLength)
{
    unsigned long theDecompressedLength;
    unsigned long theLength;
    unsigned long theActualLength;
    outLength = 0;
    UDataCompression* theCompressor = new UDataCompression;
    long theResult = this->GetLength(theLength);
    if (theResult == kErrorOK) {
        if (theCompressor->GetIsCompressed(reinterpret_cast<const char*>(this->GetData()), theLength)) {
            theResult = theCompressor->GetDecompressedSize(reinterpret_cast<const char*>(this->GetData()), theLength, theDecompressedLength);
            if (theResult == kErrorOK) {
                unsigned char* theBuffer = new unsigned char[theDecompressedLength];
                theResult = theCompressor->Decompress(reinterpret_cast<const char*>(this->GetData()), theLength, reinterpret_cast<char*>(theBuffer),
                                                      theDecompressedLength, theActualLength);
                if (theResult == kErrorOK) {
                    theResult = theDecompressedLength != theActualLength ? kErrorFailed : kErrorOK;
                    if (theResult == kErrorOK) {
                        delete[] this->mBuffer;
                        this->mEnd = theBuffer + theDecompressedLength;
                        this->mLimit = theBuffer + theDecompressedLength;
                        this->mAllocatedLength = theDecompressedLength;
                        this->mBuffer = theBuffer;
                        this->mStart = theBuffer;
                        this->mCurrent = theBuffer;
                        this->Close();
                        outLength = theDecompressedLength;
                        theResult = kErrorOK;
                    } else
                        delete[] theBuffer;
                } else
                    delete[] theBuffer;   // Fixed: the buffer leaked when decompression failed
            }
        } else
            this->GetLength(outLength);
    }
    delete theCompressor;
    return theResult;
}

// FUNCTION: LLE 0x1000a315
// Frees the buffer and leaves the memory file empty and closed
long FSMemoryFile::Delete()
{
    if (this->mBuffer)
        delete[] this->mBuffer;
    this->mBuffer = NULL;
    this->mStart = NULL;
    this->mCurrent = NULL;
    this->mLimit = NULL;
    this->mEnd = NULL;
    this->mAllocatedLength = 0;
    this->mIsOpen = false;
    return kErrorOK;
}

// FUNCTION: LLE 0x1000a343
// Copies the contents to a new temporary disk file and reopens it as a stdio stream with the given mode
long FSMemoryFile::FOpen(char* inMode)
{
    char theDirectory[MAX_PATH];
    char theName[MAX_PATH];
    char thePrefix[] = "EE_";
    bool theOpened = false;
    long theResult;
    this->mIsOpen = true;
    if (GetTempPathA(_MAX_FNAME, theDirectory) && GetTempFileNameA(theDirectory, thePrefix, 0, theName)) {
        this->mTemporaryFileName = UWideString(theName);
        this->mStream = fopen(this->mTemporaryFileName, "w+b");
        if (this->mStream) {
            theResult = this->Compact();
            if (theResult == kErrorOK) {
                this->mCurrent = this->mStart;
                for (unsigned long i = 0; i < this->mAllocatedLength; i++) {
                    fwrite(this->mCurrent, 1, 1, this->mStream);
                    this->mCurrent++;
                }
            }
            fclose(this->mStream);
            this->mStream = fopen(this->mTemporaryFileName, inMode);
            if (this->mStream)
                theOpened = true;
            else
                this->mIsOpen = false;
        } else {
            this->mTemporaryFileName = "";
            this->mIsOpen = false;
        }
    }
    if (!theOpened) {
        theResult = kErrorNativeError;
        ESRecordNativeError(GetLastError());
    }
    return theResult;
}

// FUNCTION: LLE 0x1000a45b
// Returns the written length
long FSMemoryFile::GetLength(unsigned long& outLength) const
{
    outLength = this->mEnd - this->mStart;
    return kErrorOK;
}

// FUNCTION: LLE 0x1000a46c
// Returns the current position, or -1 when closed or empty
long FSMemoryFile::GetPosition(long& outPosition) const
{
    if (this->mIsOpen && this->mBuffer)
        outPosition = this->mCurrent - this->mStart;
    else
        outPosition = -1;
    return kErrorOK;
}

// FUNCTION: LLE 0x1000a492
// Grows the buffer by inLength bytes, keeping the contents and position
void FSMemoryFile::IncreaseFileSize(unsigned long inLength)
{
    unsigned long theLength = this->mAllocatedLength + inLength;
    unsigned char* theBuffer = new unsigned char[theLength];
    this->mCurrent += theBuffer - this->mStart;
    this->mEnd += theBuffer - this->mStart;
    this->mLimit = theBuffer + theLength;
    this->mStart = theBuffer;
    if (this->mBuffer) {
        memcpy(theBuffer, this->mBuffer, this->mAllocatedLength);
        delete[] this->mBuffer;
    }
    this->mAllocatedLength = theLength;
    this->mBuffer = theBuffer;
}

// FUNCTION: LLE 0x1000a4e9
// Replaces the contents with inLength bytes read from another file; on failure leaves it empty and closed
long FSMemoryFile::Load(FSFile& inFile, unsigned long inLength)
{
    unsigned long theRead = 0;
    long theResult;
    if (this->mBuffer)
        this->Delete();
    this->mBuffer = new unsigned char[inLength];
    if (this->mBuffer) {
        theResult = inFile.Read(this->mBuffer, inLength, theRead);
        if (theResult == kErrorOK) {
            this->mStart = this->mBuffer;
            this->mCurrent = this->mBuffer;
            this->mIsOpen = true;
            this->mAllocatedLength = inLength;
            this->mEnd = this->mBuffer + inLength;
            this->mLimit = this->mBuffer + inLength;
        } else
            this->Delete();
    } else {
        this->Delete();
        theResult = kErrorFailed;
    }
    return theResult;
}

// FUNCTION: LLE 0x1000a55c
// Replaces the contents with a length-prefixed block read from another file
long FSMemoryFile::Load(FSFile& inFile)
{
    unsigned long theLength = 0;
    long theResult = inFile.Read(theLength);
    if (theResult == kErrorOK)
        theResult = this->Load(inFile, theLength);
    return theResult;
}

// FUNCTION: LLE 0x1000a58b
// Reopens the memory file at its start, allocating one block if it has no buffer
long FSMemoryFile::Open(unsigned long inAccess)
{
    long theResult = kErrorOK;
    if (!this->mIsOpen) {
        if (this->mBuffer) {
            this->mIsOpen = true;
            this->SetPosition(0, kFileOriginBegin, false);
        } else {
            this->mAllocatedLength = 0;
            this->mCurrent = NULL;
            this->mStart = NULL;
            this->mEnd = NULL;
            this->mLimit = NULL;
            this->IncreaseFileSize(this->mBlockSize);
            if (this->mBuffer)
                this->mIsOpen = true;
            else
                theResult = kErrorFailed;
        }
    }
    return theResult;
}

// FUNCTION: LLE 0x1000a5de
// Copies inLength bytes from the current position; kErrorEndOfFile (nothing read) if fewer are left
long FSMemoryFile::Read(unsigned char* outData, unsigned long inLength, unsigned long& outRead)
{
    long theResult = kErrorNotOpen;
    if (this->mBuffer) {
        if (this->mCurrent + inLength <= this->mEnd) {
            memcpy(outData, this->mCurrent, inLength);
            this->mCurrent += inLength;
            outRead = inLength;
            theResult = kErrorOK;
        } else {
            outRead = 0;
            theResult = kErrorEndOfFile;
        }
    }
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

long FSMemoryFile::Read(long& outValue)
{
    unsigned long theRead;
    return this->Read(reinterpret_cast<unsigned char*>(&outValue), sizeof(outValue), theRead);
}

long FSMemoryFile::Read(unsigned long& outValue)
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
    unsigned long theWritten;
    return outFile.Write(this->GetData(), inLength, theWritten);
}

// FUNCTION: LLE 0x1000a6ab
// Writes the length, then the compacted contents to another file; returns only the second write's result
long FSMemoryFile::Save(FSFile& outFile)
{
    unsigned long theLength = 0;
    this->Compact();
    this->GetLength(theLength);
    outFile.Write(theLength);
    return this->Save(outFile, theLength);
}

// FUNCTION: LLE 0x1000a6ec
// Moves the position within the written data (from the end it counts backwards); never past the end
long FSMemoryFile::SetPosition(long inOffset, unsigned long inOrigin, bool inExtend)
{
    long theResult = kErrorOK;
    if (inOffset >= 0) {
        switch (inOrigin) {
        case kFileOriginCurrent:
            if (this->mCurrent + inOffset <= this->mEnd) {
                this->mCurrent += inOffset;
                if (this->mCurrent > this->mEnd)
                    this->mEnd = this->mCurrent;
            } else
                theResult = kErrorBeyondEnd;
            break;
        case kFileOriginEnd:
            if (inOffset <= this->mEnd - this->mStart)
                this->mCurrent = this->mEnd - inOffset;
            else
                theResult = kErrorBeyondEnd;
            break;
        case kFileOriginBegin:
            if (inOffset <= this->mEnd - this->mStart) {
                this->mCurrent = this->mStart + inOffset;
                if (this->mCurrent > this->mEnd)
                    this->mEnd = this->mCurrent;
            } else
                theResult = kErrorBeyondEnd;
            break;
        default:
            theResult = kErrorFailed;
            break;
        }
    } else
        theResult = kErrorBeyondEnd;
    return theResult;
}

// FUNCTION: LLE 0x1000a754
// Writes bytes at the current position, growing the buffer by a block or by doubling; kErrorNotOpen when closed
long FSMemoryFile::Write(const unsigned char* inData, unsigned long inLength, unsigned long& outWritten)
{
    if (this->mBuffer == NULL || !this->mIsOpen) {
        outWritten = 0;
        return kErrorNotOpen;
    }
    if (this->mCurrent + inLength > this->mLimit) {
        unsigned long theIncrease;
        if (this->mGrowByDoubling)
            theIncrease = this->mAllocatedLength < inLength ? inLength * 2 : this->mAllocatedLength;
        else
            theIncrease = this->mBlockSize < inLength ? this->mBlockSize + inLength : this->mBlockSize;
        this->IncreaseFileSize(theIncrease);
    }
    memcpy(this->mCurrent, inData, inLength);
    this->mCurrent += inLength;
    if (this->mCurrent > this->mEnd)
        this->mEnd = this->mCurrent;
    outWritten = inLength;
    return kErrorOK;
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

long FSMemoryFile::Write(const long& inValue)
{
    unsigned long theWritten;
    return this->Write(reinterpret_cast<const unsigned char*>(&inValue), sizeof(inValue), theWritten);
}

long FSMemoryFile::Write(const unsigned long& inValue)
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
