// C:\EEX\Low-Level Engine\File System\FSDebugFile.cpp (file name inferred: no __FILE__; RTTI .?AVFSDebugFile@@)
// Unit flags (docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
// Members in alphabetical order (as the original lays them out), private helpers last.
#include <windows.h>
#include <stdio.h>
#include <string.h>

#include "lle/LowLevelEngineCore.h"
#include "lle/UWideString.h"
#include "lle/UString.h"
#include "lle/FSMemoryFile.h"
#include "lle/FSDebugFile.h"

// Compiler-generated for this unit (names in analysis/lle/names.txt):

// FUNCTION: LLE 0x100080b9
// Creates a closed debug text file that mirrors every value read or written to the given delegate file
FSDebugFile::FSDebugFile(FSFile* inDelegate)
{
    this->mDelegate = inDelegate;
    this->mStream = NULL;
    this->mIsTextOutput = false;
    this->mIsTextInput = false;
}

// FUNCTION: LLE 0x100080e7
// Destroys the debug file, closing its text stream if open
FSDebugFile::~FSDebugFile()
{
    if (this->mIsOpen == true)
        this->Close();
}

// FUNCTION: LLE 0x1000810c
// Closes the text stream, then the delegate file; kErrorNotOpen when not open, native error if fclose fails
long FSDebugFile::Close()
{
    long theResult = kErrorOK;
    if (this->mIsOpen == true) {
        if (fclose(this->mStream) == 0) {
            this->mStream = NULL;
            this->mIsOpen = false;
            if (this->mDelegate)
                this->mDelegate->Close();
        } else {
            theResult = kErrorNativeError;
            ESRecordNativeError(GetLastError());
        }
    } else
        theResult = kErrorNotOpen;
    return theResult;
}

// FUNCTION: LLE 0x10008154
// Refuses to copy a debug file (always kErrorNotSupported)
long FSDebugFile::Copy(const UString& inDestination, bool inOverwrite, bool inKeepTimes)
{
    return kErrorNotSupported;
}

bool FSDebugFile::IsDirectory() const
{
    UString thePath(MAX_PATH);
    bool theResult = true;
    if (this->mFile.GetLength() == 0 && this->mVolume.GetLength() + this->mDirectory.GetLength() <= MAX_PATH) {
        thePath = this->mVolume + this->mDirectory;
        DWORD theAttributes = GetFileAttributesA(thePath);
        if (theAttributes == 0xffffffff || !(theAttributes & FILE_ATTRIBUTE_DIRECTORY))
            theResult = false;
    } else
        theResult = false;
    return theResult;
}

// FUNCTION: LLE 0x1000815a
// Opens the text stream for reading ("rt") or writing ("wt"); other access modes fail
long FSDebugFile::Open(unsigned long inAccess)
{
    long theResult = kErrorOK;
    UString thePath(MAX_PATH);
    UString theMode;
    this->mIsOpen = true;
    this->mFileAccess = inAccess;
    if (this->mVolume.GetLength() + this->mDirectory.GetLength() + this->mFile.GetLength() <= MAX_PATH) {
        if (inAccess == kFileAccessRead || inAccess == kFileAccessWrite) {
            if (inAccess == kFileAccessRead)
                theMode = "rt";
            else
                theMode = "wt";
        } else {
            this->mIsOpen = false;
            theResult = kErrorFailed;
        }
        if (theResult == kErrorOK) {
        thePath = this->mVolume + this->mDirectory + this->mFile;
        this->mStream = fopen(thePath, theMode);
        if (this->mStream == NULL) {
            this->mIsOpen = false;
            theResult = kErrorNativeError;
            ESRecordNativeError(GetLastError());
        } else if (strrchr(theMode, 'w') && strrchr(theMode, 't'))
            this->mIsTextOutput = true;
        else if (strrchr(theMode, 'r') && strrchr(theMode, 't'))
            this->mIsTextInput = true;
        }
    } else {
        this->mIsOpen = false;
        theResult = kErrorBadPath;
    }
    return theResult;
}

// FUNCTION: LLE 0x1000829b
// Removes one indent level (two spaces) from the "name: value" lines written to the text stream
void FSDebugFile::PopIndentHint()
{
    if (!this->mStream || !this->mIsTextOutput)
        return;
    long theLength = this->mIndent.GetLength();
    if (theLength >= 2)
    {
        UString theIndent = this->mIndent.GetSubstring(0, theLength - 2);
        this->mIndent = theIndent;
    }
}

// FUNCTION: LLE 0x100082df
// Adds one indent level (two spaces) to the "name: value" lines written to the text stream
void FSDebugFile::PushIndentHint()
{
    if (this->mStream && this->mIsTextOutput)
        this->mIndent += UString("  ");
}

// FUNCTION: LLE 0x10008315
// Reads raw bytes from the text stream; kErrorIOFailed on a short read, kErrorNotOpen (0 read) when closed
long FSDebugFile::ReadText(unsigned char* outData, unsigned long inLength, unsigned long& outRead)
{
    long theResult = kErrorOK;
    if (this->mIsOpen == true) {
        outRead = fread(outData, 1, inLength, this->mStream);
        if (outRead != inLength)
            theResult = kErrorIOFailed;
    } else {
        theResult = kErrorNotOpen;
        outRead = 0;
    }
    return theResult;
}

// No marker: the char overload (reads raw bytes from the text stream), the same body ICF-folded into the
// unsigned char one above (0x10008315).
long FSDebugFile::ReadText(char* outData, unsigned long inLength, unsigned long& outRead)
{
    long theResult = kErrorOK;
    if (this->mIsOpen == true) {
        outRead = fread(outData, 1, inLength, this->mStream);
        if (outRead != inLength)
            theResult = kErrorIOFailed;
    } else {
        theResult = kErrorNotOpen;
        outRead = 0;
    }
    return theResult;
}

// FUNCTION: LLE 0x10008354
// Parses an unsigned decimal value from the text stream; kErrorFailed if none
long FSDebugFile::ReadText(unsigned long& outValue)
{
    long theResult = kErrorOK;
    if (fscanf(this->mStream, " %u ", &outValue) != 1)
        theResult = kErrorFailed;
    return theResult;
}

// FUNCTION: LLE 0x1000837a
// Parses a signed decimal value from the text stream; kErrorFailed if none
long FSDebugFile::ReadText(long& outValue)
{
    long theResult = kErrorOK;
    if (fscanf(this->mStream, " %d ", &outValue) != 1)
        theResult = kErrorFailed;
    return theResult;
}

// FUNCTION: LLE 0x100083a0
// Parses an unsigned short from the text stream; kErrorFailed if none
long FSDebugFile::ReadText(unsigned short& outValue)
{
    long theResult = kErrorOK;
    if (fscanf(this->mStream, " %hu ", &outValue) != 1)
        theResult = kErrorFailed;
    return theResult;
}

// FUNCTION: LLE 0x100083c6
// Parses a short from the text stream; kErrorFailed if none
long FSDebugFile::ReadText(short& outValue)
{
    long theResult = kErrorOK;
    if (fscanf(this->mStream, " %hd ", &outValue) != 1)
        theResult = kErrorFailed;
    return theResult;
}

// FUNCTION: LLE 0x100083ec
// Parses an unsigned byte from the text stream; kErrorFailed if none
long FSDebugFile::ReadText(unsigned char& outValue)
{
    long theResult = kErrorOK;
    unsigned int theValue;  // Fixed: was a one-byte local, into which "%u" stored four bytes (stack overwrite)
    if (fscanf(this->mStream, " %u ", &theValue) != 1)
        theResult = kErrorFailed;
    else
        outValue = theValue;
    return theResult;
}

// FUNCTION: LLE 0x10008421
// Parses a decimal value from the text stream and stores it as a signed byte; kErrorFailed if none
long FSDebugFile::ReadText(signed char& outValue)
{
    long theResult = kErrorOK;
    long theValue = 0;
    if (fscanf(this->mStream, " %d ", &theValue) != 1)
        theResult = kErrorFailed;
    else
        outValue = (signed char)theValue;
    return theResult;
}

// FUNCTION: LLE 0x10008459
// Parses a float from the text stream; kErrorFailed if none
long FSDebugFile::ReadText(float& outValue)
{
    long theResult = kErrorOK;
    if (fscanf(this->mStream, " %f ", &outValue) != 1)
        theResult = kErrorFailed;
    return theResult;
}

// FUNCTION: LLE 0x1000847f
// Parses a double from the text stream; kErrorFailed if none
long FSDebugFile::ReadText(double& outValue)
{
    long theResult = kErrorOK;
    if (fscanf(this->mStream, " %lf ", &outValue) != 1)
        theResult = kErrorFailed;
    return theResult;
}

// FUNCTION: LLE 0x100084a5
// Parses 0 or 1 from the text stream as a bool; any other number leaves the value unchanged and succeeds
long FSDebugFile::ReadText(bool& outValue)
{
    long theResult = kErrorOK;
    long theValue = 0;
    if (fscanf(this->mStream, " %d ", &theValue) != 1)
        theResult = kErrorFailed;
    else if (theValue == 0)
        outValue = false;
    else if (theValue == 1)
        outValue = true;
    return theResult;
}

// FUNCTION: LLE 0x100084ef
// Reads raw bytes from the delegate file
long FSDebugFile::Read(unsigned char* outData, unsigned long inLength, unsigned long& outRead)
{
    return this->mDelegate->Read(outData, inLength, outRead);
}

// FUNCTION: LLE 0x10008506
// Reads raw characters from the delegate file
long FSDebugFile::Read(char* outData, unsigned long inLength, unsigned long& outRead)
{
    return this->mDelegate->Read(outData, inLength, outRead);
}

// FUNCTION: LLE 0x1000851d
// Reads an unsigned long from the delegate file
long FSDebugFile::Read(unsigned long& outValue)
{
    return this->mDelegate->Read(outValue);
}

// FUNCTION: LLE 0x1000852c
// Reads a long from the delegate file
long FSDebugFile::Read(long& outValue)
{
    return this->mDelegate->Read(outValue);
}

// FUNCTION: LLE 0x1000853b
// Reads an unsigned short from the delegate file
long FSDebugFile::Read(unsigned short& outValue)
{
    return this->mDelegate->Read(outValue);
}

// FUNCTION: LLE 0x1000854a
// Reads a short from the delegate file
long FSDebugFile::Read(short& outValue)
{
    return this->mDelegate->Read(outValue);
}

// FUNCTION: LLE 0x10008559
// Reads an unsigned byte from the delegate file
long FSDebugFile::Read(unsigned char& outValue)
{
    return this->mDelegate->Read(outValue);
}

// FUNCTION: LLE 0x10008568
// Reads a signed byte from the delegate file
long FSDebugFile::Read(signed char& outValue)
{
    return this->mDelegate->Read(outValue);
}

// FUNCTION: LLE 0x10008577
// Reads a float from the delegate file
long FSDebugFile::Read(float& outValue)
{
    return this->mDelegate->Read(outValue);
}

// FUNCTION: LLE 0x10008586
// Reads a double from the delegate file
long FSDebugFile::Read(double& outValue)
{
    return this->mDelegate->Read(outValue);
}

// FUNCTION: LLE 0x10008595
// Reads a bool from the delegate file
long FSDebugFile::Read(bool& outValue)
{
    return this->mDelegate->Read(outValue);
}

// FUNCTION: LLE 0x100085a4
// Reads a "name:" label and an unsigned long from the text stream, then the value again from the delegate
long FSDebugFile::ReadInfo(UString& outName, unsigned long& outValue)
{
    long theResult = this->ReadPossibleInfoString(outName);
    if (theResult != kErrorOK)
        return theResult;
    theResult = this->ReadText(outValue);
    if (theResult == kErrorOK && this->mDelegate)
        theResult = this->Read(outValue);
    return theResult;
}

// FUNCTION: LLE 0x100085d7
// Reads a "name:" label and a long from the text stream, then the value again from the delegate
long FSDebugFile::ReadInfo(UString& outName, long& outValue)
{
    long theResult = this->ReadPossibleInfoString(outName);
    if (theResult != kErrorOK)
        return theResult;
    theResult = this->ReadText(outValue);
    if (theResult == kErrorOK && this->mDelegate)
        theResult = this->Read(outValue);
    return theResult;
}

// FUNCTION: LLE 0x1000860a
// Reads a "name:" label and an unsigned short from the text stream, then the value again from the delegate
long FSDebugFile::ReadInfo(UString& outName, unsigned short& outValue)
{
    long theResult = this->ReadPossibleInfoString(outName);
    if (theResult != kErrorOK)
        return theResult;
    theResult = this->ReadText(outValue);
    if (theResult == kErrorOK && this->mDelegate)
        theResult = this->Read(outValue);
    return theResult;
}

// FUNCTION: LLE 0x1000863d
// Reads a "name:" label and a short from the text stream, then the value again from the delegate
long FSDebugFile::ReadInfo(UString& outName, short& outValue)
{
    long theResult = this->ReadPossibleInfoString(outName);
    if (theResult != kErrorOK)
        return theResult;
    theResult = this->ReadText(outValue);
    if (theResult == kErrorOK && this->mDelegate)
        theResult = this->Read(outValue);
    return theResult;
}

// FUNCTION: LLE 0x10008670
// Reads a "name:" label and an unsigned byte from the text stream, then the value again from the delegate
long FSDebugFile::ReadInfo(UString& outName, unsigned char& outValue)
{
    long theResult = this->ReadPossibleInfoString(outName);
    if (theResult != kErrorOK)
        return theResult;
    theResult = this->ReadText(outValue);
    if (theResult == kErrorOK && this->mDelegate)
        theResult = this->Read(outValue);
    return theResult;
}

// FUNCTION: LLE 0x100086a3
// Reads a "name:" label and a signed byte from the text stream, then the value again from the delegate
long FSDebugFile::ReadInfo(UString& outName, signed char& outValue)
{
    long theResult = this->ReadPossibleInfoString(outName);
    if (theResult != kErrorOK)
        return theResult;
    theResult = this->ReadText(outValue);
    if (theResult == kErrorOK && this->mDelegate)
        theResult = this->Read(outValue);
    return theResult;
}

// FUNCTION: LLE 0x100086d6
// Reads a "name:" label and a float from the text stream, then the value again from the delegate
long FSDebugFile::ReadInfo(UString& outName, float& outValue)
{
    long theResult = this->ReadPossibleInfoString(outName);
    if (theResult != kErrorOK)
        return theResult;
    theResult = this->ReadText(outValue);
    if (theResult == kErrorOK && this->mDelegate)
        theResult = this->Read(outValue);
    return theResult;
}

// FUNCTION: LLE 0x10008709
// Reads a "name:" label and a double from the text stream, then the value again from the delegate
long FSDebugFile::ReadInfo(UString& outName, double& outValue)
{
    long theResult = this->ReadPossibleInfoString(outName);
    if (theResult != kErrorOK)
        return theResult;
    theResult = this->ReadText(outValue);
    if (theResult == kErrorOK && this->mDelegate)
        theResult = this->Read(outValue);
    return theResult;
}

// FUNCTION: LLE 0x1000873c
// Reads a "name:" label and a bool from the text stream, then the value again from the delegate
long FSDebugFile::ReadInfo(UString& outName, bool& outValue)
{
    long theResult = this->ReadPossibleInfoString(outName);
    if (theResult != kErrorOK)
        return theResult;
    theResult = this->ReadText(outValue);
    if (theResult == kErrorOK && this->mDelegate)
        theResult = this->Read(outValue);
    return theResult;
}

// FUNCTION: LLE 0x1000876f
// Sets the file every value is also read from and written to
void FSDebugFile::SetDelegate(FSFile* inDelegate)
{
    this->mDelegate = inDelegate;
}

// FUNCTION: LLE 0x10008779
// Writes raw bytes to the text stream; kErrorIOFailed on a short write (outWritten is never set)
long FSDebugFile::WriteText(const unsigned char* inData, unsigned long inLength, unsigned long& outWritten)
{
    long theResult = kErrorOK;
    if (this->mIsOpen == true) {
        if (fwrite(inData, 1, inLength, this->mStream) != inLength)
            theResult = kErrorIOFailed;
    } else
        theResult = kErrorNotOpen;
    return theResult;
}

// FUNCTION: LLE 0x100087ab
// Prints an unsigned long to the text stream; kErrorNotOpen when closed
long FSDebugFile::WriteText(const unsigned long& inValue)
{
    long theResult = kErrorOK;
    if (this->mIsOpen == true) {
        if (fprintf(this->mStream, " %u ", inValue) <= 0)
            theResult = kErrorNativeError;
    } else
        theResult = kErrorNotOpen;
    return theResult;
}

// FUNCTION: LLE 0x100087dc
// Prints a long to the text stream; kErrorNotOpen when closed
long FSDebugFile::WriteText(const long& inValue)
{
    long theResult = kErrorOK;
    if (this->mIsOpen == true) {
        if (fprintf(this->mStream, " %d ", inValue) <= 0)
            theResult = kErrorNativeError;
    } else
        theResult = kErrorNotOpen;
    return theResult;
}

// FUNCTION: LLE 0x1000880d
// Prints an unsigned short to the text stream; kErrorNotOpen when closed
long FSDebugFile::WriteText(const unsigned short& inValue)
{
    long theResult = kErrorOK;
    if (this->mIsOpen == true) {
        if (fprintf(this->mStream, " %u ", inValue) <= 0)
            theResult = kErrorNativeError;
    } else
        theResult = kErrorNotOpen;
    return theResult;
}

// FUNCTION: LLE 0x10008840
// Prints a short to the text stream; kErrorNotOpen when closed
long FSDebugFile::WriteText(const short& inValue)
{
    long theResult = kErrorOK;
    if (this->mIsOpen == true) {
        if (fprintf(this->mStream, " %d ", inValue) <= 0)
            theResult = kErrorNativeError;
    } else
        theResult = kErrorNotOpen;
    return theResult;
}

// FUNCTION: LLE 0x10008873
// Prints an unsigned byte to the text stream; kErrorNotOpen when closed
long FSDebugFile::WriteText(const unsigned char& inValue)
{
    long theResult = kErrorOK;
    if (this->mIsOpen == true) {
        if (fprintf(this->mStream, " %u ", inValue) <= 0)
            theResult = kErrorNativeError;
    } else
        theResult = kErrorNotOpen;
    return theResult;
}

// FUNCTION: LLE 0x100088a6
// Prints a signed byte to the text stream; kErrorNotOpen when closed
long FSDebugFile::WriteText(const signed char& inValue)
{
    long theResult = kErrorOK;
    if (this->mIsOpen == true) {
        if (fprintf(this->mStream, " %d ", inValue) <= 0)
            theResult = kErrorNativeError;
    } else
        theResult = kErrorNotOpen;
    return theResult;
}

// FUNCTION: LLE 0x100088d9
// Prints a float to the text stream; kErrorNotOpen when closed
long FSDebugFile::WriteText(const float& inValue)
{
    long theResult = kErrorOK;
    if (this->mIsOpen == true) {
        if (fprintf(this->mStream, " %f ", inValue) <= 0)
            theResult = kErrorNativeError;
    } else
        theResult = kErrorNotOpen;
    return theResult;
}

// FUNCTION: LLE 0x1000890f
// Prints a double to the text stream; kErrorNotOpen when closed
long FSDebugFile::WriteText(const double& inValue)
{
    long theResult = kErrorOK;
    if (this->mIsOpen == true) {
        if (fprintf(this->mStream, " %lf ", inValue) <= 0)
            theResult = kErrorNativeError;
    } else
        theResult = kErrorNotOpen;
    return theResult;
}

// FUNCTION: LLE 0x10008945
// Prints a bool as a number to the text stream; kErrorNotOpen when closed
long FSDebugFile::WriteText(const bool& inValue)
{
    long theResult = kErrorOK;
    if (this->mIsOpen == true) {
        if (fprintf(this->mStream, " %d ", inValue) <= 0)
            theResult = kErrorNativeError;
    } else
        theResult = kErrorNotOpen;
    return theResult;
}

// FUNCTION: LLE 0x10008978
// Writes raw bytes to the delegate file
long FSDebugFile::Write(const unsigned char* inData, unsigned long inLength, unsigned long& outWritten)
{
    return this->mDelegate->Write(inData, inLength, outWritten);
}

// FUNCTION: LLE 0x10008992
// Writes raw characters to the delegate file
long FSDebugFile::Write(const char* inData, unsigned long inLength, unsigned long& outWritten)
{
    return this->mDelegate->Write(inData, inLength, outWritten);
}

// FUNCTION: LLE 0x100089ac
// Writes an unsigned long to the delegate file
long FSDebugFile::Write(const unsigned long& inValue)
{
    return this->mDelegate->Write(inValue);
}

// FUNCTION: LLE 0x100089be
// Writes a long to the delegate file
long FSDebugFile::Write(const long& inValue)
{
    return this->mDelegate->Write(inValue);
}

// FUNCTION: LLE 0x100089d0
// Writes an unsigned short to the delegate file
long FSDebugFile::Write(const unsigned short& inValue)
{
    return this->mDelegate->Write(inValue);
}

// FUNCTION: LLE 0x100089e2
// Writes a short to the delegate file
long FSDebugFile::Write(const short& inValue)
{
    return this->mDelegate->Write(inValue);
}

// FUNCTION: LLE 0x100089f4
// Writes an unsigned byte to the delegate file
long FSDebugFile::Write(const unsigned char& inValue)
{
    return this->mDelegate->Write(inValue);
}

// FUNCTION: LLE 0x10008a06
// Writes a signed byte to the delegate file
long FSDebugFile::Write(const signed char& inValue)
{
    return this->mDelegate->Write(inValue);
}

// FUNCTION: LLE 0x10008a18
// Writes a float to the delegate file
long FSDebugFile::Write(const float& inValue)
{
    return this->mDelegate->Write(inValue);
}

// FUNCTION: LLE 0x10008a27
// Writes a double to the delegate file
long FSDebugFile::Write(const double& inValue)
{
    return this->mDelegate->Write(inValue);
}

// FUNCTION: LLE 0x10008a36
// Writes a bool to the delegate file
long FSDebugFile::Write(const bool& inValue)
{
    return this->mDelegate->Write(inValue);
}

// FUNCTION: LLE 0x10008a45
// Writes a "name: value" line for an unsigned long to the text stream, then the value to the delegate
long FSDebugFile::WriteInfo(const UString& inName, const unsigned long& inValue)
{
    long theResult = this->WritePossibleInfoString(inName);
    if (theResult != kErrorOK)
        return theResult;
    theResult = this->WriteText(inValue);
    if (theResult == kErrorOK && this->mDelegate)
        theResult = this->Write(inValue);
    return theResult;
}

// FUNCTION: LLE 0x10008a7b
// Writes a "name: value" line for a long to the text stream, then the value to the delegate
long FSDebugFile::WriteInfo(const UString& inName, const long& inValue)
{
    long theResult = this->WritePossibleInfoString(inName);
    if (theResult != kErrorOK)
        return theResult;
    theResult = this->WriteText(inValue);
    if (theResult == kErrorOK && this->mDelegate)
        theResult = this->Write(inValue);
    return theResult;
}

// FUNCTION: LLE 0x10008ab1
// Writes a "name: value" line for an unsigned short to the text stream, then the value to the delegate
long FSDebugFile::WriteInfo(const UString& inName, const unsigned short& inValue)
{
    long theResult = this->WritePossibleInfoString(inName);
    if (theResult != kErrorOK)
        return theResult;
    theResult = this->WriteText(inValue);
    if (theResult == kErrorOK && this->mDelegate)
        theResult = this->Write(inValue);
    return theResult;
}

// FUNCTION: LLE 0x10008ae7
// Writes a "name: value" line for a short to the text stream, then the value to the delegate
long FSDebugFile::WriteInfo(const UString& inName, const short& inValue)
{
    long theResult = this->WritePossibleInfoString(inName);
    if (theResult != kErrorOK)
        return theResult;
    theResult = this->WriteText(inValue);
    if (theResult == kErrorOK && this->mDelegate)
        theResult = this->Write(inValue);
    return theResult;
}

// FUNCTION: LLE 0x10008b1d
// Writes a "name: value" line for an unsigned byte to the text stream, then the value to the delegate
long FSDebugFile::WriteInfo(const UString& inName, const unsigned char& inValue)
{
    long theResult = this->WritePossibleInfoString(inName);
    if (theResult != kErrorOK)
        return theResult;
    theResult = this->WriteText(inValue);
    if (theResult == kErrorOK && this->mDelegate)
        theResult = this->Write(inValue);
    return theResult;
}

// FUNCTION: LLE 0x10008b53
// Writes a "name: value" line for a signed byte to the text stream, then the value to the delegate
long FSDebugFile::WriteInfo(const UString& inName, const signed char& inValue)
{
    long theResult = this->WritePossibleInfoString(inName);
    if (theResult != kErrorOK)
        return theResult;
    theResult = this->WriteText(inValue);
    if (theResult == kErrorOK && this->mDelegate)
        theResult = this->Write(inValue);
    return theResult;
}

// FUNCTION: LLE 0x10008b89
// Writes a "name: value" line for a float to the text stream, then the value to the delegate
long FSDebugFile::WriteInfo(const UString& inName, const float& inValue)
{
    long theResult = this->WritePossibleInfoString(inName);
    if (theResult != kErrorOK)
        return theResult;
    theResult = this->WriteText(inValue);
    if (theResult == kErrorOK && this->mDelegate)
        theResult = this->Write(inValue);
    return theResult;
}

// FUNCTION: LLE 0x10008bbc
// Writes a "name: value" line for a double to the text stream, then the value to the delegate
long FSDebugFile::WriteInfo(const UString& inName, const double& inValue)
{
    long theResult = this->WritePossibleInfoString(inName);
    if (theResult != kErrorOK)
        return theResult;
    theResult = this->WriteText(inValue);
    if (theResult == kErrorOK && this->mDelegate)
        theResult = this->Write(inValue);
    return theResult;
}

// FUNCTION: LLE 0x10008bef
// Writes a "name: value" line for a bool to the text stream, then the value to the delegate
long FSDebugFile::WriteInfo(const UString& inName, const bool& inValue)
{
    long theResult = this->WritePossibleInfoString(inName);
    if (theResult != kErrorOK)
        return theResult;
    theResult = this->WriteText(inValue);
    if (theResult == kErrorOK && this->mDelegate)
        theResult = this->Write(inValue);
    return theResult;
}

// FUNCTION: LLE 0x10008c22
// Reads the next "name:" label (up to 255 characters) when reading text; native error if none is found
long FSDebugFile::ReadPossibleInfoString(UString& outName)
{
    char theName[256];
    long theResult = kErrorOK;
    if (this->mStream && this->mIsTextInput) {
        memset(theName, 0, sizeof(theName));
        if (fscanf(this->mStream, "\n%255[^:]:", theName) <= 0)
            theResult = kErrorNativeError;
        else
            outName = theName;
    }
    return theResult;
}

// FUNCTION: LLE 0x10008c87
// Skips a line end when reading text; native error if the stream fails
long FSDebugFile::ReadPossibleEOL()
{
    long theResult = kErrorOK;
    if (this->mStream && this->mIsTextInput) {
        // Fixed: tested fscanf's conversion count, which is always 0 for this format, so it always failed
        if (fscanf(this->mStream, "\r\n") == EOF && ferror(this->mStream))
            theResult = kErrorNativeError;
    }
    return theResult;
}

// FUNCTION: LLE 0x10008cb2
// Starts a new indented "name: " line when writing text; native error if the print fails
long FSDebugFile::WritePossibleInfoString(const UString& inName)
{
    long theResult = kErrorOK;
    if (this->mStream && this->mIsTextOutput) {
        if (fprintf(this->mStream, "\n%s%s: ", (const char*)this->mIndent, (const char*)inName) <= 0)
            theResult = kErrorNativeError;
    }
    return theResult;
}

// FUNCTION: LLE 0x10008cea
// Writes a line end when writing text; native error if the print fails
long FSDebugFile::WritePossibleEOL()
{
    long theResult = kErrorOK;
    if (this->mStream && this->mIsTextOutput) {
        if (fprintf(this->mStream, "\n", NULL) <= 0)
            theResult = kErrorNativeError;
    }
    return theResult;
}
