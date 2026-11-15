// The engine messages (NEMessage.cpp's 28 types, 0x20..0x3c) as the game's senders and the engine's factory see them
// (LLE Network Engine round 6): every type built by its sender's constructor over value patterns (truncated to the
// members' sizes, bools, strings, the transfer block), then its label, flags, send-to-all-and-wait id, ExportSize and
// exported bytes, and the same for the message the factory builds again from those bytes; IsResponse over every pair
// of a set of messages; the players' inputs (the 255 limit, an index past the end, nested game and engine messages
// through the factory, the inputs released with the message); the factory's unknown types.
//
// The API adapter below (Make, the inputs, the local member, the registration) is the only part that names the
// message classes: it is NEMRecord's now. The trace is compared with tests/net/golden_message.txt, which the adapter
// over the original 28 classes made (commit 31917336 of branch rust-ne6, "ne_message_test: the engine messages'
// golden"). `--print FILE` writes it.
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>

#include "lle/LowLevelEngine.h"
#include "lle/NEMessage.h"
#include "rust/ee_netmsg.h"

// ---- the engine functions the units call

void ESVerify(const char* inFile, unsigned long inLine, const char* inExpression);
static std::string sTrace;

static void Line(const char* inFormat, ...)
{
    char theLine[4096];
    va_list theArgs;
    va_start(theArgs, inFormat);
    vsnprintf(theLine, sizeof(theLine), inFormat, theArgs);
    va_end(theArgs);
    sTrace += theLine;
    sTrace += '\n';
}
void ESVerify(const char*, unsigned long inLine, const char*)
{
    Line("  verify %lu", inLine);
}
unsigned long ESRecordNativeError(unsigned long inError)
{
    return inError;
}
void ESChecksum::AddChecksum(const UString&, unsigned long)
{
}
float URandom::Generate()   // as GEBitmap.cpp (the strings' units call it)
{
    this->mSeed = (this->mMultiplier * this->mSeed + this->mIncrement) % this->mModulus;
    return (float)((double)this->mSeed / (double)this->mModulus);
}

static std::string Hex(const unsigned char* inData, size_t inSize)
{
    std::string theHex;
    char theByte[4];
    for (size_t i = 0; i < inSize; i++) {
        snprintf(theByte, sizeof(theByte), "%02x", inData[i]);
        theHex += theByte;
    }
    return theHex.empty() ? "-" : theHex;
}

// ---- a game message stand-in (type 0x41: the factory's registered builder makes it; its destructor is traced)

class TGame : public NEMessage {
public:
    TGame(unsigned char inValue) : NEMessage(0x41), mValue(inValue) {}
    TGame(unsigned char* inData) { this->ImportMessage(inData); }
    virtual ~TGame() { Line("  game message %02x freed", this->mValue); }
    virtual void ExportMessage(unsigned char** outData, unsigned long& outSize)
    {
        *outData = new unsigned char[5];
        (*outData)[0] = this->mReserved;
        (*outData)[1] = this->mType;
        (*outData)[2] = this->mForSpecificSlot;
        (*outData)[3] = this->mOriginatingSlot;
        (*outData)[4] = this->mValue;
        outSize = 5;
    }
    virtual unsigned long ExportSize() { return 5; }
    virtual void ImportMessage(unsigned char* inData)
    {
        this->mReserved = inData[0];
        this->mType = inData[1];
        this->mForSpecificSlot = inData[2];
        this->mOriginatingSlot = inData[3];
        this->mValue = inData[4];
    }
    virtual void Execute() {}
    virtual char* GetMessageLabel() const { return (char*)"Test Game Message"; }

    unsigned char mValue;
};

static NEMessage* BuildTGame(unsigned char* inData)
{
    return new TGame(inData);
}

// ---- the API adapter: the senders' constructors with the members in wire order (v), the strings (s0, s1), the
// transfer block; the players' inputs; the physics tick's member that is not on the wire; the registration

static NEMessage* Make(unsigned char inType, const uint32_t* v, const char* s0, const char* s1,
                       const unsigned char* inBlock)
{
    // the record's fixed members and strings, as many as the type has (a sender names them all)
    size_t theValues = 0, theStrings = 0;
    for (uint32_t i = 0; ee_netmsg_ne_field(inType, i).kind != EE_NETMSG_NE_FIELD_NONE; i++) {
        uint8_t theKind = ee_netmsg_ne_field(inType, i).kind;
        theValues += theKind == EE_NETMSG_NE_FIELD_INTEGER || theKind == EE_NETMSG_NE_FIELD_BOOL;
        theStrings += theKind == EE_NETMSG_NE_FIELD_STRING;
    }
    NEMRecord* theMessage = NULL;
    switch (theValues * 10 + theStrings) {
    case 0: theMessage = new NEMRecord(inType); break;
    case 1: theMessage = new NEMRecord(inType, {}, {s0}); break;
    case 10: theMessage = new NEMRecord(inType, {v[0]}); break;
    case 11: theMessage = new NEMRecord(inType, {v[0]}, {s0}); break;
    case 20: theMessage = new NEMRecord(inType, {v[0], v[1]}); break;
    case 22: theMessage = new NEMRecord(inType, {v[0], v[1]}, {s0, s1}); break;
    case 30: theMessage = new NEMRecord(inType, {v[0], v[1], v[2]}); break;
    case 40: theMessage = new NEMRecord(inType, {v[0], v[1], v[2], v[3]}); break;
    case 41: theMessage = new NEMRecord(inType, {v[0], v[1], v[2], v[3]}, {s0}); break;
    case 50: theMessage = new NEMRecord(inType, {v[0], v[1], v[2], v[3], v[4]}); break;
    }
    if (inType == kNetMessageXferMessage)   // the block: the sender's bytes, the rest 0
        memcpy(theMessage->Field(2, 0x400), inBlock, v[1]);
    return theMessage;
}

// round 7's API for the Rust server and client, checked against the senders' (not traced: a failure is counted and
// fails the run): the array constructor exports the bytes the sender's does; Value(i) is member i (its value
// truncated as stored, a bool 0 or 1; the inputs' count; the block's address)
static int sRustChecks, sRustFailures;
static void RustCheck(bool inOK, const char* inWhat, unsigned char inType, uint32_t inIndex)
{
    sRustChecks++;
    if (!inOK && sRustFailures++ < 10)
        fprintf(stderr, "round 7 check failed: %s type %02x member %u\n", inWhat, inType, inIndex);
}
static void CheckRustApi(unsigned char inType, const uint32_t* v, NEMessage* inSent)
{
    NEMRecord* theSent = static_cast<NEMRecord*>(inSent);
    size_t theValues = 0;
    bool theStrings = false;
    for (uint32_t i = 0; ee_netmsg_ne_field(inType, i).kind != EE_NETMSG_NE_FIELD_NONE; i++) {
        EeNetMsgNeField f = ee_netmsg_ne_field(inType, i);
        theStrings |= f.kind == EE_NETMSG_NE_FIELD_STRING;
        if (f.kind == EE_NETMSG_NE_FIELD_INTEGER || f.kind == EE_NETMSG_NE_FIELD_BOOL) {
            uint64_t theWant = f.kind == EE_NETMSG_NE_FIELD_BOOL ? v[theValues] != 0
                             : f.size >= 4 ? v[theValues] : v[theValues] & ((1u << (8 * f.size)) - 1);
            RustCheck(theSent->Value(i) == theWant, "value", inType, i);
            theValues++;
        } else if (f.kind == EE_NETMSG_NE_FIELD_INPUTS) {
            RustCheck(theSent->Value(i) == theSent->GetNumberOfPlayerInputs(), "inputs", inType, i);
        } else if (f.kind != EE_NETMSG_NE_FIELD_STRING) {
            RustCheck(theSent->Value(i) == (uintptr_t)theSent->Field(i, f.size), "block", inType, i);
        }
    }
    if (theStrings)
        return;
    NEMRecord* theRust = new NEMRecord(inType, v, theValues);
    if (inType == kNetMessageXferMessage)
        memcpy(theRust->Field(2, 0x400), theSent->Field(2, 0x400), 0x400);
    theRust->SetForSpecificSlot(theSent->GetForSpecificSlot());
    theRust->SetOriginatingSlot(theSent->GetOriginatingSlot());
    unsigned long theID = 0;
    if (theSent->GetSendToAllAndWaitMessageID(theID))
        theRust->SetSendToAllAndWaitMessageID(theID);
    unsigned char *a = NULL, *b = NULL;
    unsigned long na = 0, nb = 0;
    theSent->ExportMessage(&a, na);
    theRust->ExportMessage(&b, nb);
    RustCheck(na == nb && memcmp(a, b, na) == 0, "array constructor", inType, 0);
    delete[] a;
    delete[] b;
    theRust->Dereference();
}

static long AddInput(NEMessage* inTick, NEMessage* inInput)
{
    return static_cast<NEMRecord*>(inTick)->AddPlayerInput(inInput);
}
static long GetInput(NEMessage* inTick, unsigned long inIndex, NEMessage*& outInput)
{
    return static_cast<NEMRecord*>(inTick)->GetPlayerInput(inIndex, outInput);
}
static unsigned long InputCount(NEMessage* inTick)
{
    return static_cast<NEMRecord*>(inTick)->GetNumberOfPlayerInputs();
}
static unsigned long LocalPerSecond(NEMessage* inPhysicsTick)
{
    return static_cast<NEMRecord*>(inPhysicsTick)->Get<unsigned char>(4);   // mPhysicsTicksPerSecond (not on the wire)
}
static void RegisterGame()
{
    NEMessage::RegisterKnownMessages();
    NEMessage::RegisterBuilder(0x41, 0x41, BuildTGame);
}
static NEMessage* BuildEngine(unsigned char* inData)
{
    return NEMessage::BuildNEMessage(inData);
}

// ---- the trace

static const unsigned char kTypes[] = {0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b, 0x2d, 0x2e,
                                       0x2f, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x3b, 0x3c};

static void Dump(const char* inWhat, NEMessage* inMessage)
{
    unsigned long theID = 0xdeadbeef;
    bool theHasID = inMessage->GetSendToAllAndWaitMessageID(theID);
    Line("  %s type %02x for %02x from %02x label \"%s\" throttle %d paused %d available %d s2aw %d %lx", inWhat,
         inMessage->GetType(), inMessage->GetForSpecificSlot(), inMessage->GetOriginatingSlot(),
         inMessage->GetMessageLabel(), (int)inMessage->GetCountsTowardsPlayerInputThrottle(),
         (int)inMessage->GetCanSendWhilePaused(), (int)inMessage->GetIsAvailableToSend(), (int)theHasID, theID);
    unsigned char* theData = NULL;
    unsigned long theSize = 0;
    unsigned long theExportSize = inMessage->ExportSize();
    inMessage->ExportMessage(&theData, theSize);
    Line("    size %lu export %lu %s", theExportSize, theSize, Hex(theData, theSize).c_str());
    delete[] theData;
}

static uint32_t sRandom = 1;
static uint32_t Next()
{
    sRandom = sRandom * 1103515245u + 12345u;
    return sRandom >> 8;
}

// the value patterns: 0, all ones, a count, the bytes' and words' edges, random
static void Pattern(int inPattern, uint32_t* outValues)
{
    for (int i = 0; i < 8; i++) {
        switch (inPattern) {
        case 0: outValues[i] = 0; break;
        case 1: outValues[i] = 0xffffffffu; break;
        case 2: outValues[i] = (uint32_t)(i + 1); break;
        case 3: outValues[i] = 0x80u << (i % 4 * 8); break;
        case 4: outValues[i] = 0x1ffu + (uint32_t)i * 0x10101u; break;
        case 5: outValues[i] = 0x7fffffffu - (uint32_t)i; break;
        default: outValues[i] = Next() ^ (Next() << 16); break;
        }
    }
}

static const char* kStrings[] = {"", "a", "Player One", "J\xc3\xb6rg \xe2\x82\xac", "1.0.0.2"};

static void Senders()
{
    std::string theLong(300, 'x');
    unsigned char theBlock[0x400];
    for (size_t i = 0; i < sizeof(theBlock); i++)
        theBlock[i] = (unsigned char)(i * 7 + 3);
    for (unsigned char theType : kTypes) {
        for (int p = 0; p < 9; p++) {
            uint32_t v[8];
            Pattern(p, v);
            const char* s0 = p == 8 ? theLong.c_str() : kStrings[p % 5];
            const char* s1 = kStrings[(p + 2) % 5];
            if (theType == kNetMessageXferMessage)
                v[1] = p == 0 ? 0 : p == 1 ? 0x400 : p == 2 ? 5 : v[1] % 0x401;
            Line("make %02x pattern %d", theType, p);
            NEMessage* theMessage = Make(theType, v, s0, s1, theBlock);
            if (p % 3 == 1) {
                theMessage->SetForSpecificSlot((unsigned char)(v[5] + p));
                theMessage->SetOriginatingSlot((unsigned char)(v[6] + 2 * p));
            }
            if (theType == kNetMessagePauseGame || theType == kNetMessageUnPauseGame)
                theMessage->SetSendToAllAndWaitMessageID(v[1]);
            else
                theMessage->SetSendToAllAndWaitMessageID(0x1234);   // no member: nothing happens
            Dump("sent", theMessage);
            CheckRustApi(theType, v, theMessage);
            if (theType == kNetMessagePhysicsTick)
                Line("    ticks per second %lu", LocalPerSecond(theMessage));
            unsigned char* theData = NULL;
            unsigned long theSize = 0;
            theMessage->ExportMessage(&theData, theSize);
            std::vector<unsigned char> theBytes(theData, theData + theSize);
            theBytes.resize(theBytes.size() + 16, 0);
            delete[] theData;
            NEMessage* theCopy = NEMessage::ConvertMemoryToMessage(&theBytes[0]);
            if (theCopy == NULL) {
                Line("  no copy");
            } else {
                Dump("copy", theCopy);
                theCopy->Dereference();
            }
            theMessage->Dereference();
        }
    }
}

static void Responses()
{
    struct Spec {
        unsigned char mType;
        uint32_t mValue;
    };
    static const Spec kSpecs[] = {
        {kNetMessageCommunicationsTick, 5}, {kNetMessageCommunicationsTick, 6}, {kNetMessageCommunicationsTickResponse, 5},
        {kNetMessageCommunicationsTickResponse, 6}, {kNetMessagePauseGame, 7}, {kNetMessagePauseGame, 8},
        {kNetMessagePauseGameResponse, 7}, {kNetMessagePauseGameResponse, 8}, {kNetMessageUnPauseGame, 7},
        {kNetMessageUnPauseGameResponse, 7}, {kNetMessageUnPauseGameResponse, 8}, {kNetMessageServerIsAlive, 7},
        {kNetMessageStartGameResponse, 7}, {kNetMessagePlayerConnectionCheckResponse, 5}, {kNetMessageStopGame, 0},
    };
    const size_t n = sizeof(kSpecs) / sizeof(kSpecs[0]);
    std::vector<NEMessage*> theMessages;
    unsigned char theBlock[0x400] = {0};
    for (size_t i = 0; i < n; i++) {
        uint32_t v[8] = {0};
        // the tick response's servers' tick is its second member
        v[kSpecs[i].mType == kNetMessageCommunicationsTickResponse ? 1 : 0] = kSpecs[i].mValue;
        theMessages.push_back(Make(kSpecs[i].mType, v, "", "", theBlock));
    }
    theMessages.push_back(new TGame(7));
    Line("responses");
    for (size_t i = 0; i < theMessages.size(); i++) {
        std::string theRow;
        for (size_t j = 0; j < theMessages.size(); j++)
            theRow += theMessages[i]->IsResponse(theMessages[j]) ? '1' : '0';
        Line("  %02x %s", theMessages[i]->GetType(), theRow.c_str());
    }
    for (NEMessage* m : theMessages)
        m->Dereference();
}

static void PlayerInputs()
{
    unsigned char theBlock[0x400] = {0};
    for (unsigned char theType : {kNetMessageCommunicationsTick, kNetMessageCommunicationsTickResponse}) {
        uint32_t v[8] = {9, 2, 20, 1, 0, 0, 0, 0};
        Line("inputs %02x", theType);
        NEMessage* theTick = Make(theType, v, "", "", theBlock);
        NEMessage* theInput = NULL;
        Line("  empty: count %lu get %ld", InputCount(theTick), GetInput(theTick, 0, theInput));
        // a game message, an engine message, a tick with an input of its own
        NEMessage* theGame = new TGame(0x51);
        theGame->SetOriginatingSlot(3);
        Line("  add %ld", AddInput(theTick, theGame));
        uint32_t w[8] = {1, 0, 0, 0, 0, 0, 0, 0};
        Line("  add %ld", AddInput(theTick, Make(kNetMessageRemovePlayer, w, "", "", theBlock)));
        NEMessage* theInner = Make(kNetMessageCommunicationsTick, v, "", "", theBlock);
        AddInput(theInner, new TGame(0x52));
        Line("  add %ld", AddInput(theTick, theInner));
        Dump("tick", theTick);
        unsigned char* theData = NULL;
        unsigned long theSize = 0;
        theTick->ExportMessage(&theData, theSize);
        std::vector<unsigned char> theBytes(theData, theData + theSize);
        theBytes.resize(theBytes.size() + 16, 0);
        delete[] theData;
        NEMessage* theCopy = NEMessage::ConvertMemoryToMessage(&theBytes[0]);
        Dump("copy", theCopy);
        for (unsigned long i = 0; i <= InputCount(theCopy); i++) {
            long theResult = GetInput(theCopy, i, theInput);
            Line("  input %lu: %ld type %02x", i, theResult, theResult == 0 ? theInput->GetType() : 0);
        }
        Line("  free the copy");
        theCopy->Dereference();
        // the limit: 255 inputs, the 256th refused
        for (int i = 0; i < 256; i++) {
            NEMessage* theMore = new TGame((unsigned char)i);
            long theResult = AddInput(theTick, theMore);
            if (theResult != 0 || i >= 250) {
                Line("  add %d %ld count %lu", i, theResult, InputCount(theTick));
                if (theResult != 0)
                    theMore->Dereference();
            }
        }
        Line("  get 254 %ld get 255 %ld", GetInput(theTick, 254, theInput), GetInput(theTick, 255, theInput));
        Line("  size %lu", theTick->ExportSize());
        Line("  free the tick");
        theTick->Dereference();
    }
}

static void Factory()
{
    Line("factory");
    for (int t = 0x1f; t <= 0x42; t++) {
        unsigned char theData[0x420] = {0};
        theData[1] = (unsigned char)t;
        NEMessage* theMessage = NEMessage::ConvertMemoryToMessage(theData);
        Line("  convert %02x: %s", t, theMessage == NULL ? "none" : theMessage->GetMessageLabel());
        if (theMessage != NULL)
            theMessage->Dereference();
    }
    for (int t : {0x2c, 0x3d, 0x41}) {
        unsigned char theData[0x420] = {0};
        theData[1] = (unsigned char)t;
        NEMessage* theMessage = BuildEngine(theData);
        Line("  build %02x: %s", t, theMessage == NULL ? "none" : theMessage->GetMessageLabel());
        if (theMessage != NULL)
            theMessage->Dereference();
    }
}

int main(int argc, char** argv)
{
    const char* thePrint = NULL;
    const char* theGolden = NULL;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--print") == 0 && i + 1 < argc)
            thePrint = argv[++i];
        else if (strcmp(argv[i], "--golden") == 0 && i + 1 < argc)
            theGolden = argv[++i];
    }
    Line("before registration");
    unsigned char theData[16] = {0, 0x26};
    Line("  convert 26: %s", NEMessage::ConvertMemoryToMessage(theData) == NULL ? "none" : "built");
    RegisterGame();
    Senders();
    Responses();
    PlayerInputs();
    Factory();
    printf("ne_message_test: round 7 API: %d checks, %d failed\n", sRustChecks, sRustFailures);
    if (sRustFailures != 0)
        return 1;
    if (thePrint != NULL) {
        FILE* theFile = fopen(thePrint, "wb");
        fwrite(sTrace.data(), 1, sTrace.size(), theFile);
        fclose(theFile);
    }
    size_t theLines = 0;
    for (char c : sTrace)
        theLines += c == '\n';
    std::string theDefault = __FILE__;
    theDefault = theDefault.substr(0, theDefault.find_last_of("/\\") + 1) + "golden_message.txt";
    if (theGolden == NULL && thePrint == NULL)
        theGolden = theDefault.c_str();
    if (theGolden == NULL) {
        printf("ne_message_test: %zu trace lines written\n", theLines);
        return 0;
    }
    FILE* theFile = fopen(theGolden, "rb");
    if (theFile == NULL) {
        printf("ne_message_test: no golden %s\n", theGolden);
        return 1;
    }
    std::string theWant;
    char theChunk[65536];
    size_t n;
    while ((n = fread(theChunk, 1, sizeof(theChunk), theFile)) > 0)
        theWant.append(theChunk, n);
    fclose(theFile);
    if (theWant == sTrace) {
        printf("ne_message_test: %zu trace lines, equal to the golden\n", theLines);
        return 0;
    }
    size_t i = 0;
    while (i < theWant.size() && i < sTrace.size() && theWant[i] == sTrace[i])
        i++;
    size_t theStart = theWant.rfind('\n', i == 0 ? 0 : i - 1);
    theStart = theStart == std::string::npos ? 0 : theStart + 1;
    printf("ne_message_test: differs from the golden at byte %zu:\n  golden: %.400s\n  now:    %.400s\n", i,
           theWant.c_str() + theStart, sTrace.c_str() + theStart);
    return 1;
}
