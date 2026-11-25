// New (game messages lane, round 6): the game messages' class (ee/EEMessage.h): EEMessage's codec over the slots
// and EEMRecord, the one data class every game message type is (its constructors, members, rules and codec slots;
// ee_netmsg records.rs), and the lobby's two whole-object senders. Split from EEMessage.cpp (its 0xb8cda.. range keeps
// the factory, the Execute host and the bodies) so that a program can link the messages without the world: the
// server's and client's golden (tests/net/ee_session_test.cpp).
#include <windows.h>
#include <string.h>
#include "lle/LowLevelEngine.h"
#include "lle/NEMessage.h"
#include "ee/EEGameOptions.h"
#include "ee/EEMessage.h"
#include "rust/sim/netmsg.h"

#include "rust/game/gamecpp.h"

// port (round 10, lane C): the codec's and the record's bodies are gamecpp mrecord.rs's (each message hands its
// members to the Rust as slots in wire order, the header's four first; the layout is ee_netmsg records.rs's)
static_assert(sizeof(EEMRecordValue) == sizeof(EeGamecppMrValue) && offsetof(EEMRecordValue, mFloats) == offsetof(EeGamecppMrValue, floats)
                  && offsetof(EEMRecordValue, mIsFloat) == offsetof(EeGamecppMrValue, is_float)
                  && offsetof(EEMRecordValue, mString) == offsetof(EeGamecppMrValue, string)
                  && offsetof(EEMRecordValue, mListElement) == offsetof(EeGamecppMrValue, list_element),
              "gamecpp mrecord.rs EeGamecppMrValue");
static_assert(sizeof(EEMRecord::mRecord) == 64 && sizeof(EEMessage::mFastSelectionID) == 1, "gamecpp mrecord.rs");

// New (game messages lane, round 1): the codec's side of every game message, once. The message gives its codec
// class and its members as slots in wire order (GetCodecSlots); these run the codec over them, as each class's own
// ExportMessage, ExportSize and ImportMessage did.
void EEMessage::ExportMessage(unsigned char** outPayloadPointer, unsigned long& outMessageSize)
{
    ee_gamecpp_msg_export(static_cast<NEMessage*>(this), outPayloadPointer, &outMessageSize);
}

void EEMessage::ImportMessage(unsigned char* inPayloadPointer)
{
    ee_gamecpp_msg_import(static_cast<NEMessage*>(this), inPayloadPointer);
}

// ---- EEMRecord (game messages lane, round 3): the record types' messages (ee/EEMessage.h; ee_netmsg records.rs) ----
EEMRecordValue::EEMRecordValue(const GE2DPoint& inPoint) : mInteger(0), mIsFloat(true)
{
    this->mFloats[0] = inPoint.GetX();
    this->mFloats[1] = inPoint.GetY();
}

EEMRecordValue::EEMRecordValue(const GEVector& inVector) : mInteger(0), mIsFloat(true)
{
    this->mFloats[0] = inVector.GetX();
    this->mFloats[1] = inVector.GetY();
    this->mFloats[2] = inVector.GetZ();
}

// a sender's: the members in wire order, then the ones off the wire (fewer leave the rest 0)
EEMRecord::EEMRecord(unsigned char inType, std::initializer_list<EEMRecordValue> inValues)
    : EEMessage(inType, (ee_netmsg_record_flags(inType) & EE_NETMSG_RECORD_WHEN_OUT) != 0)
{
    ee_gamecpp_mrecord_init(static_cast<NEMessage*>(this), reinterpret_cast<const EeGamecppMrValue*>(inValues.begin()), inValues.size());
}

// the codec over the slots, then the string member's text from where the codec found it (UWideString::Import), the
// list's records from where it found them, the blob's bytes as many as it found
void EEMRecord::ImportMessage(unsigned char* inPayloadPointer)
{
    ee_gamecpp_mrecord_import(static_cast<NEMessage*>(this), inPayloadPointer, false);
}

// The lobby's game options, whole (the server sends gGameOptions); the members in wire order
EEMRecord* EENewFullGameOptions(const EEGameOptions& inOptions)
{
    return new EEMRecord(kMessageFullGameOptions,
                         {inOptions.GetAllowCustomCivs(), inOptions.GetB407(), inOptions.GetCheatCodes(),
                          inOptions.GetDifficulty(), inOptions.GetEpochEnd(), inOptions.GetGameSpeed(),
                          inOptions.GetLoadType(), inOptions.GetVariant(), inOptions.GetLockSpeed(),
                          inOptions.GetLockTeams(), inOptions.GetMapSize(), inOptions.GetRecordGames(),
                          inOptions.GetResources(), inOptions.GetRevealMap(), inOptions.GetEpochStart(),
                          inOptions.GetUnitLimit(), inOptions.GetVictory(), inOptions.GetWondersForVictory(),
                          inOptions.GetMapType()});
}

// One lobby slot, whole (the server sends it to the players); the members in wire order
EEMRecord* EENewFullPlayerInformation(const EEGameOptionsPlayer& inPlayer)
{
    return new EEMRecord(kMessageFullPlayerInformation,
                         {inPlayer.mSlot, inPlayer.mType, inPlayer.mLoadedType, inPlayer.mName, inPlayer.mPing,
                          inPlayer.mColor, inPlayer.mTeam, inPlayer.mStartingCitizens, inPlayer.mIsReady,
                          inPlayer.mIsLocal, inPlayer.mB36, (int32_t)inPlayer.mEngine});   // the engine: port
}


// ------------------------------------------------------------------------- round 11, lane C: Communications, Calamities, Database, Tech Tree, Persistent


// the C++ calls and members of lane C's bodies in Rust (gamecpp session_objs.rs, ...; EeGameCpp's round 11 lane C
// section) are the cases of a chain of switches beside the members they read: EEServer.cpp (C2) -> EEClient.cpp (C3)
// -> EEMessage.cpp (C4) -> lane E


