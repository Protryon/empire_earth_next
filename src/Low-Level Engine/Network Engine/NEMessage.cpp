// C:\EEX\Low-Level Engine\Network Engine\NEMessage.cpp
// Unit flags (docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
// Message (de)serialization: a 4-byte header (reserved, type, for-slot, originating slot) then each message's
// fields in its own order. The 28 engine messages are one data class, NEMRecord (LLE Network Engine round 6): its
// layout, flags, response rules and labels are the Rust's (ee_netmsg ne_records.rs), its wire form the codec's
// (ee_netmsg engine.rs); the factory's builders by type are the Rust's table. LLE round 8: NEMRecord's bodies are
// Rust too (rust/crates/ee_game/src/lle/netrecord.rs: the record's filling, the members, the inputs, the export and
// import through the codec's slots); the members forward, and the shims below are the C++ calls those bodies make
// (one statement each). The bodies' history: git history before LLE round 8.
#include <windows.h>
#include <string.h>

#include "lle/LowLevelEngineCore.h"
#include "lle/UWideString.h"
#include "lle/UReferenceCounter.h"
#include "lle/NEMessage.h"
#include "rust/sim/netmsg.h"   // ee_netmsg: the engine messages' wire layout and records
#include "rust/game/lle.h"

static_assert(sizeof(NEMRecordValue) == sizeof(int64_t), "a sender's value is one long long");

// New (port): the header's members (reserved, type, target slot, originating slot) as the codec's first four slots
// (from mType: the four bytes are consecutive, NEMessage.h)

// Builds a message from received bytes with the builder registered for its type; NULL for a type with none
NEMessage* NEMessage::ConvertMemoryToMessage(unsigned char* inMemory)
{
    ESVERIFY_AT(inMemory != NULL, 31);
    return static_cast<NEMessage*>(ee_netmsg_build(inMemory));
}


// ---- NEMRecord: the engine messages (netrecord.rs) ----


// The shims netrecord.rs calls (RecordCpp)
extern "C" {
unsigned char ee_nemcpp_type(const NEMRecord* inRecord) { return inRecord->GetType(); }
void ee_nemcpp_set_type(NEMRecord* inRecord, unsigned char inType) { inRecord->SetType(inType); }
unsigned char** ee_nemcpp_record_slot(NEMRecord* inRecord) { return &inRecord->mRecord; }
unsigned char* ee_nemcpp_bytes_new(size_t inSize) { return new unsigned char[inSize]; }
void ee_nemcpp_bytes_delete(unsigned char* inBytes) { delete[] inBytes; }
void ee_nemcpp_header_slots(NEMRecord* inRecord, EeNetMsgSlot* outSlots) { inRecord->GetHeaderSlots(outSlots); }
void ee_nemcpp_string_set(NEMRecord* inRecord, uint32_t inOrdinal, const char* inText) { inRecord->mStrings[inOrdinal] = inText; }
const char* ee_nemcpp_string_chars(NEMRecord* inRecord, uint32_t inOrdinal) { return inRecord->mStrings[inOrdinal]; }
const UWideString* ee_nemcpp_string_at(const NEMRecord* inRecord, uint32_t inOrdinal) { return &inRecord->mStrings[inOrdinal]; }
void ee_nemcpp_string_import(NEMRecord* inRecord, uint32_t inOrdinal, unsigned char* inData) { inRecord->mStrings[inOrdinal].Import(inData); }
size_t ee_nemcpp_inputs_size(const NEMRecord* inRecord) { return inRecord->mPlayerInputs.size(); }
NEMessage* ee_nemcpp_inputs_at(const NEMRecord* inRecord, size_t inIndex) { return inRecord->mPlayerInputs[inIndex]; }
NEMessage* ee_nemcpp_inputs_back(const NEMRecord* inRecord) { return inRecord->mPlayerInputs.back(); }
void ee_nemcpp_inputs_pop(NEMRecord* inRecord) { inRecord->mPlayerInputs.pop_back(); }
void ee_nemcpp_inputs_push(NEMRecord* inRecord, NEMessage* inInput) { inRecord->mPlayerInputs.push_back(inInput); }
unsigned char ee_nemcpp_type_of(const NEMessage* inMessage) { return inMessage->GetType(); }
void ee_nemcpp_dereference(NEMessage* inMessage) { inMessage->Dereference(); }
void ee_nemcpp_export(NEMessage* inMessage, unsigned char** outData, unsigned long* outSize) { inMessage->ExportMessage(outData, *outSize); }
unsigned long ee_nemcpp_export_size(NEMessage* inMessage) { return inMessage->ExportSize(); }
NEMessage* ee_nemcpp_from_memory(unsigned char* inData) { return NEMessage::ConvertMemoryToMessage(inData); }
NEMRecord* ee_nemcpp_record_new(unsigned char* inData) { return new NEMRecord(inData); }
}
