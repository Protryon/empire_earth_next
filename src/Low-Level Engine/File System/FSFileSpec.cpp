// C:\EEX\Low-Level Engine\File System\FSFileSpec.cpp
// Unit flags (docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
// Whole in Rust (rust/crates/ee_fs/src/spec.rs, LLE lane round 5): each member forwards the object, from its vtable
// pointer on (EeFsSpec), to Rust, which makes the original's platform calls in its order. operator< and
// GetLengthFromClosedFile stay here: they call the virtuals FSMemoryFile overrides.
#include <windows.h>
#include "platform/PlatFile.h"
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "lle/LowLevelEngineCore.h"
#include "lle/UWideString.h"
#include "lle/UString.h"
#include "lle/FSMemoryFile.h"
#include "rust/ee_fs.h"

static_assert(sizeof(UString) == sizeof(EeFsString), "a UString member is an EeFsString");
static_assert(sizeof(FSFileSpec) == sizeof(EeFsSpec), "FSFileSpec is an EeFsSpec");

static EeFsSpec* Record(FSFileSpec* inSpec) { return reinterpret_cast<EeFsSpec*>(inSpec); }
static const EeFsSpec* Record(const FSFileSpec* inSpec) { return reinterpret_cast<const EeFsSpec*>(inSpec); }
static const EeFsString* StartupVolume() { return reinterpret_cast<const EeFsString*>(&gStartupVolume); }
static const EeFsString* StartupDirectory() { return reinterpret_cast<const EeFsString*>(&gStartupDirectory); }

// FUNCTION: LLE 0x10008d15
// Creates a closed file spec for the game's startup directory with no file name
FSFileSpec::FSFileSpec()
{
    static_assert(offsetof(FSFileSpec, mVolume) == offsetof(EeFsSpec, volume), "layout");
    static_assert(offsetof(FSFileSpec, mFileAccess) == offsetof(EeFsSpec, file_access), "layout");
    static_assert(offsetof(FSFileSpec, mIsOpen) == offsetof(EeFsSpec, is_open), "layout");
    static_assert(offsetof(FSFileSpec, mHandle) == offsetof(EeFsSpec, handle), "layout");
    static_assert(offsetof(FSFileSpec, mStream) == offsetof(EeFsSpec, stream), "layout");
    ee_fs_spec_init(Record(this), StartupVolume(), StartupDirectory());
}

// FUNCTION: LLE 0x10008dad
// Copies a file spec's path; the copy is closed and has no search handle
FSFileSpec::FSFileSpec(const FSFileSpec& inSpec)
    : FSBasicFile(inSpec), mHandle(NULL), mStream(NULL)
{
}

// FUNCTION: LLE 0x10008dcd
// Creates a closed file spec from a full path, split into volume, directory and file name
FSFileSpec::FSFileSpec(const UString& inPath)
{
    ee_fs_spec_init_path(Record(this), reinterpret_cast<const EeFsString*>(&inPath), StartupVolume(),
                         StartupDirectory());
}

// FUNCTION: LLE 0x10008efe
// Destroys the file spec, closing the file or an open directory search
FSFileSpec::~FSFileSpec()
{
    ee_fs_spec_finish(Record(this));
}

// FUNCTION: LLE 0x10008f2f
// Copies another spec's path into this one, leaving it closed with no search handle
FSFileSpec& FSFileSpec::operator=(const FSFileSpec& inSpec)
{
    ee_fs_spec_assign(Record(this), Record(&inSpec));
    return *this;
}

// FUNCTION: LLE 0x10008f74
// Returns whether this file was last written before the other one, opening either as needed (true on failure)
bool FSFileSpec::operator<(FSFileSpec& inSpec)
{
    bool theOpenedThis = false;
    bool theOpenedOther = false;
    bool theResult = true;
    uint64_t theThisTime = 0;
    uint64_t theOtherTime = 0;
    if (mHandle == NULL && Open(1) == kErrorOK)
        theOpenedThis = true;
    if (inSpec.mHandle == NULL && inSpec.Open(1) == kErrorOK)
        theOpenedOther = true;
    if (mHandle && inSpec.mHandle) {
        PlatFileGetWriteTime(static_cast<PlatFile*>(mHandle), &theThisTime);
        PlatFileGetWriteTime(static_cast<PlatFile*>(inSpec.mHandle), &theOtherTime);
        theResult = theThisTime < theOtherTime;
    }
    if (theOpenedThis == true)
        Close();
    if (theOpenedOther == true)
        inSpec.Close();
    return theResult;
}

// 0x13d6 (inline in the original)
void FSFileSpec::CloseSearchHandle()
{
    ee_fs_spec_close_search(Record(this));
}

// FUNCTION: LLE 0x1000901a
long FSFileSpec::Create()
{
    return ee_fs_spec_create(Record(this));
}

// FUNCTION: LLE 0x10009142
long FSFileSpec::CreateDirectory()
{
    return ee_fs_spec_create_directory(Record(this));
}

// FUNCTION: LLE 0x100091cd
long FSFileSpec::Close()
{
    return ee_fs_spec_close(Record(this));
}

// FUNCTION: LLE 0x1000921c
long FSFileSpec::Copy(const UString& inDestination, bool inFailIfExists, bool inKeepAttributes)
{
    return ee_fs_spec_copy(Record(this), inDestination, inFailIfExists, inKeepAttributes);
}

// FUNCTION: LLE 0x100092ca
long FSFileSpec::DeleteDirectory()
{
    return ee_fs_spec_delete_directory(Record(this));
}

// FUNCTION: LLE 0x1000935a
long FSFileSpec::FOpen(char* inMode)
{
    return ee_fs_spec_fopen(Record(this), inMode);
}

// FUNCTION: LLE 0x10009406
long FSFileSpec::GetFirstFile(const UString& inPattern, FSFileSpec& outFile)
{
    return ee_fs_spec_first_file(Record(this), inPattern, Record(&outFile), StartupVolume(), StartupDirectory());
}

// FUNCTION: LLE 0x100095e3
long FSFileSpec::GetLength(unsigned long& outLength) const
{
    return ee_fs_spec_get_length(Record(this), &outLength);
}

// FUNCTION: LLE 0x10009627
// Opens the file just to read its length, then closes it; returns OK even when it cannot open it (length 0)
long FSFileSpec::GetLengthFromClosedFile(unsigned long& outLength)
{
    unsigned long theLength = 0;
    if (this->Open(kFileAccessRead) != kErrorOK) {
        // Fixed: outLength used to be left unset here (the caller saw success with a stale length)
        outLength = 0;
        return kErrorOK;
    }
    this->GetLength(theLength);
    this->Close();
    outLength = theLength;
    return kErrorOK;
}

// FUNCTION: LLE 0x10009661
long FSFileSpec::GetNextFile(FSFileSpec& outFile)
{
    return ee_fs_spec_next_file(Record(this), Record(&outFile));
}

// FUNCTION: LLE 0x10009816
void FSFileSpec::GetParentDirectory(FSFileSpec& outDirectory) const
{
    ee_fs_spec_parent(Record(this), Record(&outDirectory));
}

// FUNCTION: LLE 0x100098d8
long FSFileSpec::GetPosition(long& outPosition) const
{
    return ee_fs_spec_get_position(Record(this), &outPosition);
}

// FUNCTION: LLE 0x1000991f
bool FSFileSpec::IsDirectory() const
{
    return ee_fs_spec_is_directory(Record(this));
}

// FUNCTION: LLE 0x10009992
long FSFileSpec::Lock()
{
    return ee_fs_spec_lock(Record(this));
}

// FUNCTION: LLE 0x10009a41
long FSFileSpec::Open(unsigned long inAccess)
{
    return ee_fs_spec_open(Record(this), inAccess);
}

// FUNCTION: LLE 0x10009b48
long FSFileSpec::Read(unsigned char* outData, unsigned long inLength, unsigned long& outRead)
{
    return ee_fs_spec_read(Record(this), outData, inLength, &outRead);
}

// The scalar reads (0x10009ba5 and the folded ones): kErrorFailed on a short read
template <typename T> static long ReadValue(FSFileSpec* inSpec, T& outValue)
{
    return ee_fs_spec_read_value(Record(inSpec), reinterpret_cast<uint8_t*>(&outValue), sizeof(outValue));
}
long FSFileSpec::Read(float& outValue) { return ReadValue(this, outValue); }
long FSFileSpec::Read(int32_t& outValue) { return ReadValue(this, outValue); }
long FSFileSpec::Read(uint32_t& outValue) { return ReadValue(this, outValue); }
long FSFileSpec::Read(short& outValue) { return ReadValue(this, outValue); }
long FSFileSpec::Read(unsigned short& outValue) { return ReadValue(this, outValue); }
long FSFileSpec::Read(double& outValue) { return ReadValue(this, outValue); }
long FSFileSpec::Read(signed char& outValue) { return ReadValue(this, outValue); }
long FSFileSpec::Read(unsigned char& outValue) { return ReadValue(this, outValue); }
long FSFileSpec::Read(bool& outValue) { return ReadValue(this, outValue); }

// FUNCTION: LLE 0x10009c41
long FSFileSpec::SetLength(unsigned long& ioLength, bool inTruncate)
{
    return ee_fs_spec_set_length(Record(this), ioLength);
}

// FUNCTION: LLE 0x10009c9f
long FSFileSpec::SetPosition(long inOffset, unsigned long inOrigin, bool inExtend)
{
    return ee_fs_spec_set_position(Record(this), inOffset, inOrigin, inExtend);
}

// FUNCTION: LLE 0x10009d44
long FSFileSpec::Unlock()
{
    return ee_fs_spec_unlock(Record(this));
}

// FUNCTION: LLE 0x10009df6
long FSFileSpec::Write(const unsigned char* inData, unsigned long inLength, unsigned long& outWritten)
{
    return ee_fs_spec_write(Record(this), inData, inLength, &outWritten);
}

// The scalar writes (0x10009e51 and the folded ones)
template <typename T> static long WriteValue(FSFileSpec* inSpec, const T& inValue)
{
    return ee_fs_spec_write_value(Record(inSpec), reinterpret_cast<const uint8_t*>(&inValue), sizeof(inValue));
}
long FSFileSpec::Write(const float& inValue) { return WriteValue(this, inValue); }
long FSFileSpec::Write(const int32_t& inValue) { return WriteValue(this, inValue); }
long FSFileSpec::Write(const uint32_t& inValue) { return WriteValue(this, inValue); }
long FSFileSpec::Write(const short& inValue) { return WriteValue(this, inValue); }
long FSFileSpec::Write(const unsigned short& inValue) { return WriteValue(this, inValue); }
long FSFileSpec::Write(const signed char& inValue) { return WriteValue(this, inValue); }
long FSFileSpec::Write(const unsigned char& inValue) { return WriteValue(this, inValue); }
long FSFileSpec::Write(const bool& inValue) { return WriteValue(this, inValue); }
long FSFileSpec::Write(const double& inValue) { return WriteValue(this, inValue); }

// Returns whether the file is open. Folded by ICF into an identical body elsewhere (0x1004b4d9)
bool FSFileSpec::GetIsOpen() const
{
    return this->mIsOpen;
}

// SYNTHETIC: LLE 0x10008d53
