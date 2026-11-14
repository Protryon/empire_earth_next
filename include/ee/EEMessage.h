// C:\EEX\Empire Earth\Communications\EEMessage.h (RECONSTRUCTION; file name inferred). Base of the game's network
// messages (RTTI EEMessage > NEMessage > UReferenceCounter; vtable 0x438ae8; EEMessage.cpp around 0xb8f8e). sizeof 0x10.
// Requires lle/NEMessage.h.
#ifndef EE_EEMESSAGE_H
#define EE_EEMESSAGE_H
#include "ee/EECivUpgrade.h"

#include <stdint.h>   // int32_t/uint32_t: the wire fields' fixed widths
#include <stddef.h>   // size_t: the codec slots' count
#include <string.h>   // memcpy: EEMRecord's members
#include <initializer_list>
#include <type_traits>

#include "lle/U2DPoint.h"                  // EEMSendFlare's inline ctor
#include "ee/EEMessageID.h"

class EEGameOptions;
class GE2DPoint;
class GEVector;
struct EeNetMsgSlot;                       // rust/ee_rust.h (the game messages' codec)
class EEGameOptionsPlayer;

extern bool gLocalPlayerOut;                 // 0x4e6099 (provisional name)

class EEMessage : public NEMessage {
public:
    // inline: EEMSelectPlayerChatType's ctor (0x1d2b36) expands it; 0x32a6 is the out-of-line copy where VC6 refused it
    EEMessage(unsigned char inType = 0xff, bool inFlag = false) : NEMessage(inType), mCanSendWhenOut(inFlag) {}

    virtual ~EEMessage() {}   // user-declared: every ??_G stores EEMessage's vptr (0x26fbf7)
    // inline override of NEMessage's +0x04 (the body 0x1891 is in every EEM* vtable that does not override it and
    // sits in EEABuild.cpp, its first user in link order)
    virtual bool GetIsAvailableToSend() const { return !gLocalPlayerOut || this->mCanSendWhenOut; }

    static NEMessage* BuildEEMessage(unsigned char* inMemory);   // the factory 0xb8f8e (sConstructors[0x41..0xfd])
    static void RegisterKnownMessages();                         // 0xb8cda
    // the physics loop's two ways to execute a message (0x1537ba/0x1537e3; provisional names): types 0x82,
    // 0x84, 0x96 record their selection id (mU0C) per player, the rest execute only when it is the current one
    void ExecuteSelectionMessage();                              // 0xb8cf7
    void ExecuteMessage();                                       // 0xb8e86

    // New (game messages lane, round 1): the codec's side of every game message in one place (EEMessage.cpp): a
    // class gives its codec class (ee_netmsg) and its members as slots in wire order; ExportMessage, ExportSize and
    // ImportMessage run the codec over them (a class whose members the slots cannot carry overrides them); a game
    // message answers no request (IsResponse; EEMLoadOrSaveGame's overrides it).
    virtual size_t GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass);
    size_t CodecSlots(EeNetMsgSlot* outSlots)
    {
        uint32_t theClass;
        return this->GetCodecSlots(outSlots, theClass);
    }
    virtual void ExportMessage(unsigned char** outData, unsigned long& outSize);
    virtual unsigned long ExportSize();
    virtual void ImportMessage(unsigned char* inData);
    virtual bool IsResponse(NEMessage* inMessage) { return false; }

    unsigned char mFastSelectionID;         // +0x0c (provisional name; not set by the ctor; EEMSelectPlayerChatType 0x1d2b36 stores +0x0d)
    bool mCanSendWhenOut;                   // +0x0d
};

// New (port): dynamic_cast<EEMessage*>(inMessage) as the message kind of its type (ee_netmsg: a message is an
// EEMessage exactly when its type is a game type, 0x41..0xfd); NULL stays NULL
extern "C" uint8_t ee_netmsg_message_kind(uint8_t message_type);   // rust/ee_rust.h
inline EEMessage* EEGameMessageCast(NEMessage* inMessage)
{
    return inMessage != NULL && ee_netmsg_message_kind(inMessage->GetType()) == 2 /* EE_NETMSG_KIND_GAME */
               ? static_cast<EEMessage*>(inMessage)
               : NULL;
}

// New (game messages lane, round 3): a sender's value for one member of an EEMRecord, converted as the old classes'
// constructors converted it to the member's type (an integer truncated, a bool as != 0, a float, a point, a vector)
struct EEMRecordValue {
    template <class T, typename std::enable_if<std::is_integral<T>::value || std::is_enum<T>::value, int>::type = 0>
    EEMRecordValue(T inValue) : mInteger((long long)inValue), mIsFloat(false) {}
    EEMRecordValue(double inValue) : mInteger(0), mIsFloat(true) { this->mFloats[0] = (float)inValue; }
    EEMRecordValue(const GE2DPoint& inPoint);
    EEMRecordValue(const GEVector& inVector);

    long long mInteger;
    float mFloats[3];
    bool mIsFloat;
};

// New (game messages lane, round 3): the game messages whose members only their senders' constructors, their own
// Execute and the capture touched (54 types: the census in PORT_LEARNINGS "R5b.4 game messages, round 3"). One class
// carries any of them: its codec class's members after the selection id, packed in wire order in mRecord, the layout,
// flags and label the Rust's (ee_netmsg records.rs). A sender passes the members in wire order:
// new EEMRecord(kMessageCheatCode, {theCode, theParameter}); Execute runs the type's body (EEMessage.cpp).
class EEMRecord : public EEMessage {
public:
    EEMRecord(unsigned char* inData);   // the factory's: the type at inData[1], then the import
    EEMRecord(unsigned char inType, std::initializer_list<EEMRecordValue> inValues = {});
    virtual bool GetCanSendWhilePaused() const;
    virtual size_t GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass);
    virtual void Execute();
    virtual char* GetMessageLabel() const;

    // member inIndex (1 = the first after the selection id) as its type (the size is verified)
    template <class T> T Get(uint32_t inIndex) const
    {
        T theValue;
        memcpy((void*)&theValue, this->Field(inIndex, sizeof(T)), sizeof(T));
        return theValue;
    }
    template <class T> void Set(uint32_t inIndex, T inValue) { memcpy(this->Field(inIndex, sizeof(T)), (const void*)&inValue, sizeof(T)); }
    unsigned char* Field(uint32_t inIndex, size_t inSize) const;

    unsigned char mRecord[32];   // EE_NETMSG_RECORD_BYTES
};

// The local player changes the formation of the selected group (vtable 0x43de14). sizeof 0x18.
class EEMChangeFormation : public EEMessage {
public:
    // the received message does not carry the formation flag (ImportMessage reads only the selection and type):
    // start it cleared instead of leaving it uninitialised
    EEMChangeFormation(unsigned char* inData) : mB14(false) { this->ImportMessage(inData); }
    EEMChangeFormation(long inFormationType) : EEMessage(kMessageChangeFormation, false), mFormationType(inFormationType)
    {
        this->mB14 = true;
    }
    virtual size_t GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass);
    virtual void Execute();
    virtual char* GetMessageLabel() const;

    int32_t mFormationType;                 // +0x10
    bool mB14;                              // +0x14
};

// A chat line (type 0x7d, vtable 0x43e0bc). sizeof 0x24.
class EEMInGameChat : public EEMessage {
public:
    EEMInGameChat(unsigned char* inData) { this->ImportMessage(inData); }
    virtual ~EEMInGameChat() {}
    EEMInGameChat(const UWideString& inText) : EEMessage(kMessageInGameChat, true) { this->mText = inText; }  // 0x1d3196
    virtual bool GetCanSendWhilePaused() const { return true; }
    virtual size_t GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass);
    virtual void ImportMessage(unsigned char* inData);
    virtual void Execute();
    virtual char* GetMessageLabel() const;

    UWideString mText;                      // +0x10
};

// ---- the hot-key events (0x1e93df..0x1e9f40) ----

// The local player adds one unit to the selection (type 0x49, vtable 0x438e44; out-of-line ctor 0x32d2). sizeof 0x14.
class EEMAddUnitToSelectedGroup : public EEMessage {
public:
    EEMAddUnitToSelectedGroup(unsigned char* inData) { this->ImportMessage(inData); }
    EEMAddUnitToSelectedGroup(unsigned long inUnitID) : EEMessage(kMessageAddUnitToSelectedGroup, true) { this->mUnitID = inUnitID; }
    virtual bool GetCanSendWhilePaused() const { return true; }   // datacmp: vtable slot 2 = 0x418aa9 (mov al, 1)
    virtual size_t GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass);
    virtual void Execute();
    virtual char* GetMessageLabel() const;

    uint32_t mUnitID;                       // +0x10
};

// The same without the toggle (type 0x76, vtable 0x438e10; the hot keys with shift held)
class EEMAlwaysAddUnitToSelectedGroup : public EEMAddUnitToSelectedGroup {
public:
    EEMAlwaysAddUnitToSelectedGroup(unsigned char* inData) : EEMAddUnitToSelectedGroup(inData) {}
    EEMAlwaysAddUnitToSelectedGroup(unsigned long inUnitID) : EEMAddUnitToSelectedGroup(inUnitID) { this->mType = kMessageAlwaysAddUnitToSelectedGroup; }
    virtual void Execute();
    virtual char* GetMessageLabel() const;
};

// ---- every message of EEMessage.cpp (0xb8cda..0xca90f), generated from the Export/Import bodies;
// field names are provisional (m<type><offset>) unless proven ----

// vtable 0x438cf0; sizeof 0x138; ctor 0xc5d7f (EEMessage.cpp), import ctor 0xba41b; ??_G 0x2a02 (first game unit)
// The selection travels compressed: mBuffer holds mBufferSize bytes (file-static helpers 0xc5e67 / 0xc5fd1).
class EEMSelectUnits : public EEMessage {
public:
    EEMSelectUnits(const GERectangle& inRectangle, long inUnitType, unsigned char inType);
    EEMSelectUnits(unsigned char* inData) { this->ImportMessage(inData); }
    virtual ~EEMSelectUnits() {}
    virtual bool GetCanSendWhilePaused() const { return true; }
    virtual size_t GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass);
    virtual void ImportMessage(unsigned char* inData);
    virtual void Execute();
    virtual char* GetMessageLabel() const;

    long GetUnitIDsAt(unsigned long inIndex) const { return this->mUnitIDs[inIndex]; }
    std::vector<long> mUnitIDs;             // +0x10
    GERectangle mRectangle;                 // +0x20
    int32_t mL30;                           // +0x30
    unsigned char mBuffer[0x100];           // +0x34
    unsigned char mBufferSize;              // +0x134
};

// vtable 0x438cbc; sizeof 0x138
class EEMAddAndRemoveUnitsFromSelectedGroup : public EEMSelectUnits {
public:
    EEMAddAndRemoveUnitsFromSelectedGroup(unsigned char* inData) : EEMSelectUnits(inData) {}
    // inline: EEALasso::SelectUnits 0x28e5 builds it (the base ctor 0xc5d7f with type 0x50, then the vptr)
    EEMAddAndRemoveUnitsFromSelectedGroup(const GERectangle& inRectangle, long inUnitType, unsigned char inType)
        : EEMSelectUnits(inRectangle, inUnitType, inType) {}
    virtual void Execute();
    virtual char* GetMessageLabel() const;
};

// vtable 0x43df18; sizeof 0x18
class EEMAddPatrolPoint : public EEMessage {
public:
    EEMAddPatrolPoint(unsigned char* inData) { this->ImportMessage(inData); }
    EEMAddPatrolPoint(const GE2DPoint& inPoint) : EEMessage(kMessageAddPatrolPoint, false) { this->mPoint = inPoint; }   // the next AI's ai.patrol
    virtual size_t GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass);
    virtual void Execute();
    virtual char* GetMessageLabel() const;

    GE2DPoint mPoint;                       // +0x10
};

// vtable 0x438c88; sizeof 0x138
class EEMAddUnitsToSelectedGroup : public EEMSelectUnits {
public:
    EEMAddUnitsToSelectedGroup(unsigned char* inData) : EEMSelectUnits(inData) {}
    // inline: EEALasso::SelectUnits 0x28e5 builds it (the base ctor 0xc5d7f with type 0x4f, then the vptr)
    EEMAddUnitsToSelectedGroup(const GERectangle& inRectangle, long inUnitType, unsigned char inType)
        : EEMSelectUnits(inRectangle, inUnitType, inType) {}
    virtual void Execute();
    virtual char* GetMessageLabel() const;
};

// vtable 0x43deb0; sizeof 0x18
class EEMAttackGround : public EEMessage {
public:
    EEMAttackGround(unsigned char* inData) { this->ImportMessage(inData); }
    // inline: EEModeAttackGround expands it (type 0x9f)
    EEMAttackGround(float inX, float inY) : EEMessage(kMessageAttackGround, false)
    {
        this->mX = inX;
        this->mY = inY;
    }
    virtual size_t GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass);
    virtual void Execute();
    virtual char* GetMessageLabel() const { return "Attack Ground"; }

    float mX;                               // +0x10 (the target's x)
    float mY;                               // +0x14 (the target's y)
};

// vtable 0x438f18; sizeof 0x18
class EEMAttackUnit : public EEMessage {
public:
    EEMAttackUnit(unsigned char* inData) { this->ImportMessage(inData); }
    // inline: EEModeConvert expands it (type 0x4c; mB14 follows the vptr store)
    EEMAttackUnit(long inTarget, bool inIsQueued, bool inIsConvert) : EEMessage(kMessageAttackUnit, false), mTargetID(inTarget), mIsConvert(inIsConvert)
    {
        this->mIsQueued = inIsQueued;
    }
    virtual size_t GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass);
    virtual void Execute();
    virtual char* GetMessageLabel() const;

    int32_t mTargetID;                      // +0x10
    bool mIsQueued;                         // +0x14 (EEActiveGroup::SetGoal's flag)
    bool mIsConvert;                        // +0x15 (EEUGAttackUnit's last argument)
};

// vtable 0x438b1c; sizeof 0x24
class EEMBuildGate : public EEMessage {
public:
    EEMBuildGate(unsigned char* inData) { this->ImportMessage(inData); }
    // inline: COMDAT 0x194a after its first user EEABuild::BuildGate 0x18dd (type 0x86; the members after the vptr)
    EEMBuildGate(const GE2DPoint& inLocation, long inTechnology, long inSide, long inPlayerID) : EEMessage(kMessageBuildGate, false)
    {
        this->mPoint = inLocation;
        this->mTechnology = inTechnology;
        this->mSide = inSide;
        this->mPlayerID = inPlayerID;
    }
    virtual size_t GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass);
    virtual void Execute();
    virtual char* GetMessageLabel() const;

    GE2DPoint mPoint;                       // +0x10
    int32_t mTechnology;                    // +0x18
    int32_t mSide;                          // +0x1c
    int32_t mPlayerID;                      // +0x20
};

// vtable 0x438ab4; sizeof 0x20
class EEMBuildUnit : public EEMessage {
public:
    EEMBuildUnit(unsigned char* inData) { this->ImportMessage(inData); }
    // inline: COMDAT 0x1844 after its first user EEABuild::Build 0x1760 (type 0x56; the members after the vptr)
    EEMBuildUnit(const GE2DPoint& inLocation, long inTechnology, unsigned char inIsQueued) : EEMessage(kMessageBuildUnit, false)
    {
        this->mPoint = inLocation;
        this->mTechnology = inTechnology;
        this->mIsQueued = inIsQueued;
    }
    virtual size_t GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass);
    virtual void Execute();
    virtual char* GetMessageLabel() const;

    GE2DPoint mPoint;                       // +0x10
    int32_t mTechnology;                    // +0x18
    unsigned char mIsQueued;                // +0x1c
};

// vtable 0x438b50; sizeof 0x28
class EEMBuildWall : public EEMessage {
public:
    EEMBuildWall(unsigned char* inData) { this->ImportMessage(inData); }
    // inline: COMDAT 0x1a0b after its first user EEABuild::BuildWall 0x199e (type 0x59; the members after the vptr)
    EEMBuildWall(const GE2DPoint& inStart, const GE2DPoint& inEnd, long inTechnology, long inPlayerID)
        : EEMessage(kMessageBuildWall, false)
    {
        this->mStart = inStart;
        this->mEnd = inEnd;
        this->mTechnology = inTechnology;
        this->mPlayerID = inPlayerID;
    }
    virtual size_t GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass);
    virtual void Execute();
    virtual char* GetMessageLabel() const;

    GE2DPoint mStart;                       // +0x10
    GE2DPoint mEnd;                         // +0x18
    int32_t mTechnology;                    // +0x20
    int32_t mPlayerID;                      // +0x24
};

// vtable 0x438b98; sizeof 0x24
class EEMCastCalamity : public EEMessage {
public:
    EEMCastCalamity(unsigned char* inData) { this->ImportMessage(inData); }
    // inline: COMDAT 0x1bce after its first user EEACalamity::Cast 0x1ab2 (type 0x75; the first four members in the
    // init list (stored before the vptr), the last in the body)
    EEMCastCalamity(long inCasterID, long inTargetID, long inTileX, long inTileY, long inTechnology)
        : EEMessage(kMessageCastCalamity, false), mCasterID(inCasterID), mTargetID(inTargetID), mTileX(inTileX), mTileY(inTileY)
    {
        this->mTechnology = inTechnology;
    }
    virtual size_t GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass);
    virtual void Execute();
    virtual char* GetMessageLabel() const { return "Cast Calamity"; }

    int32_t mCasterID;                      // +0x10
    int32_t mTargetID;                      // +0x14
    int32_t mTileX;                         // +0x18
    int32_t mTileY;                         // +0x1c
    int32_t mTechnology;                    // +0x20
};

// vtable 0x43df80; sizeof 0x28
class EEMChat : public EEMessage {
public:
    EEMChat(unsigned char* inData) { this->ImportMessage(inData); }
    // inline; out-of-line copy 0x1da661 after its first user EEUIELEChat::ProcessEvent
    EEMChat(const unsigned char& inPlayer, const UWideString& inText) : EEMessage(kMessageChat, false), mPlayerID(inPlayer)
    {
        this->mText = inText;
    }
    virtual ~EEMChat() {}
    virtual size_t GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass);
    virtual void ImportMessage(unsigned char* inData);
    virtual void Execute();
    virtual char* GetMessageLabel() const;

    UWideString mText;                      // +0x10
    unsigned char mPlayerID;                // +0x24
};

// vtable 0x43ddac; sizeof 0x14
class EEMCivTimerExpired : public EEMessage {
public:
    EEMCivTimerExpired(unsigned char* inData) { this->ImportMessage(inData); }
    EEMCivTimerExpired(unsigned char inIsWarning);  // 0x153dd2 (EEPhysics.cpp, after its first user)
    virtual size_t GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass);
    virtual void Execute();
    virtual char* GetMessageLabel() const;

    unsigned char mIsWarning;               // +0x10
};

// vtable 0x43dfb4; sizeof 0x3c; default ctor 0xba684, dtor 0xba6fd, ??_G 0xba6e1 (all inline, emitted here)
class EEMCivUpgrades : public EEMessage {
public:
    EEMCivUpgrades() { this->mPlayerID = 0; }
    // inline, out of line after its first user 0x1e6635 (EEUIEPDCSOk; also 0x27ca6f, 0x280abb): mB0D = the type is
    // 0x7f (Purchased); argument names are provisional
    EEMCivUpgrades(unsigned char inType, const unsigned char& inPlayer, const std::vector<EECivUpgrade>& inUpgrades,
                   const UWideString& inName, long inNameString)
        : EEMessage(inType, inType == kMessageCivUpgradesPurchased), mCivName(inName), mUpgrades(inUpgrades), mNameString(inNameString), mPlayerID(inPlayer) {}
    // (implicit dtor: 0xba6fd does not store EEMCivUpgrades' vptr)
    virtual bool GetCanSendWhilePaused() const { return true; }
    virtual size_t GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass);
    virtual void ImportMessage(unsigned char* inData);
    virtual void SetName(const UWideString& inName) { this->mCivName = inName; }  // +0x30 (0xba6d1) (provisional name)

    UWideString mCivName;                   // +0x10
    std::vector<EECivUpgrade> mUpgrades;  // +0x24
    int32_t mNameString;                    // +0x34
    unsigned char mPlayerID;                // +0x38
};

// vtable 0x43dd74; sizeof 0x3c; ??_G 0xba727
class EEMCivUpgradesPurchased : public EEMCivUpgrades {
public:
    EEMCivUpgradesPurchased(unsigned char* inData) { this->ImportMessage(inData); }
    // EECBRequestUpgrades 0x280ab9: EEMCivUpgrades' ctor 0x1e6635 with kind 0x7f, then the Purchased vptr
    EEMCivUpgradesPurchased(const unsigned char& inPlayer, const std::vector<EECivUpgrade>& inUpgrades,
                            const UWideString& inName, long inNameString)
        : EEMCivUpgrades(kMessageCivUpgradesPurchased, inPlayer, inUpgrades, inName, inNameString) {}
    virtual ~EEMCivUpgradesPurchased() {}
    virtual void Execute();
    virtual char* GetMessageLabel() const;
};

// vtable 0x43dd3c; sizeof 0x3c; ??_G 0xba749
class EEMCivUpgradesRequested : public EEMCivUpgrades {
public:
    EEMCivUpgradesRequested(unsigned char* inData) { this->ImportMessage(inData); }
    // expanded by EEUIEPDCSOk 0x1e64ed and 0x27ca2b around the out-of-line base ctor
    EEMCivUpgradesRequested(const unsigned char& inPlayer, const std::vector<EECivUpgrade>& inUpgrades,
                            const UWideString& inName, long inNameString)
        : EEMCivUpgrades(kMessageCivUpgradesRequested, inPlayer, inUpgrades, inName, inNameString) {}
    virtual ~EEMCivUpgradesRequested() {}
    virtual void Execute();
    virtual char* GetMessageLabel() const;
};

// vtable 0x438fe8; sizeof 0x14
class EEMDropoffResource : public EEMessage {
public:
    EEMDropoffResource(long inDropoffID) : EEMessage(kMessageDropoffResource, false) { this->mDropoffID = inDropoffID; }  // 0x4041cf (inline, first user EEAMouseClick::Execute)
    EEMDropoffResource(unsigned char* inData) { this->ImportMessage(inData); }
    virtual size_t GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass);
    virtual void Execute();
    virtual char* GetMessageLabel() const;

    int32_t mDropoffID;                     // +0x10
};

// vtable 0x43dca0; sizeof 0x14
class EEMEdictProposal : public EEMessage {
public:
    EEMEdictProposal(unsigned char* inData) { this->ImportMessage(inData); }
    virtual size_t GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass);
    virtual void Execute();
    virtual char* GetMessageLabel() const { return "Edict Proposal"; }

    // inline (EEServer::FilterPlayerInput pushes its value without widening it; provisional name)
    unsigned short GetEdict() const { return this->mEdict; }
    unsigned short mEdict;                  // +0x10
};

// vtable 0x43dc6c; sizeof 0x14
class EEMEdictVote : public EEMessage {
public:
    EEMEdictVote(unsigned char* inData) { this->ImportMessage(inData); }
    virtual size_t GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass);
    virtual void Execute();
    virtual char* GetMessageLabel() const { return "Edict Vote"; }

    unsigned short mEdict;                  // +0x10
    bool mIsYes;                            // +0x12 (yes; EEServer::CastVote takes it as is)
    unsigned char mCount;                   // +0x13
};

// vtable 0x44ae34; sizeof 0x14
class EEMEndCinematic : public EEMessage {
public:
    EEMEndCinematic(unsigned char* inData) { this->ImportMessage(inData); }
    // inline: COMDAT 0x26fbca after its first user EEUIEndCinematicModeEvent::ProcessEvent (type 0x9d; the original
    // pushes a float with fstp and the COMDAT copies the dword)
    EEMEndCinematic(float inTime) : EEMessage(kMessageEndCinematic, false), mF10(inTime) {}
    virtual size_t GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass);
    virtual void Execute();
    virtual char* GetMessageLabel() const;

    float mF10;                             // +0x10 (the cinematic's end time)
};

// vtable 0x43dfec; sizeof 0x1c
class EEMExchangeResources : public EEMessage {
public:
    EEMExchangeResources(unsigned char* inData) { this->ImportMessage(inData); }
    // inline: COMDAT 0x26f94b after its first user EEUIFMGStatusButton::Command (type 0xae)
    EEMExchangeResources(unsigned char inIsSell, long inResourceType, unsigned char inFiveTimes)
        : EEMessage(kMessageExchangeResources, false), mIsSell(inIsSell), mResourceType(inResourceType), mFiveTimes(inFiveTimes) {}
    virtual size_t GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass);
    virtual void Execute();
    virtual char* GetMessageLabel() const;

    unsigned char mIsSell;                  // +0x10
    int32_t mResourceType;                  // +0x14
    unsigned char mFiveTimes;               // +0x18
};

// vtable 0x43db9c; sizeof 0x14
class EEMExitShiftMode : public EEMessage {
public:
    EEMExitShiftMode(unsigned char* inData) { this->ImportMessage(inData); }
    EEMExitShiftMode(long inGameTime);      // 0x153ff1 (EEPhysics.cpp, after its first user)
    virtual size_t GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass);
    virtual void Execute();
    virtual char* GetMessageLabel() const;

    int32_t mGameTime;                      // +0x10
};

// vtable 0x43e054; sizeof 0x54
class EEMFullGameOptions : public EEMessage {
public:
    EEMFullGameOptions(unsigned char* inData) { this->ImportMessage(inData); }
    EEMFullGameOptions(const EEGameOptions& inOptions);   // 0xbe6c6 (the lobby sends gGameOptions)
    virtual size_t GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass);
    virtual void ImportMessage(unsigned char* inData);
    virtual void Execute();
    virtual char* GetMessageLabel() const;

    UWideString mMapType;                   // +0x10
    int32_t mGameSpeed;                     // +0x24
    int32_t mLoadType;                      // +0x28
    int32_t mMapSize;                       // +0x2c
    int32_t mEpochStart;                    // +0x30
    int32_t mEpochEnd;                      // +0x34
    int32_t mVariant;                       // +0x38
    int32_t mResources;                     // +0x3c
    int32_t mUnitLimit;                     // +0x40
    int32_t mWondersForVictory;             // +0x44
    int32_t mDifficulty;                    // +0x48
    bool mVictory;                          // +0x4c
    bool mLockTeams;                        // +0x4d
    bool mLockSpeed;                        // +0x4e
    bool mRevealMap;                        // +0x4f
    bool mCheatCodes;                       // +0x50
    bool mRecordGames;                      // +0x51
    bool mAllowCustomCivs;                  // +0x52
    bool mB53;                              // +0x53
};

// vtable 0x43e124; sizeof 0x44
class EEMFullPlayerInformation : public EEMessage {
public:
    EEMFullPlayerInformation(unsigned char* inData) { this->ImportMessage(inData); }
    EEMFullPlayerInformation(const EEGameOptionsPlayer& inPlayer);   // 0xc378e (the server sends a lobby slot)
    virtual bool GetCanSendWhilePaused() const { return true; }
    virtual size_t GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass);
    virtual void ImportMessage(unsigned char* inData);
    virtual void Execute();
    virtual char* GetMessageLabel() const;

    UWideString mName;                      // +0x10
    int32_t mSlotType;                      // +0x24
    int32_t mLoadedType;                    // +0x28
    float mPing;                            // +0x2c (the ping)
    int32_t mSlot;                          // +0x30
    int32_t mColor;                         // +0x34
    int32_t mTeam;                          // +0x38
    int32_t mCitizenBonus;                  // +0x3c
    bool mIsLocal;                          // +0x40
    bool mIsReady;                          // +0x41
    bool mB42;                              // +0x42
    int32_t mEngine;                        // port: EEGameOptionsPlayer::mEngine (ee/EECPEngine.h; 4 more payload bytes)
};

// vtable 0x43e020; sizeof 0x2c
class EEMGameOptions : public EEMessage {
public:
    EEMGameOptions(unsigned char* inData) { this->ImportMessage(inData); }
    EEMGameOptions(long inOption, long inValue, const UWideString& inS14);  // 0xbebe1
    virtual size_t GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass);
    virtual void ImportMessage(unsigned char* inData);
    virtual void Execute();
    virtual char* GetMessageLabel() const;

    int32_t mOption;                        // +0x10
    UWideString mMapType;                   // +0x14
    uint32_t mValue;                        // +0x28 the value (unsigned: the flags are mL28 > 0 with seta)
};

// vtable 0x438fb4; sizeof 0x18
class EEMGatherResource : public EEMessage {
public:
    EEMGatherResource(long inTargetID, bool inIsQueued) : EEMessage(kMessageGatherResource, false), mTargetID(inTargetID), mIsQueued(inIsQueued) {}  // inline (EEAMouseClick::Execute 0x403907)
    EEMGatherResource(unsigned char* inData) { this->ImportMessage(inData); }
    virtual size_t GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass);
    virtual void Execute();
    virtual char* GetMessageLabel() const;

    int32_t mTargetID;                      // +0x10
    bool mIsQueued;                         // +0x14 (EEActiveGroup::SetGoal's flag)
};

// vtable 0x43e088; sizeof 0x20
class EEMHackANewUnit : public EEMessage {
public:
    // inline (out-of-line COMDAT 0x262c24 after its first user, the main game form's unit hack)
    EEMHackANewUnit(const long& inType, const GE2DPoint& inTile, unsigned char inPlayer)
        : EEMessage(kMessageHackANewUnit), mUnitType(inType), mPoint(inTile), mPlayerID(inPlayer) {}
    EEMHackANewUnit(unsigned char* inData) { this->ImportMessage(inData); }
    virtual size_t GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass);
    virtual void Execute();
    virtual char* GetMessageLabel() const { return "Hack A New Unit"; }

    int32_t mUnitType;                      // +0x10
    GE2DPoint mPoint;                       // +0x14
    unsigned char mPlayerID;                // +0x1c
};

// vtable 0x43901c; sizeof 0x14
class EEMHealUnit : public EEMessage {
public:
    EEMHealUnit(unsigned char* inData) { this->ImportMessage(inData); }
    // inline: out-of-line COMDAT 0x41fc after its first user (type 0x92; the store follows the vptr)
    EEMHealUnit(long inTarget) : EEMessage(kMessageHealUnit, false) { this->mTargetID = inTarget; }
    virtual size_t GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass);
    virtual void Execute();
    virtual char* GetMessageLabel() const;

    int32_t mTargetID;                      // +0x10
};

// vtable 0x43da64; sizeof 0x1c
class EEMImmediateCargoUnload : public EEMessage {
public:
    EEMImmediateCargoUnload(unsigned char* inData) { this->ImportMessage(inData); }
    // inline (out-of-line COMDAT 0x25af3d after its first user EEUICUnitIconBar::MouseRButtonUp; type 0x95)
    EEMImmediateCargoUnload(long inTransportID, long inCargoID, bool inFlag)
        : EEMessage(kMessageImmediateCargoUnload, false), mTransportID(inTransportID), mCargoID(inCargoID), mB18(inFlag) {}
    virtual size_t GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass);
    virtual void Execute();
    virtual char* GetMessageLabel() const { return "Immediate Cargo Unload"; }

    int32_t mTransportID;                   // +0x10
    int32_t mCargoID;                       // +0x14
    bool mB18;                              // +0x18
};

// vtable 0x439118; sizeof 0x18
class EEMImproveUnit : public EEMessage {
public:
    EEMImproveUnit(unsigned char* inData) { this->ImportMessage(inData); }
    // inline: COMDAT-free (EEAUpgradeImprovement::Virtual4 0x50dc expands it, calling EEMessage's out-of-line copy
    // 0x32a6; type 0x77; the members after the vptr)
    EEMImproveUnit(long inUnitID, long inImprovement) : EEMessage(kMessageImproveUnit, false)
    {
        this->mUnitID = inUnitID;
        this->mImprovement = inImprovement;
    }
    virtual size_t GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass);
    virtual void Execute();
    virtual char* GetMessageLabel() const { return "Upgrade Unit"; }

    int32_t mUnitID;                        // +0x10
    int32_t mImprovement;                   // +0x14
};

// vtable 0x43d994; sizeof 0x14
class EEMKillUnit : public EEMessage {
public:
    EEMKillUnit(unsigned char* inData) { this->ImportMessage(inData); }
    // inline: out-of-line COMDAT 0x260685 after its first user (type 0x5c)
    EEMKillUnit(unsigned char inAllUnits) : EEMessage(kMessageKillUnit, false), mAllUnits(inAllUnits) {}
    virtual size_t GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass);
    virtual void Execute();
    virtual char* GetMessageLabel() const { return "Delete Unit"; }

    unsigned char mAllUnits;                // +0x10
};

// vtable 0x43d828; sizeof 0x1c
class EEMLoadGarrison : public EEMessage {
public:
    EEMLoadGarrison(unsigned char* inData) { this->ImportMessage(inData); }
    // inline: EEModeGarrison expands it (type 0x7b)
    EEMLoadGarrison(long inTarget, long inUnitID, unsigned char inB18) : EEMessage(kMessageLoadGarrison, false)
    {
        this->mB18 = inB18;
        this->mUnitID = inUnitID;
        this->mGarrisonID = inTarget;
    }
    virtual size_t GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass);
    virtual void Execute();
    virtual char* GetMessageLabel() const { return "Load Unit(s) Into Garrison"; }

    int32_t mGarrisonID;                    // +0x10
    int32_t mUnitID;                        // +0x14
    unsigned char mB18;                     // +0x18
};

// vtable 0x43e0f0; sizeof 0x34; import ctor 0xba957, ??_G 0xba9b9; Get/SetSendToAllAndWaitMessageID inline (0xba9a1/0xba9af)
class EEMLoadOrSaveGame : public EEMessage {
public:
    EEMLoadOrSaveGame(unsigned char* inData) { this->ImportMessage(inData); }
    EEMLoadOrSaveGame(const UWideString& inS20, unsigned long inTime, unsigned char inIsLoad, bool inIsQuiet,
                      long inTick, unsigned char inIsAutoSave);  // 0x151de3 (SLE SaveFile)
    long LoadOrSave();                      // 0xc1052 (EEPhysics::ServeLoadSaveRequests runs each queued one; provisional name)
    virtual bool GetCanSendWhilePaused() const { return true; }
    virtual bool GetSendToAllAndWaitMessageID(unsigned long& outID) const { outID = this->mWaitMessageID; return true; }
    virtual void SetSendToAllAndWaitMessageID(unsigned long inID) { this->mWaitMessageID = inID; }
    virtual size_t GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass);
    virtual void ImportMessage(unsigned char* inData);
    virtual bool IsResponse(NEMessage* inMessage);
    virtual void Execute();
    virtual char* GetMessageLabel() const { return "Load or Save"; }

    uint32_t mWaitMessageID;                // +0x10
    int32_t mTick;                          // +0x14
    int32_t mTime;                          // +0x18
    unsigned char mIsLoad;                  // +0x1c
    bool mIsQuiet;                          // +0x1d
    unsigned char mIsAutoSave;              // +0x1e
    UWideString mFileName;                  // +0x20
};

// vtable 0x438ddc; sizeof 0x1c
class EEMLoadTransport : public EEMessage {
public:
    // inline (out-of-line COMDAT 0x3192 after its first user EEAMouseClick::CreateLoadTransportMessage, which also
    // expands it once)
    EEMLoadTransport(long inTransportID, long inPassengerID, unsigned char inB18)
        : EEMessage(kMessageLoadTransport), mTransportID(inTransportID), mPassengerID(inPassengerID), mB18(inB18) {}
    EEMLoadTransport(unsigned char* inData) { this->ImportMessage(inData); }
    virtual size_t GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass);
    virtual void Execute();
    virtual char* GetMessageLabel() const { return "Load Unit(s) Into Transport"; }

    int32_t mTransportID;                   // +0x10
    int32_t mPassengerID;                   // +0x14
    unsigned char mB18;                     // +0x18
};

// vtable 0x43d7f4; sizeof 0x14
class EEMLockUnlock : public EEMessage {
public:
    EEMLockUnlock(unsigned char* inData) { this->ImportMessage(inData); }
    // inline: COMDAT 0x26f9ce after its first user EEUIFMGStatusButton::Command (type 0x8c)
    EEMLockUnlock(bool inLock) : EEMessage(kMessageLockUnlock, false), mIsLocked(inLock) {}
    virtual size_t GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass);
    virtual void Execute();
    virtual char* GetMessageLabel() const;

    bool mIsLocked;                         // +0x10 (locked)
};

// vtable 0x43d6f0; sizeof 0x18
class EEMNewEdictEnacted : public EEMessage {
public:
    EEMNewEdictEnacted(unsigned char* inData) { this->ImportMessage(inData); }
    // 0xcd0f0 (EEServer.cpp; the three words come by reference)
    EEMNewEdictEnacted(const unsigned short& inW10, const unsigned short& inW12, const unsigned short& inW14,
                       unsigned char inIsEnacted)
        : EEMessage(kMessageNewEdictEnacted, false), mEdict(inW10), mYesVotes(inW12), mNoVotes(inW14), mIsEnacted(inIsEnacted) {}
    virtual size_t GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass);
    virtual void Execute();
    virtual char* GetMessageLabel() const { return "New Edict Enabled"; }

    unsigned short mEdict;                  // +0x10
    unsigned short mYesVotes;               // +0x12
    unsigned short mNoVotes;                // +0x14
    unsigned char mIsEnacted;               // +0x16
};

// vtable 0x43d7c0; sizeof 0x14
class EEMPackUnpack : public EEMessage {
public:
    EEMPackUnpack(unsigned char* inData) { this->ImportMessage(inData); }
    // inline: COMDAT 0x26f9fb after its first user EEUIFMGStatusButton::Command (type 0xab)
    EEMPackUnpack(bool inPack) : EEMessage(kMessagePackUnpack, false), mPack(inPack) {}
    virtual size_t GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass);
    virtual void Execute();
    virtual char* GetMessageLabel() const;

    bool mPack;                             // +0x10 (pack)
};

// vtable 0x438d5c; sizeof 0x20
class EEMPathTo : public EEMessage {
public:
    EEMPathTo(unsigned char* inData) { this->ImportMessage(inData); }
    // inline: EEAMiniMap::SendGroup 0x2aaa expands it (type 0x4d; NEMessage's own ctor refused there: the exported
    // copy is called; +0x14/+0x18/+0x1c/+0x1f before the vptr, +0x1d/+0x10/+0x1e after it)
    EEMPathTo(float inX, float inY, float inAngle, bool inIsForcedMove, bool inIsQueued)
        : EEMessage(kMessagePathTo, false), mX(inX), mY(inY), mIsForcedMove(inIsForcedMove), mUseGroupMovement(false)
    {
        this->mIsRotating = true;
        this->mFacing = inAngle;
        this->mIsQueued = inIsQueued;
    }
    // EEAMouseClick::Execute 0x403350 builds its paths with mB1D/mUseGroupMovement from arguments (stores
    // +0x14/+0x18/+0x1c/+0x1d/+0x10/+0x1e, vptr, +0x1f; no separate null test before the stores)
    EEMPathTo(float inX, float inY, float inAngle, bool inIsForcedMove, bool inIsRotating, bool inIsQueued, bool inUseGroupMovement)
        : EEMessage(kMessagePathTo, false), mX(inX), mY(inY), mIsForcedMove(inIsForcedMove), mIsRotating(inIsRotating)
    {
        this->mFacing = inAngle;
        this->mIsQueued = inIsQueued;
        this->mUseGroupMovement = inUseGroupMovement;
    }
    virtual size_t GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass);
    virtual void Execute();
    virtual char* GetMessageLabel() const;

    float mFacing;                          // +0x10
    float mX;                               // +0x14
    float mY;                               // +0x18
    bool mIsForcedMove;                     // +0x1c
    bool mIsRotating;                       // +0x1d
    bool mIsQueued;                         // +0x1e
    bool mUseGroupMovement;                 // +0x1f
};

// vtable 0x43d6bc; sizeof 0x14
class EEMPatrol : public EEMessage {
public:
    EEMPatrol(unsigned char* inData) { this->ImportMessage(inData); }
    // inline: COMDAT 0x26fa28 after its first user EEUIFMGStatusButton::Command (type 0x73; the store follows the vptr:
    // a body assignment)
    EEMPatrol(long inTargetID) : EEMessage(kMessagePatrol, false) { this->mTargetID = inTargetID; }
    virtual size_t GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass);
    virtual void Execute();
    virtual char* GetMessageLabel() const;

    int32_t mTargetID;                      // +0x10
};

// The lobby slot fields EEMPlayerInformation updates (mField; Execute): 0 type, 1 ping, 2 colour, 3 team,
// 4 starting citizens, 5 ready; port: 6 a computer slot's engine (ee/EECPEngine.h). The lobby redraws the row with
// the field as LobbyShowPlayer's flags.
const long kPlayerInfoEngine = 6;

// vtable 0x43e158; sizeof 0x28
class EEMPlayerSlotInfo : public EEMessage {
public:
    EEMPlayerSlotInfo(unsigned char* inData) { this->ImportMessage(inData); }
    // 0xccee8 (EEServer.cpp; argument names are provisional)
    EEMPlayerSlotInfo(unsigned char inSlot, unsigned char inPlayerID, const UWideString& inS10, bool inIsBehindFirewall)
        : EEMessage(kMessagePlayerSlotInfo, true), mAddress(inS10), mPlayerID(inPlayerID), mSlot(inSlot), mIsBehindFirewall(inIsBehindFirewall) {}
    virtual size_t GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass);
    virtual void ImportMessage(unsigned char* inData);
    virtual void Execute();
    virtual char* GetMessageLabel() const { return "Player Slot Info"; }

    UWideString mAddress;                   // +0x10
    unsigned char mPlayerID;                // +0x24
    unsigned char mSlot;                    // +0x25
    bool mIsBehindFirewall;                 // +0x26
};

// vtable 0x43d584; sizeof 0x1c
class EEMRemoveTechnology : public EEMessage {
public:
    EEMRemoveTechnology(unsigned char* inData) { this->ImportMessage(inData); }
    // inline: out-of-line COMDAT 0x26f2b0 after its first user EEUIFMGStatusButton::MouseRButtonUp (type 0x53;
    // mFiveTimes follows the vptr store: a body assignment)
    EEMRemoveTechnology(long inUnitID, long inTechnology, bool inToggleJITManufacturing, bool inFiveTimes)
        : EEMessage(kMessageRemoveTechnology, false), mUnitID(inUnitID), mTechnology(inTechnology), mToggleJITManufacturing(inToggleJITManufacturing)
    {
        this->mFiveTimes = inFiveTimes;
    }
    // inline: COMDAT-free (EEUICPortrait::MouseLButtonDown 0x258d02 expands it: mFiveTimes before the vptr, then the
    // body; type 0x53)
    EEMRemoveTechnology(long inUnitID, long inTechnology, bool inToggleJITManufacturing) : EEMessage(kMessageRemoveTechnology, false), mFiveTimes(false)
    {
        this->mUnitID = inUnitID;
        this->mTechnology = inTechnology;
        this->mToggleJITManufacturing = inToggleJITManufacturing;
    }
    virtual size_t GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass);
    virtual void Execute();
    virtual char* GetMessageLabel() const;

    int32_t mUnitID;                        // +0x10
    int32_t mTechnology;                    // +0x14
    bool mToggleJITManufacturing;           // +0x18 (toggle the JIT manufacturing)
    bool mFiveTimes;                        // +0x19 (five times)
};

// vtable 0x438f80; sizeof 0x18
class EEMRepairUnit : public EEMessage {
public:
    EEMRepairUnit(long inTargetID, bool inIsQueued) : EEMessage(kMessageRepairUnit, false), mTargetID(inTargetID), mIsQueued(inIsQueued) {}  // inline (EEAMouseClick::Execute 0x40395c)
    EEMRepairUnit(unsigned char* inData) { this->ImportMessage(inData); }
    virtual size_t GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass);
    virtual void Execute();
    virtual char* GetMessageLabel() const;

    int32_t mTargetID;                      // +0x10
    bool mIsQueued;                         // +0x14
};

// vtable 0x43d5b8; sizeof 0x14
class EEMReplantFarms : public EEMessage {
public:
    EEMReplantFarms(unsigned char* inData) { this->ImportMessage(inData); }
    EEMReplantFarms(long inFarmID);         // 0x1fe345 (EEComplexUnit.cpp, after its first user SetBuilt)
    virtual size_t GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass);
    virtual void Execute();
    virtual char* GetMessageLabel() const;

    int32_t mFarmID;                        // +0x10
};

// vtable 0x43d51c; sizeof 0x18
class EEMReportStatistics : public EEMessage {
public:
    EEMReportStatistics(unsigned char* inData) { this->ImportMessage(inData); }
    EEMReportStatistics(long inL10, long inL14);   // 0x153917 (EEPhysics.cpp, after its first user)
    virtual size_t GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass);
    virtual void Execute();
    virtual char* GetMessageLabel() const;

    int32_t mL10;                           // +0x10
    int32_t mL14;                           // +0x14
};

// vtable 0x43d550; sizeof 0x1c
class EEMResearchTechnology : public EEMessage {
public:
    EEMResearchTechnology(unsigned char* inData) { this->ImportMessage(inData); }
    // inline: EEUIFMGStatusButton::ButtonUp 0x26e274 expands it (type 0x52)
    EEMResearchTechnology(long inUnitID, long inTechnology, bool inFiveTimes) : EEMessage(kMessageResearchTechnology, false), mBuildingID(inUnitID)
    {
        this->mTechnology = inTechnology;
        this->mFiveTimes = inFiveTimes;
    }
    virtual size_t GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass);
    virtual void Execute();
    virtual char* GetMessageLabel() const;

    int32_t mBuildingID;                    // +0x10
    int32_t mTechnology;                    // +0x14
    bool mFiveTimes;                        // +0x18 (five times)
};

// vtable 0x438d24; sizeof 0x14
class EEMSelectPatrolGroup : public EEMessage {
public:
    EEMSelectPatrolGroup(unsigned char* inData) { this->ImportMessage(inData); }
    // inline: COMDAT 0x2a3b, first used by EEALasso::SelectUnits 0x28e5 (type 0x74; mL10 assigned after the vptr)
    EEMSelectPatrolGroup(long inUnitID) : EEMessage(kMessageSelectPatrolGroup, false) { this->mUnitID = inUnitID; }
    virtual size_t GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass);
    virtual void Execute();
    virtual char* GetMessageLabel() const;

    int32_t mUnitID;                        // +0x10
};

// vtable 0x43e18c; sizeof 0x20; import ctor 0xbaaf1, ??_G 0xbab3d
class EEMSelectTheseUnits : public EEMessage {
public:
    EEMSelectTheseUnits(unsigned char* inData) { this->ImportMessage(inData); }
    // inline (out-of-line COMDAT 0x25adcd after its first user EEUICUnitIconBar::MouseLButtonUp; type 0x96)
    EEMSelectTheseUnits(std::vector<long>& inUnitIDs) : EEMessage(kMessageSelectTheseUnits, true)
    {
        for (unsigned long i = 0; i < inUnitIDs.size(); i++)
            this->mUnitIDs.push_back(inUnitIDs[i]);
    }
    virtual ~EEMSelectTheseUnits() {}
    virtual bool GetCanSendWhilePaused() const { return true; }
    virtual void ExportMessage(unsigned char** outData, unsigned long& outSize);
    virtual unsigned long ExportSize();
    virtual void ImportMessage(unsigned char* inData);
    virtual void Execute();
    virtual char* GetMessageLabel() const;

    long GetUnitIDsAt(unsigned long inIndex) const { return this->mUnitIDs[inIndex]; }
    std::vector<long> mUnitIDs;             // +0x10
};

// vtable 0x438d90; sizeof 0x24
class EEMSetRallyGoal : public EEMessage {
public:
    EEMSetRallyGoal(unsigned char* inData) { this->ImportMessage(inData); }
    // inline: COMDAT 0x2ca6 after its first user EEAMiniMap::SendGroup (type 0x66; every member in the init list, so
    // EEMessage's vptr store stays)
    EEMSetRallyGoal(long inUnitID, const GE2DPoint& inLocation, long inTargetID, unsigned char inIsQueued)
        : EEMessage(kMessageSetRallyGoal, false), mPoint(inLocation), mTargetID(inTargetID), mBuildingID(inUnitID), mIsQueued(inIsQueued) {}
    virtual size_t GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass);
    virtual void Execute();
    virtual char* GetMessageLabel() const { return "Set Rally Goal"; }

    GE2DPoint mPoint;                       // +0x10
    int32_t mTargetID;                      // +0x18
    int32_t mBuildingID;                    // +0x1c
    unsigned char mIsQueued;                // +0x20
};

// vtable 0x43e2f8; sizeof 0x14
class EEMSetUnitBehavior : public EEMessage {
public:
    EEMSetUnitBehavior(unsigned char* inData) { this->ImportMessage(inData); }
    // inline (0x260a12 expands it: the EEMessage ctor 0x32a6 call, the vptr, then +0x10; type 0x9e)
    EEMSetUnitBehavior(long inBehavior) : EEMessage(kMessageSetUnitBehavior, false) { this->mBehavior = inBehavior; }
    virtual size_t GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass);
    virtual void Execute();
    virtual char* GetMessageLabel() const { return "Set Unit Behavior"; }

    int32_t mBehavior;                      // +0x10
};

// vtable 0x438f4c; sizeof 0x18
class EEMSmartFarmers : public EEMessage {
public:
    EEMSmartFarmers(long inTargetID, unsigned char inIsQueued) : EEMessage(kMessageSmartFarmers, false), mTargetID(inTargetID), mIsQueued(inIsQueued) {}  // inline (EEAMouseClick::Execute 0x4039e4)
    EEMSmartFarmers(unsigned char* inData) { this->ImportMessage(inData); }
    virtual size_t GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass);
    virtual void Execute();
    virtual char* GetMessageLabel() const;

    int32_t mTargetID;                      // +0x10
    unsigned char mIsQueued;                // +0x14
};

// vtable 0x43d244; sizeof 0x14,None
class EEMStartLoadingResponse : public EEMessage {
public:
    EEMStartLoadingResponse(unsigned char* inData) { this->ImportMessage(inData); }
    EEMStartLoadingResponse(unsigned char inSlot, unsigned char inPlayerID, unsigned char inLobbySlot) : EEMessage(kMessageStartLoadingResponse, true), mSlot(inSlot), mPlayerID(inPlayerID), mLobbySlot(inLobbySlot) {}  // 0xcb815 (EEServer.cpp)
    virtual bool GetCanSendWhilePaused() const { return true; }
    virtual size_t GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass);
    virtual void Execute();
    virtual char* GetMessageLabel() const { return "Start Loading Response"; }

    unsigned char mSlot;                    // +0x10
    unsigned char mPlayerID;                // +0x11
    unsigned char mLobbySlot;               // +0x12
};

// vtable 0x43d78c; sizeof 0x1c
class EEMUnloadTransport : public EEMessage {
public:
    EEMUnloadTransport(unsigned char* inData) { this->ImportMessage(inData); }
    // inline: EEUIFMGStatusButton::Command / EEModeUnload expand it (type 0x68)
    // members in the initializer list: the inline expansions store +0x10/+0x14/+0x18 before the vptr (0x26f41b..)
    EEMUnloadTransport(long inUnitID, long inX, long inY)
        : EEMessage(kMessageUnloadTransport, false), mUnitID(inUnitID), mX(inX), mY(inY) {}
    virtual size_t GetCodecSlots(EeNetMsgSlot* outSlots, uint32_t& outClass);
    virtual void Execute();
    virtual char* GetMessageLabel() const { return "Unload Unit(s) From Transport"; }
    // inline (Execute 0x4c8615 loads both into registers before the argument pushes; provisional names)
    long GetX() const { return this->mX; }
    long GetY() const { return this->mY; }

    int32_t mUnitID;                        // +0x10
    int32_t mX;                             // +0x14
    int32_t mY;                             // +0x18
};

#endif
