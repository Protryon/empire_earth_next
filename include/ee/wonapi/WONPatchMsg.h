// The version and patch records and the patch ops' messages through ee_won's patch module
// (rust/crates/ee_won/src/patch.rs): Rust reads a record or reply into the original's ordered assignments (a cut
// one keeps what was read, a string cut after its length erased) and writes the records and requests; these apply
// them through the records' setters. Templates over the record types so that tests/won/won_misc_test.cpp runs the
// same code over stand-ins.
#pragma once
#include <set>
#include <string>
#include <vector>
#include "ee/wonapi/WONBuffer.h"
#include "rust/ee_won.h"

namespace WONAPI {

inline void PatchFieldSink(void* inContext, const EeWonPatchField* inFields, size_t inCount)
{
    static_cast<std::vector<EeWonPatchField>*>(inContext)->assign(inFields, inFields + inCount);
}
inline void PatchAppendSink(void* inContext, const uint8_t* inBytes, size_t inLength)
{
    static_cast<WriteBuffer*>(inContext)->AppendBytes(inBytes, static_cast<unsigned long>(inLength));
}
inline EeWonPatchBytes PatchBytes(const std::string& inText)
{
    EeWonPatchBytes b = {reinterpret_cast<const uint8_t*>(inText.data()), inText.size()};
    return b;
}

// The fields of a read of the reader's remaining bytes (*outBase: where they start)
inline std::vector<EeWonPatchField> PatchRead(int inKind, ReadBuffer& ioIn, const char** outBase)
{
    std::vector<EeWonPatchField> f;
    const char* aBase = ioIn.data() + ioIn.pos();
    ee_won_patch_read(inKind, reinterpret_cast<const uint8_t*>(aBase), ioIn.Available(), PatchFieldSink, &f);
    *outBase = aBase;
    return f;
}
inline std::string PatchString(const EeWonPatchField& f, const char* inBase)
{
    return f.whole ? std::string(inBase + f.at, f.len) : std::string();
}
inline void PatchThrowCut()
{
    throw ReadBufferException("Attempt to read past end of buffer.");
}
// the read's end: the reader moved to where the original's stopped, and the throw of a cut
inline void PatchEnd(const EeWonPatchField& f, ReadBuffer& ioIn)
{
    ioIn.SetPos(ioIn.pos() + static_cast<int>(f.at));
    if (f.slot != EE_WON_PATCH_END) PatchThrowCut();
}

// The pieces between commas (inFilter: none for an empty string)
inline std::vector<std::string> PatchSplit(const std::string& inText, bool inFilter)
{
    std::vector<EeWonPatchField> f;
    ee_won_patch_split(reinterpret_cast<const uint8_t*>(inText.data()), inText.size(), inFilter, PatchFieldSink, &f);
    std::vector<std::string> v;
    for (size_t i = 0; i < f.size(); i++) v.push_back(inText.substr(f[i].at, f[i].len));
    return v;
}

// VersionData::AddVersionType: each comma-separated piece pushed
template <class List> void PatchAddVersionType(List& ioList, const std::string& theVersionType)
{
    std::vector<std::string> v = PatchSplit(theVersionType, false);
    for (size_t i = 0; i < v.size(); i++) ioList.push_back(v[i]);
}

// VersionData::ReadFromBuffer
template <class V> void PatchReadVersion(V& v, ReadBuffer& ioIn)
{
    const char* b;
    std::vector<EeWonPatchField> f = PatchRead(0, ioIn, &b);
    for (size_t i = 0; i < f.size(); i++) {
        switch (f[i].slot) {
        case EE_WON_VERSION_CONFIG: v.SetConfigName(PatchString(f[i], b)); break;
        case EE_WON_VERSION_VERSION: v.SetVersion(PatchString(f[i], b)); break;
        case EE_WON_VERSION_TYPE: v.AddVersionType(PatchString(f[i], b)); break;
        case EE_WON_VERSION_DESCRIPTION: v.SetDescriptionURL(PatchString(f[i], b)); break;
        case EE_WON_VERSION_STATE: v.SetState(static_cast<VersionState>(f[i].num)); break;
        default: PatchEnd(f[i], ioIn);
        }
    }
}

// PatchData::ReadFromBuffer (inList: from a patch list, with the config and versions)
template <class P> void PatchReadPatch(P& p, ReadBuffer& ioIn, bool inList)
{
    const char* b;
    std::vector<EeWonPatchField> f = PatchRead(inList ? 2 : 1, ioIn, &b);
    for (size_t i = 0; i < f.size(); i++) {
        const EeWonPatchField& x = f[i];
        switch (x.slot) {
        case EE_WON_PATCH_CONFIG: p.SetConfigName(PatchString(x, b)); break;
        case EE_WON_PATCH_FROM: p.SetFromVersion(PatchString(x, b)); break;
        case EE_WON_PATCH_TO: p.SetToVersion(PatchString(x, b)); break;
        case EE_WON_PATCH_ACTIVE: p.SetIsActive(x.num != 0); break;
        case EE_WON_PATCH_DISPLAY: p.SetDisplayName(PatchString(x, b)); break;
        case EE_WON_PATCH_NET_ADDRESS: p.SetNetAddress(PatchString(x, b)); break;
        case EE_WON_PATCH_CHECKSUM: p.SetCheckSum(static_cast<unsigned long>(x.num)); break;
        case EE_WON_PATCH_SIZE: p.SetPatchSize(static_cast<unsigned long>(x.num)); break;
        case EE_WON_PATCH_HOST_NAME: p.SetHostName(PatchString(x, b)); break;
        case EE_WON_PATCH_HOST_URL: p.SetHostURL(PatchString(x, b)); break;
        case EE_WON_PATCH_HOST_TEXT: p.SetHostText(PatchString(x, b)); break;
        case EE_WON_PATCH_HOST_IMAGE: p.SetHostImageURL(PatchString(x, b)); break;
        case EE_WON_PATCH_MUST_VISIT: p.SetMustVisitHost(x.num != 0); break;
        default: PatchEnd(x, ioIn);
        }
    }
}

// VersionData::WriteToBuffer
template <class V> void PatchWriteVersion(const V& v, WriteBuffer& outBuffer, DWORD inFlags)
{
    std::vector<EeWonPatchBytes> aTypes;
    for (typename VersionTypeList::const_iterator i = v.GetVersionTypeList().begin();
         i != v.GetVersionTypeList().end(); ++i)
        aTypes.push_back(PatchBytes(*i));
    ee_won_version_write(PatchBytes(v.GetConfigName()), PatchBytes(v.GetVersion()), aTypes.empty() ? NULL : &aTypes[0],
                         aTypes.size(), PatchBytes(v.GetDescriptionURL()), static_cast<uint8_t>(v.GetState()),
                         inFlags, PatchAppendSink, &outBuffer);
}

// PatchData::WriteToBuffer
template <class P> void PatchWritePatch(const P& p, WriteBuffer& outBuffer, bool inUpdateOnly)
{
    EeWonPatchBytes s[9] = {PatchBytes(p.GetConfigName()), PatchBytes(p.GetFromVersion()), PatchBytes(p.GetToVersion()),
                            PatchBytes(p.GetDisplayName()), PatchBytes(p.GetNetAddress()), PatchBytes(p.GetHostName()),
                            PatchBytes(p.GetHostURL()), PatchBytes(p.GetHostText()),
                            PatchBytes(p.GetHostImageURL())};
    ee_won_patch_write(s, static_cast<int32_t>(p.GetCheckSum()), static_cast<int32_t>(p.GetPatchSize()),
                       p.GetMustVisitHost(), p.GetIsActive(), inUpdateOnly, PatchAppendSink, &outBuffer);
}

// CheckValidVersionOp's (inVersion set: the target version, its URL, the patches from inVersion) and
// GetPatchListOp's reply data: the patches, each pushed once read whole
template <class P, class List, class V>
void PatchReplyData(const ByteBuffer* inData, const V* inVersion, std::string* outTo, std::string* outURL, List& outList)
{
    std::vector<EeWonPatchField> f;
    const char* b = inData->data();
    ee_won_patch_read(inVersion != NULL ? 3 : 4, reinterpret_cast<const uint8_t*>(b), inData->length(), PatchFieldSink, &f);
    for (size_t i = 0; i < f.size(); i++) {
        const EeWonPatchField& x = f[i];
        if (x.slot == EE_WON_PATCH_VERSION_TO) *outTo = PatchString(x, b);
        else if (x.slot == EE_WON_PATCH_VERSION_URL) *outURL = PatchString(x, b);
        else if (x.slot == EE_WON_PATCH_RECORD) {
            SmartPtr<P> aPatch = new P;
            if (inVersion != NULL) {
                aPatch->SetFromVersion(inVersion->GetVersion());
                aPatch->SetToVersion(*outTo);
                aPatch->SetIsActive(true);
                aPatch->SetConfigName(inVersion->GetConfigName());
            }
            ReadBuffer aRecord(b + x.at, x.len);
            aPatch->ReadFromBuffer(aRecord, inVersion == NULL);
            outList.push_back(aPatch);
        } else if (x.slot != EE_WON_PATCH_END)
            PatchThrowCut();
    }
}

// A patch op's request data: the version check's (product, version record, patch list flag), else the strings
// and the byte
inline ByteBufferPtr PatchRequest(int inKind, const std::vector<std::string>& inStrings, unsigned char inByte)
{
    std::vector<EeWonPatchBytes> s;
    for (size_t i = 0; i < inStrings.size(); i++) s.push_back(PatchBytes(inStrings[i]));
    ByteBufferPtr aBuffer;
    ee_won_patch_request(inKind, s.empty() ? NULL : &s[0], s.size(), inByte, WonByteBufferSink, &aBuffer);
    return aBuffer;
}

} // namespace WONAPI
