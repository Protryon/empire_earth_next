// C:\EEX\Low-Level Engine\Network Engine\TCP Protocol\TCP Protocol.cpp
// The TCP/IP network plug-in's shim: the plug-in itself is Rust (rust/crates/ee_net_tcp). This file keeps the NPIM*
// entry points the engine looks up by name (their table is PIMPlugin below), converts UStrings and TSEvents, and
// holds what is engine data: the protocol's configuration object and PIMGetName's string.
#include <string.h>
#include <string>
#include <vector>

#include "platform/wintypes.h"   // the Windows types the engine headers use (on Windows <windows.h>, as before)
#include "platform/PlatSocket.h"
#include "platform/PlatSim.h"
#include "platform/PlatThread.h"

#include "lle/LowLevelEngine.h"
#include "lle/PIMModule.h"
#include "rust/ee_rust.h"

NEProtocolConfiguration gConfiguration(0, 1);    // 0x72d8
UString gProtocolName("TCP/IP Protocol");        // 0x72a8
unsigned long gProtocolKind = 1;                 // 0x7020 (NPIMGetProtocolType)
unsigned long gProtocolType = 'ntwk';            // 0x7024

// ---- the engine calls Rust needs (extern "C" so Rust can link them)

extern "C" void ee_tcp_host_record_native_error(uint32_t inError)
{
    ESRecordNativeError(inError);
}
extern "C" uint32_t ee_tcp_host_native_error()
{
    return (uint32_t)ESGetNativeError();
}
extern "C" uint32_t ee_tcp_host_time_ms()
{
    return (uint32_t)TMGetTime();
}
extern "C" int32_t ee_tcp_host_port_offset()
{
    return gPlatSimPortOffset;
}

static const uint8_t* Bytes(const char* inText)
{
    return reinterpret_cast<const uint8_t*>(inText);
}
static const uint8_t* Bytes(const UString& inString)
{
    return Bytes(static_cast<const char*>(inString));
}
static const uint8_t* Bytes(const UWideString& inString)
{
    return Bytes(static_cast<const char*>(inString));
}
static size_t Length(const UWideString& inString)
{
    return strlen(static_cast<const char*>(inString));
}
static void ToUString(void* inContext, const uint8_t* inBytes, size_t inLength)
{
    *static_cast<UString*>(inContext) = std::string(reinterpret_cast<const char*>(inBytes), inLength).c_str();
}
static void ToUWide(void* inContext, const uint8_t* inBytes, size_t inLength)
{
    *static_cast<UWideString*>(inContext) = std::string(reinterpret_cast<const char*>(inBytes), inLength).c_str();
}
static PlatEvent* Handle(TSEvent* inEvent)
{
    return inEvent->mHandle;
}

extern "C" void NPIMAllowNewConnections(void** inHandle, bool inAllow)
{
    ee_tcp_allow_new_connections(inHandle, inAllow);
}
extern "C" long NPIMAccept(void** inHandle, void** outConnection)
{
    return ee_tcp_accept(inHandle, outConnection);
}
extern "C" long NPIMAcceptForXfer(void** inHandle, void** outConnection)
{
    return ee_tcp_accept_for_xfer(inHandle, outConnection);
}
extern "C" long NPIMActivate()
{
    return ee_tcp_activate();
}
extern "C" long NPIMCloseConnection(void** ioConnection)
{
    return ee_tcp_close_connection(ioConnection);
}
extern "C" long NPIMCloseServer(void** ioServer)
{
    return ee_tcp_close_server(ioServer);
}
extern "C" long NPIMConnect(const UString& inName, const UString& inHost, const UWideString& inAddress, long inType,
                            TSEvent* inEvent, void** outConnection, unsigned long inAdapter)
{
    return ee_tcp_connect(Bytes(inName), inName.GetLength(), Bytes(inHost), inHost.GetLength(), Bytes(inAddress),
                          Length(inAddress), (int32_t)inType, Handle(inEvent), outConnection, (uint32_t)inAdapter);
}
extern "C" long NPIMConnectForXfer(void** inHost, long inMode, TSEvent* inEvent, void** outConnection)
{
    return ee_tcp_connect_for_xfer(inHost, (int32_t)inMode, Handle(inEvent), outConnection);
}
extern "C" long NPIMDeactivate()
{
    return ee_tcp_deactivate();
}
extern "C" long NPIMEnumerateNetworkAdapters(std::vector<UWideString>& outAdapters)
{
    outAdapters.clear();
    struct Lambda {
        static void Add(void* inContext, const uint8_t* inBytes, size_t inLength)
        {
            static_cast<std::vector<UWideString>*>(inContext)->push_back(
                UWideString(std::string(reinterpret_cast<const char*>(inBytes), inLength).c_str()));
        }
    };
    return ee_tcp_enumerate_adapters(Lambda::Add, &outAdapters);
}
extern "C" long NPIMGetClientAddress(void** inHandle, unsigned long inIndex, UWideString& outAddress)
{
    return ee_tcp_get_client_address(inHandle, (uint32_t)inIndex, ToUWide, &outAddress);
}
extern "C" long NPIMGetConfiguration(NEProtocolConfiguration** outConfiguration)
{
    *outConfiguration = new NEProtocolConfiguration(gConfiguration);
    return *outConfiguration != NULL ? 0 : 8;
}
extern "C" long NPIMGetDetectedGame(unsigned long inIndex, UString& outName, UString& outHost,
                                    UWideString& outAddress)
{
    return ee_tcp_get_detected_game((uint32_t)inIndex, ToUString, &outName, ToUString, &outHost, ToUWide, &outAddress);
}
extern "C" long NPIMGetDetectedGameCount()
{
    return ee_tcp_get_detected_game_count();
}
extern "C" const UWideString& NPIMGetLocalAddress(unsigned long inAdapter)
{
    static UWideString sAddress;
    ee_tcp_get_local_address((uint32_t)inAdapter, ToUWide, &sAddress);
    return sAddress;
}
extern "C" unsigned long NPIMGetProtocolType()
{
    return gProtocolKind;
}
extern "C" long NPIMGetStatus(const UWideString& inName, unsigned long inPort)
{
    return ee_tcp_get_status();
}
extern "C" bool NPIMIsAlive(void** inHandle)
{
    return ee_tcp_is_alive(inHandle);
}
extern "C" long NPIMPublish(const UString& inName, const UWideString& inAddress, unsigned long inFlags,
                            TSEvent* inDataEvent, TSEvent* inListenEvent, TSEvent* inAcceptEvent, void** outHandle,
                            bool inFlag55, bool inFlag56, unsigned long inAdapter)
{
    return ee_tcp_publish(Bytes(inName), inName.GetLength(), Bytes(inAddress), Length(inAddress), (uint32_t)inFlags,
                          Handle(inDataEvent), Handle(inListenEvent), Handle(inAcceptEvent), outHandle, inFlag55,
                          inFlag56, (uint32_t)inAdapter);
}
extern "C" long NPIMReceive(void** inHandle, unsigned long inSize, char* outBuffer, unsigned long* outReceived)
{
    uint32_t theReceived = (uint32_t)*outReceived;
    long theResult = ee_tcp_receive(inHandle, (uint32_t)inSize, reinterpret_cast<uint8_t*>(outBuffer), &theReceived);
    *outReceived = theReceived;
    return theResult;
}
extern "C" long NPIMReceiveFrom(void** inHandle, unsigned long inSize, char* outBuffer, unsigned long* outReceived,
                                unsigned long* outIndex)
{
    uint32_t theReceived = (uint32_t)*outReceived;
    uint32_t theIndex = (uint32_t)*outIndex;
    long theResult = ee_tcp_receive_from(inHandle, (uint32_t)inSize, reinterpret_cast<uint8_t*>(outBuffer),
                                         &theReceived, &theIndex);
    *outReceived = theReceived;
    *outIndex = theIndex;
    return theResult;
}
extern "C" void NPIMResetClientConnection(void** inHandle, unsigned long inIndex)
{
    ee_tcp_reset_client_connection(inHandle, (uint32_t)inIndex);
}
extern "C" void NPIMResetXfer(void** inHandle)
{
    ee_tcp_reset_xfer(inHandle);
}
extern "C" long NPIMSearchForGame(const UString& inHost, const UString& inName, bool inStart)
{
    return ee_tcp_search_for_game(Bytes(inName), inName.GetLength(), inStart);
}
extern "C" long NPIMSend(void** inHandle, char* ioData, unsigned long inLength)
{
    return ee_tcp_send(inHandle, reinterpret_cast<uint8_t*>(ioData), (uint32_t)inLength);
}
extern "C" long NPIMSendSingle(const UString& inHost, unsigned long inPort, char* inData, unsigned long inLength)
{
    return ee_tcp_send_single(Bytes(inHost), inHost.GetLength(), (uint32_t)inPort,
                              reinterpret_cast<const uint8_t*>(inData), (uint32_t)inLength);
}
extern "C" long NPIMSendTo(void** inHandle, unsigned long inIndex, char* ioData, unsigned long inLength)
{
    return ee_tcp_send_to(inHandle, (uint32_t)inIndex, reinterpret_cast<uint8_t*>(ioData), (uint32_t)inLength);
}
extern "C" long NPIMSetConfiguration(const NEProtocolConfiguration* inConfiguration)
{
    if (inConfiguration->GetProtocolType() == gConfiguration.GetProtocolType())
        return inConfiguration->GetParameterCount() == gConfiguration.GetParameterCount() ? 0 : 9;
    return 9;
}
extern "C" long NPIMXferReceive(unsigned long inSize, char* outBuffer, unsigned long* outReceived)
{
    uint32_t theReceived = (uint32_t)*outReceived;
    long theResult = ee_tcp_xfer_receive((uint32_t)inSize, reinterpret_cast<uint8_t*>(outBuffer), &theReceived);
    *outReceived = theReceived;
    return theResult;
}
extern "C" long NPIMXferSendTo(void** inHandle, unsigned long inIndex, char* inData, unsigned long inLength)
{
    return ee_tcp_xfer_send_to(inHandle, (uint32_t)inIndex, reinterpret_cast<const uint8_t*>(inData), (uint32_t)inLength);
}

static const UString* PIMGetName()
{
    return &gProtocolName;
}
static unsigned long PIMGetType()
{
    return gProtocolType;
}
static bool PIMIsValid()
{
    return ee_tcp_is_valid();
}

// The plug-in's entry points (its DLL's export table before; registered in PIMModule.cpp)
static const PIMEntry sEntries[] = {
    PIM_ENTRY(PIMGetName),
    PIM_ENTRY(PIMGetType),
    PIM_ENTRY(PIMIsValid),
    PIM_ENTRY(NPIMAccept),
    PIM_ENTRY(NPIMAcceptForXfer),
    PIM_ENTRY(NPIMActivate),
    PIM_ENTRY(NPIMAllowNewConnections),
    PIM_ENTRY(NPIMCloseConnection),
    PIM_ENTRY(NPIMCloseServer),
    PIM_ENTRY(NPIMConnect),
    PIM_ENTRY(NPIMConnectForXfer),
    PIM_ENTRY(NPIMDeactivate),
    PIM_ENTRY(NPIMEnumerateNetworkAdapters),
    PIM_ENTRY(NPIMGetClientAddress),
    PIM_ENTRY(NPIMGetConfiguration),
    PIM_ENTRY(NPIMGetDetectedGame),
    PIM_ENTRY(NPIMGetDetectedGameCount),
    PIM_ENTRY(NPIMGetLocalAddress),
    PIM_ENTRY(NPIMGetProtocolType),
    PIM_ENTRY(NPIMGetStatus),
    PIM_ENTRY(NPIMIsAlive),
    PIM_ENTRY(NPIMPublish),
    PIM_ENTRY(NPIMReceive),
    PIM_ENTRY(NPIMReceiveFrom),
    PIM_ENTRY(NPIMResetClientConnection),
    PIM_ENTRY(NPIMResetXfer),
    PIM_ENTRY(NPIMSearchForGame),
    PIM_ENTRY(NPIMSend),
    PIM_ENTRY(NPIMSendSingle),
    PIM_ENTRY(NPIMSendTo),
    PIM_ENTRY(NPIMSetConfiguration),
    PIM_ENTRY(NPIMXferReceive),
    PIM_ENTRY(NPIMXferSendTo),
    { NULL, NULL },
};
extern const PIMPlugin gPIMPluginTCPProtocol = { "TCP Protocol.dll", sEntries };
