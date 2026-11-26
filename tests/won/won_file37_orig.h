// FileReader and FileWriter as they were in include/ee/wonapi/WONLobbyPersistence.h until R6 WON lobby and network
// round 37 (shims over ee_won file.rs), for the drivers: no game code uses them any more. Include after
// ee/wonapi/WONLobbyPersistence.h.
#ifndef EE_TESTS_WON_FILE37_ORIG_H
#define EE_TESTS_WON_FILE37_ORIG_H
#include "ee/wonapi/WONLobbyPersistence.h"
namespace WONAPI {
class FileWriterException;
inline void FileWriterCheck(int inStatus);
// FileReader: 0x14 bytes (ConfigParser embeds one at 0x18; constructor 0x7b1d20). FileWriter: 4 bytes
// (constructor 0x6a1e10, destructor 0x6a1e20). Both are ee_won's (rust/crates/ee_won_net/src/file.rs): the state is
// its #[repr(C)] core, these methods forward and throw the exceptions its statuses name (FileReader.cpp,
// FileReaderWideStrings.cpp and FileWriter.cpp are deleted: net round 27).
class FileReader {
public:
    bool IsOpen() { return this->mCore.file!=NULL || this->mCore.buffer!=NULL; }
    FileReader() { ee_won_file_reader_init(&this->mCore); }
    FileReader(const char* inPath) { ee_won_file_reader_init(&this->mCore); this->Open(inPath); }
    FileReader(const void* inData, unsigned long inLength) { ee_won_file_reader_init(&this->mCore); this->Open(inData, inLength); }
    bool OpenText(const char* inPath) { return ee_won_file_reader_open_text(&this->mCore, inPath); } // fopen "r"
    void Open(const void* inData, unsigned long inLength) { ee_won_file_reader_open_memory(&this->mCore, inData, inLength); }
    unsigned long pos() { return ee_won_file_reader_pos(&this->mCore); }
    unsigned long length() // measures the file once by seeking to its end
    { int32_t s = 0; unsigned long n = ee_won_file_reader_length(&this->mCore, &s); FileReaderCheck(s); return n; }
    unsigned long Available() { return this->length()-this->pos(); }
    void SetPos(unsigned long inPos) { FileReaderCheck(ee_won_file_reader_set_pos(&this->mCore, inPos)); }
    int ReadMaxBytes(void* outData, int inMaxBytes)
    { int32_t s = 0; int n = ee_won_file_reader_read_max(&this->mCore, outData, inMaxBytes, &s); FileReaderCheck(s); return n; }
    int TryReadByte() { return ee_won_file_reader_try_read_byte(&this->mCore); } // -1 at the end
    void SkipBytes(unsigned long inLength) { FileReaderCheck(ee_won_file_reader_skip(&this->mCore, inLength)); }
    void ReadIntoMemory() { FileReaderCheck(ee_won_file_reader_read_into_memory(&this->mCore)); }
    int ReadLength(int inFieldSize) // 1, 2 or 4 bytes; 0 for any other size
    { int v = 0; FileReaderCheck(ee_won_file_reader_read_length(&this->mCore, inFieldSize, &v)); return v; }
    void ReadRawString(std::string& outString, int inNumChars)
    {
        outString.erase();
        outString.reserve(inNumChars);
        for (int i = 0; i < inNumChars; ++i) outString += (char)this->ReadByte();
    }
    void ReadString(std::string& outString, unsigned char inFieldSize) { this->ReadRawString(outString, this->ReadLength(inFieldSize)); }
    void ReadRawWString(std::wstring& outString, int inNumChars) // UTF-16 units, one by one
    {
        outString.erase();
        outString.reserve(inNumChars);
        for (int i = 0; i < inNumChars; i++) WideAppendUtf16Unit(outString, this->ReadShort());
    }
    void ReadWString(std::wstring& outString, unsigned char inFieldSize = 2) { this->ReadRawWString(outString, this->ReadLength(inFieldSize)); }
    void Rewind() { ee_won_file_reader_rewind(&this->mCore); }
    bool EndOfFile() { return ee_won_file_reader_end_of_file(&this->mCore); } // also when nothing is open
    ~FileReader() { ee_won_file_reader_close(&this->mCore); }
    bool Open(const char* inPath) { return ee_won_file_reader_open(&this->mCore, inPath); }
    void Close() { ee_won_file_reader_close(&this->mCore); }
    void ReadBytes(void* outData, unsigned long inLength) { FileReaderCheck(ee_won_file_reader_read_bytes(&this->mCore, outData, inLength)); }
    unsigned char ReadByte() { unsigned char v; this->ReadBytes(&v, 1); return v; }
    unsigned short ReadShort() { unsigned short v; this->ReadBytes(&v, 2); return v; }
    int32_t ReadLong() { int32_t v; this->ReadBytes(&v, 4); return v; } // 32-bit on every OS
    void Check(int inStatus) { FileReaderCheck(inStatus); } // throws FileReaderException for a failed status (the image decoders' Rust reads)
    // the file (0x00), the memory source (0x04; freed when owned), length (0x08), position (0x0c), owned (0x10)
    // and memory mode (0x11: set by the memory Open at 0x6a0dd6 and by length(), cleared by the file Open)
    EeWonFileReader mCore;
private:
    FileReader(const FileReader&);
    FileReader& operator=(const FileReader&);
};
class FileWriter {
public:
    FileWriter() { ee_won_file_writer_init(&this->mCore); }
    unsigned long pos() { return ee_won_file_writer_pos(&this->mCore); } // 0 with no file
    void SetPos(unsigned long inPos) { FileWriterCheck(ee_won_file_writer_set_pos(&this->mCore, inPos)); }
    void WriteLength(unsigned long inLength, unsigned char inFieldSize) // 1, 2 or 4 bytes; nothing for another size
    { FileWriterCheck(ee_won_file_writer_write_length(&this->mCore, inLength, inFieldSize)); }
    void WriteRawString(const std::string& inString) { this->WriteBytes(inString.c_str(), inString.length()); }
    void WriteString(const std::string& inString, unsigned char inFieldSize)
    { this->WriteLength(inString.length(), inFieldSize); this->WriteRawString(inString); }
    void WriteRawWString(const std::wstring& inString)
    { std::string b; WideAppendUtf16LE(b, inString); this->WriteBytes(b.data(), b.length()); }
    void WriteWString(const std::wstring& inString, unsigned char inFieldSize = 2)
    { this->WriteLength(WideUtf16Length(inString), inFieldSize); this->WriteRawWString(inString); }
    ~FileWriter() { this->Close(); }
    bool Open(const char* inPath, bool inOpenIfReadOnly) // retries after clearing the read-only flag when asked
    {
        if (ee_won_file_writer_open(&this->mCore, inPath)) return true;
        if (inPath[0] == 0 || !inOpenIfReadOnly) return false;
        WONFile(inPath).SetReadOnly(false);
        return ee_won_file_writer_open(&this->mCore, inPath);
    }
    void Close() { ee_won_file_writer_close(&this->mCore); }
    void WriteBytes(const void* inData, unsigned long inLength) { FileWriterCheck(ee_won_file_writer_write_bytes(&this->mCore, inData, inLength)); }
    void WriteByte(unsigned char inValue) { this->WriteBytes(&inValue, 1); }
    void WriteShort(unsigned short inValue) { this->WriteBytes(&inValue, 2); }
    void WriteLong(int32_t inValue) { this->WriteBytes(&inValue, 4); } // 32-bit on every OS
    EeWonFileWriter mCore; // the file (0x00)
private:
    FileWriter(const FileWriter&);
    FileWriter& operator=(const FileWriter&);
};

class FileWriterException : public std::exception {
public:
    FileWriterException(const std::string& inDescription) { this->mDescription = inDescription; }
    // the destructor is implicit (0x6a2210)
    std::string mDescription;       // 0x08 (thrown by 0x6a2400)
};
inline void FileWriterCheck(int inStatus)
{ if (inStatus != 0) throw FileWriterException(ee_won_file_status_text(inStatus)); }
}
#endif
