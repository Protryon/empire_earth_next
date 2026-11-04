// C:\EEX\Low-Level Engine\Network Engine\TCP Protocol\TCP Protocol.cpp
// Unit flags (pinned, docs/CALIBRATION.md): -O1 -G6 -MD
#include <winsock2.h>  // includes <windows.h>; must precede it (winsock.h clash)
#include <iphlpapi.h>
#include <vector>
#include <float.h>

#include "lle/LowLevelEngine.h"
#include "LostDebug.h"  // NOT original source: EE_LOST_DEBUG filler (ResolveAddress; docs/LOST_DEBUG.md)
#include "TCPPacketType.h"
// ---- Result codes returned by the NPIM entry points (provisional names); values proven by codegen.
enum {
    kNPIMOK = 0,
    kNPIMNativeError = 1,
    kNPIMBadIndex = 3,
    kNPIMWouldBlock = 7,
    kNPIMOutOfMemory = 8,
    kNPIMUnavailable = 9
};

// ---- Unit-local types. Layouts: proven offsets only, the rest padded.
struct TCPConnection {                    // proven sizeof 0x18 (operator new(0x18) in NPIMAccept)
    SOCKET mSocket;                       // proven +0x00
    sockaddr_in mAddress;                 // proven +0x04 (memcpy 0x10 in NPIMConnectForXfer)
    long mState;                          // proven +0x14 (1 = connected)
};

struct TCPHost {
    unsigned char _provisional_pad_00[0x08];
    unsigned long mAddress;               // proven +0x08 (sin_addr in NPIMConnectForXfer)
};

struct DetectedGame {                     // proven sizeof 0x54 (vector size() divides by 0x54)
    DetectedGame()                        // inline; emitted out of line at 0x3178 (QueryGames local, resize)
        : mName(0x40), mHost(0x40), mAddress(0x40)
    {
        memset(&this->mStreamAddress, 0, sizeof(this->mStreamAddress));
        memset(&this->mDatagramAddress, 0, sizeof(this->mDatagramAddress));
    }

    sockaddr_in mStreamAddress;           // proven +0x00 (copied for SOCK_STREAM in 0x1714)
    sockaddr_in mDatagramAddress;         // proven +0x10 (copied otherwise)
    UString mName;                        // proven +0x20 (NPIMGetDetectedGame)
    UString mHost;                        // proven +0x30
    UWideString mAddress;                 // proven +0x40
};

// Element types proven by NPIMReceiveFrom (0x2539): it calls two distinct push_back instantiations, 0x32d1 on
// mClients and 0x32fd on mConnections (byte-identical twins that survived single-pass /OPT:ICF). Other
// members (~vector 0x34ef, ...) were folded, so their names in names.txt are representatives only.
// NEProtocol::Publish passes an unsigned long; the plugin reads it as bitfields (and eax,1; cmp al,1 on the
// by-value parameter) and copies it whole into the server (mov [esi+0x28],eax in NPIMPublish).
struct TCPServerFlags {
    unsigned long mListening : 1;         // proven bit 0 (and ecx,1; cmp cl,1): stream socket
    unsigned long mXfer : 1;              // proven bit 1 (and eax,2; cmp al,2): datagram socket
};

struct TCPServer {
    TCPServer();                          // out of line, defined after NPIMPublish (0x211a)
    ~TCPServer();                         // out of line, defined after NPIMCloseServer (0x14c3)

    SOCKET mSocket;                       // proven +0x00 (closed when mListening)
    SOCKET mXferSocket;                   // proven +0x04 (closed when mXfer)
    std::vector<sockaddr_in*> mClients;   // proven +0x08: one address per client slot
    std::vector<TCPConnection*> mConnections; // proven +0x18: one xfer connection per client slot
    TCPServerFlags mFlags;                // proven +0x28
    UString mName;                        // proven +0x2c (dtor)
    UWideString mAddress;                 // proven +0x3c (dtor)
    unsigned long mPort;                  // proven +0x50 (NPIMPublish: port from getsockname)
    bool mAllowNewConnections;            // proven +0x54
    bool mFlag55;                         // proven +0x55 (answer queries only when false; role unknown)
    bool mAnnounce;                       // proven +0x56 (answer queries only when true; role unknown)
};

// Discovery packets (NPIMReceiveFrom answers a query with an announcement; NPIMReceive filters both).
// Byte 0 is the cipher key, the rest is enciphered.
struct GameQuery {                        // proven sizeof 0x43 (recvfrom length check)
    unsigned char mKey;
    unsigned char mType;                  // 0x15
    char mName[0x41];                     // proven +0x02 (strcmp with the server name)
};

struct GameAnnouncement {                 // proven sizeof 0x88 (sendto length)
    unsigned char mKey;                   // proven +0x00 (memcpy of the key)
    unsigned char mType;                  // proven +0x01: 0x16
    char mName[0x41];                     // proven +0x02 (strcpy server name)
    char mAddress[0x45];                  // proven +0x43 (strcpy server address)
};

// ---- Entry points defined below and called before their definition. The original most likely declared every
// NPIM entry point in a shared protocol-interface header (inferred).
extern "C" long NPIMGetStatus(const UWideString& inName, unsigned long inPort);
static long ResolveAddress(const UString& inName, const UString& inHost, const UWideString& inAddress,
                           long inType, SOCKET* outSocket, sockaddr_in* outAddress);
static unsigned long FindAdapterAddressIndex(unsigned long inAdapter);
static void PrepareCipher(unsigned long inKeyOffset, unsigned long inKeyLength);
static void Encipher(unsigned char* ioData, long inLength, unsigned long inKey);
static void swap_byte(unsigned char* a, unsigned char* b);   // defined at the end (classic RC4 rc4.c layout)
static long CreateSocket(long inType, SOCKET* outSocket, unsigned long* ioPort);
static long QueryGames(const UString& inName, hostent* inHost);
static unsigned long Searcher(void* inService);           // 0x31da (names from assert text)

// Compiler-generated for this unit (names in analysis/tcp/names.txt):
// TEMPLATE: TCP 0x100041c5
// TEMPLATE: TCP 0x10004275
// SYNTHETIC: TCP 0x100044aa
// SYNTHETIC: TCP 0x100044bb
// SYNTHETIC: TCP 0x100040e1
// SYNTHETIC: TCP 0x100040e5
// SYNTHETIC: TCP 0x10004108
// SYNTHETIC: TCP 0x10004109
// TEMPLATE: TCP 0x10004126
// SYNTHETIC: TCP 0x1000416d
// SYNTHETIC: TCP 0x1000417a
// SYNTHETIC: TCP 0x10004183
// TEMPLATE: TCP 0x100041f0
// TEMPLATE: TCP 0x10003565
// TEMPLATE: TCP 0x10003621
// TEMPLATE: TCP 0x10003750
// SYNTHETIC: TCP 0x10004097
// TEMPLATE: TCP 0x10004261
// TEMPLATE: TCP 0x100043b4
// TEMPLATE: TCP 0x100043e0
// TEMPLATE: TCP 0x100043fd
// TEMPLATE: TCP 0x10003678
// SYNTHETIC: TCP 0x100029b5
// TEMPLATE: TCP 0x10004306
// SYNTHETIC: TCP 0x100044ea
// TEMPLATE: TCP 0x100034b6
// TEMPLATE: TCP 0x10003abd
// TEMPLATE: TCP 0x100041b1
// TEMPLATE: TCP 0x1000434f
// TEMPLATE: TCP 0x10004374
// TEMPLATE: TCP 0x1000439c
// TEMPLATE: TCP 0x10004082
// TEMPLATE: TCP 0x100042a6
// TEMPLATE: TCP 0x10004429
// TEMPLATE: TCP 0x1000445f
// TEMPLATE: TCP 0x1000447d
// TEMPLATE: TCP 0x100037bc
// TEMPLATE: TCP 0x1000432a
// SYNTHETIC: TCP 0x100044cd
// SYNTHETIC: TCP 0x10004534
// SYNTHETIC: TCP 0x10001000
// SYNTHETIC: TCP 0x1000100a
// SYNTHETIC: TCP 0x10001022
// SYNTHETIC: TCP 0x1000102e
// SYNTHETIC: TCP 0x10001038
// SYNTHETIC: TCP 0x10001042
// SYNTHETIC: TCP 0x1000105a
// SYNTHETIC: TCP 0x10001066
// SYNTHETIC: TCP 0x10001070
// SYNTHETIC: TCP 0x1000107a
// SYNTHETIC: TCP 0x1000108a
// SYNTHETIC: TCP 0x10001096
// SYNTHETIC: TCP 0x100010a1
// SYNTHETIC: TCP 0x100010ab
// SYNTHETIC: TCP 0x100010bc
// SYNTHETIC: TCP 0x100010c8
// SYNTHETIC: TCP 0x100031ba
// TEMPLATE: TCP 0x10003329
// TEMPLATE: TCP 0x10003364
// TEMPLATE: TCP 0x10003370
// TEMPLATE: TCP 0x10003514
// TEMPLATE: TCP 0x10003558
// TEMPLATE: TCP 0x100036e1
// TEMPLATE: TCP 0x10003718
// TEMPLATE: TCP 0x100042e6
// TEMPLATE: TCP 0x100034ef
// SYNTHETIC: TCP 0x10001d4f
// TEMPLATE: TCP 0x100033ff
// TEMPLATE: TCP 0x1000342b
// TEMPLATE: TCP 0x100032d1
// TEMPLATE: TCP 0x100032fd
// TEMPLATE: TCP 0x10003466
// TEMPLATE: TCP 0x10003827
// SYNTHETIC: TCP 0x10003178
// TEMPLATE: TCP 0x1000337c
// TEMPLATE: TCP 0x10003398
// TEMPLATE: TCP 0x10003acf
// TEMPLATE: TCP 0x100042bd
// TEMPLATE: TCP 0x100033c4
// TEMPLATE: TCP 0x10003762
// TEMPLATE: TCP 0x10003da6

// ---- Globals in definition order (= dynamic initializer order 0x1000, 0x1038, 0x1070, 0x10a1).
// Addresses in analysis/tcp/names.txt (provisional names).
std::vector<DetectedGame> gDetectedGames;        // 0x7288
std::vector<DetectedGame> gDetectedGames2;       // 0x7270 (role unknown)
NEProtocolConfiguration gConfiguration(0, 1);    // 0x72d8
UString gProtocolName("TCP/IP Protocol");        // 0x72a8
unsigned long gProtocolKind = 1;                 // 0x7020 (NPIMGetProtocolType)
unsigned long gProtocolType = 'ntwk';            // 0x7024
bool gValid;                                     // 0x72f8
SOCKET gXferSocket;                              // 0x72d4
SOCKET gSocket7280;                              // 0x7280 (role unknown)
TSCriticalSection* gDetectedGamesLock;           // 0x726c
TSCriticalSection* gSocketLock;                    // 0x729c (role unknown)
TSCriticalSection* gCipherLock;                    // 0x7298 (role unknown)
TSCriticalSection* gXferLock;                    // 0x72cc (role unknown)
struct RC4State {                                // proven sizeof 0x102 (operator new in NPIMActivate)
    unsigned char mState[256];                   // proven: identity-filled + swapped (0x24a8, 0x23a7)
    unsigned char mX;                            // proven +0x100
    unsigned char mY;                            // proven +0x101
};
RC4State* gCipher;                       // 0x72e8
char* gRandomBuffer;                             // 0x72ec
URandom* gRandom;                                // 0x72a0
unsigned long gAdapter;                          // 0x72f0 (NPIMConnect)
unsigned long gAdapterAddressIndex;              // 0x72f4 (NPIMConnect)
TSEvent* gSearcherThreadFinishedEvent;           // 0x72e4 (name from assert text, 0x287c)
TSEvent* gSearcherThreadExitEvent;               // 0x7268 (name from assert text)
TSThread* gSearcherThread;                       // 0x72a4 (name from assert text)
bool gSearcherRunning;                           // 0x72f9 (provisional name)

const unsigned long kRandomBufferSize = 0x105;

// FUNCTION: TCP 0x100010d3
// DLL entry point; does nothing and reports success
BOOL WINAPI DllMain(HINSTANCE inInstance, DWORD inReason, LPVOID inReserved)
{
    return TRUE;
}

// FUNCTION: TCP 0x100010d9
// Sets whether the server accepts new clients (announces itself to queries and adds unknown senders)
extern "C" void NPIMAllowNewConnections(TCPServer** inHandle, bool inAllow)
{
    (*inHandle)->mAllowNewConnections = inAllow;
}

// FUNCTION: TCP 0x100010e7
// Accepts a pending stream connection on a listening server; would-block or native error when none is waiting
extern "C" long NPIMAccept(TCPServer** inHandle, TCPConnection** outConnection)
{
    TCPServer* theServer = *inHandle;
    if (theServer->mFlags.mListening == 1) {
        SOCKET theSocket = accept(theServer->mSocket, NULL, NULL);
        if (theSocket != INVALID_SOCKET) {
            TCPConnection* theConnection = new TCPConnection;
            theConnection->mSocket = theSocket;
            theConnection->mState = 1;
            *outConnection = theConnection;
            return kNPIMOK;
        }
        int theError = WSAGetLastError();
        ESRecordNativeError(theError);
        return theError == WSAEWOULDBLOCK ? kNPIMWouldBlock : kNPIMNativeError;
    }
    return kNPIMUnavailable;
}

// FUNCTION: TCP 0x1000114e
// Accepts a file-transfer connection on the shared xfer listen socket and binds it to the matching client slot
// (13 when the sender is no known client)
extern "C" long NPIMAcceptForXfer(TCPServer** inHandle, TCPConnection** outConnection)
{
    long theResult;
    sockaddr_in theAddress;
    int theLength = sizeof(theAddress);
    gXferLock->Enter();
    SOCKET theSocket = accept(gSocket7280, (sockaddr*)&theAddress, &theLength);
    if (theSocket != INVALID_SOCKET) {
        TCPServer* theServer = *inHandle;
        unsigned long i;
        for (i = 0; i < theServer->mClients.size(); i++) {
            sockaddr_in* theClient = theServer->mClients[i];
            if (theAddress.sin_family == theClient->sin_family && theClient->sin_port != 0
                && theAddress.sin_addr.s_addr == theClient->sin_addr.s_addr)
                break;
        }
        if (i == theServer->mClients.size()) {
            closesocket(theSocket);
            theResult = 13;
        } else {
            TCPConnection* theConnection = new TCPConnection;
            theConnection->mSocket = theSocket;
            theConnection->mState = 1;
            *outConnection = theConnection;
            theServer->mConnections[i] = theConnection;
            theResult = kNPIMOK;
        }
    } else {
        int theError = WSAGetLastError();
        ESRecordNativeError(theError);
        theResult = theError == WSAEWOULDBLOCK ? kNPIMWouldBlock : kNPIMNativeError;
    }
    gXferLock->Leave();
    return theResult;
}

// FUNCTION: TCP 0x10001238
// Starts WinSock 2.0, creates the four locks, the cipher state and the random generator, then fills the key buffer
extern "C" long NPIMActivate()
{
    long theResult;
    WSADATA theData;
    if (WSAStartup(MAKEWORD(2, 0), &theData) == 0) {
        SOCKET theSocket;
        if (LOBYTE(theData.wVersion) == 2 && HIBYTE(theData.wVersion) == 0
            && (theSocket = socket(AF_INET, SOCK_STREAM, 0)) != INVALID_SOCKET) {
            closesocket(theSocket);
            gValid = true;
            theResult = TSScheduler::CreateCriticalSection(gDetectedGamesLock);
            ESVERIFY_AT(theResult == keseOK, 409);
            theResult = TSScheduler::CreateCriticalSection(gSocketLock);
            ESVERIFY_AT(theResult == keseOK, 411);
            theResult = TSScheduler::CreateCriticalSection(gCipherLock);
            ESVERIFY_AT(theResult == keseOK, 413);
            theResult = TSScheduler::CreateCriticalSection(gXferLock);
            ESVERIFY_AT(theResult == keseOK, 415);
            gCipher = new RC4State;
            gRandom = new URandom(TMGetTime());
            theResult = NPIMGetStatus(UWideString("WinSock 2.0"), 0);
        } else {
            theResult = kNPIMUnavailable;
        }
    } else {
        ESRecordNativeError(WSAGetLastError());
        theResult = kNPIMNativeError;
    }
    return theResult;
}

// FUNCTION: TCP 0x100013a6
// Closes and frees a connection, then also closes the shared xfer listen socket; native error if either close fails
extern "C" long NPIMCloseConnection(TCPConnection** ioConnection)
{
    long theResult;
    TCPConnection* theConnection = *ioConnection;
    if (theConnection != NULL && closesocket(theConnection->mSocket) == 0) {
        delete theConnection;
        *ioConnection = NULL;
        theResult = kNPIMOK;
    } else {
        ESRecordNativeError(WSAGetLastError());
        ESGetNativeError();
        theResult = kNPIMNativeError;
    }
    if (closesocket(gSocket7280) != 0) {
        ESRecordNativeError(WSAGetLastError());
        ESGetNativeError();
        theResult = kNPIMNativeError;
    }
    return theResult;
}

// FUNCTION: TCP 0x1000140e
// Closes the server's stream, datagram and xfer listen sockets; frees the server only when every close succeeded
extern "C" long NPIMCloseServer(TCPServer** ioServer)
{
    long theResult = kNPIMOK;
    TCPServer* theServer = *ioServer;
    if (theServer->mFlags.mListening == 1 && closesocket(theServer->mSocket) != 0) {
        ESRecordNativeError(WSAGetLastError());
        ESGetNativeError();
        theResult = kNPIMNativeError;
    }
    if (theServer->mFlags.mXfer == 1 && closesocket(theServer->mXferSocket) != 0) {
        ESRecordNativeError(WSAGetLastError());
        ESGetNativeError();
        theResult = kNPIMNativeError;
    }
    if (closesocket(gSocket7280) != 0) {
        ESRecordNativeError(WSAGetLastError());
        ESGetNativeError();
        theResult = kNPIMNativeError;
    }
    if (theResult == kNPIMOK) {
        delete theServer;
        *ioServer = NULL;
    }
    return theResult;
}

// FUNCTION: TCP 0x100014c3
// Destroys the server, freeing every client address and xfer connection it holds
TCPServer::~TCPServer()
{
    unsigned long i;
    for (i = 0; i < this->mClients.size(); i++)
        delete this->mClients[i];
    for (i = 0; i < this->mConnections.size(); i++)
        delete this->mConnections[i];
}

// FUNCTION: TCP 0x1000153c
// Opens a connection to a game (stream or datagram) on the chosen adapter
extern "C" long NPIMConnect(const UString& inName, const UString& inHost, const UWideString& inAddress,
                            long inType, WSAEVENT* inEvent, TCPConnection** outConnection,
                            unsigned long inAdapter)
{
    long theResult;
    if (inType != 0) {
        gAdapter = inAdapter;
        gAdapterAddressIndex = FindAdapterAddressIndex(inAdapter);
        SOCKET theSocket;
        sockaddr_in theAddress;
        theResult = ResolveAddress(inName, inHost, inAddress, inType, &theSocket, &theAddress);
        if (theResult == kNPIMOK) {
            if (inType == 1 && connect(theSocket, (sockaddr*)&theAddress, sizeof(theAddress)) != 0) {
                ESRecordNativeError(WSAGetLastError());
                ESGetNativeError();
                theResult = kNPIMNativeError;
                closesocket(theSocket);
            } else {
                sockaddr_in theLocal;
                memset(&theLocal, 0, sizeof(theLocal));
                theLocal.sin_port = 0;
                theLocal.sin_family = AF_INET;
                gSocketLock->Enter();
                if (bind(theSocket, (sockaddr*)&theLocal, sizeof(theLocal)) == 0) {
                    WSAEventSelect(theSocket, *inEvent, FD_READ);
                    TCPConnection* theConnection = new TCPConnection;
                    theConnection->mState = inType;
                    theConnection->mSocket = theSocket;
                    memcpy(&theConnection->mAddress, &theAddress, sizeof(theAddress));
                    *outConnection = theConnection;
                    theResult = kNPIMOK;
                } else {
                    // Fix: a failed bind used to return success with no connection and leak the socket.
                    ESRecordNativeError(WSAGetLastError());
                    theResult = kNPIMNativeError;
                    closesocket(theSocket);
                }
                gSocketLock->Leave();
            }
        }
    } else {
        theResult = kNPIMUnavailable;
    }
    return theResult;
}

// FUNCTION: TCP 0x1000164d
// Opens the file-transfer stream connection to the host on port 33335 (13 for a bad mode or no host)
extern "C" long NPIMConnectForXfer(TCPHost** inHost, long inMode, WSAEVENT* inEvent,
                                   TCPConnection** outConnection)
{
    TCPHost* theHost = *inHost;
    if (inMode != 1 || theHost == NULL)
        return 13;
    gXferSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (gXferSocket != INVALID_SOCKET) {
        sockaddr_in theAddress;
        theAddress.sin_family = AF_INET;
        theAddress.sin_port = htons(33335);
        theAddress.sin_addr.s_addr = theHost->mAddress;
        if (connect(gXferSocket, (sockaddr*)&theAddress, sizeof(theAddress)) != SOCKET_ERROR) {
            TCPConnection* theConnection = new TCPConnection;
            theConnection->mState = 1;
            theConnection->mSocket = gXferSocket;
            memcpy(&theConnection->mAddress, &theAddress, sizeof(theAddress));
            *outConnection = theConnection;
            WSAEventSelect(gXferSocket, *inEvent, FD_READ);
            return kNPIMOK;
        }
    }
    ESRecordNativeError(WSAGetLastError());
    ESGetNativeError();
    return kNPIMNativeError;
    return 13;
}

// FUNCTION: TCP 0x10001714
// Creates a socket and resolves the game's address: from the detected-games list when any exist, else by DNS
static long ResolveAddress(const UString& inName, const UString& inHost, const UWideString& inAddress,
                           long inType, SOCKET* outSocket, sockaddr_in* outAddress)
{
    long theResult;
    // LOST-DEBUG: every ESVERIFY __LINE__ of the AoC build is 11 over the base-game twin's (the 11 lines fall before
    // NPIMActivate), and the twin's identical ResolveAddress inlines all five _Ptrit::operator+ that AoC calls out
    // of line: the AoC build carried inline cost this source does not show. n = 9 (exact for 9..13; docs/LOST_DEBUG.md)
    EE_LOST_DEBUG(9);
    gDetectedGamesLock->Enter();
    SOCKET theSocket = socket(AF_INET, inType == 1 ? SOCK_STREAM : SOCK_DGRAM, 0);
    if (theSocket != INVALID_SOCKET) {
        if (0 < gDetectedGames.size()) {
            long theIndex = -1;
            for (unsigned long i = 0; i < gDetectedGames.size(); i++) {
                if (inName == gDetectedGames[i].mName && inHost == gDetectedGames[i].mHost
                    && inAddress == gDetectedGames[i].mAddress) {
                    theIndex = i;
                    break;
                }
            }
            if (theIndex != -1) {
                if (inType == 1)
                    memcpy(outAddress, &gDetectedGames[theIndex].mStreamAddress, sizeof(sockaddr_in));
                else
                    memcpy(outAddress, &gDetectedGames[theIndex].mDatagramAddress, sizeof(sockaddr_in));
                outAddress->sin_port = htons(33334);
                *outSocket = theSocket;
                theResult = kNPIMOK;
            } else {
                theResult = kNPIMBadIndex;
                closesocket(theSocket);
            }
        } else {
            hostent* theHost = gethostbyname(inName);
            if (theHost == NULL) {
                unsigned long theAddress = inet_addr(inName);
                if (theAddress == INADDR_NONE)
                    theHost = NULL;
                else
                    theHost = gethostbyaddr(reinterpret_cast<char*>(&theAddress), 4, AF_INET);
            }
            if (theHost != NULL) {
                *outSocket = theSocket;
                memset(outAddress, 0, sizeof(sockaddr_in));
                outAddress->sin_family = AF_INET;
                outAddress->sin_port = htons(33334);
                memcpy(&outAddress->sin_addr, theHost->h_addr_list[0], 4);
                theResult = kNPIMOK;
            } else {
                theResult = kNPIMUnavailable;
            }
        }
    } else {
        ESRecordNativeError(WSAGetLastError());
        ESGetNativeError();
        theResult = kNPIMNativeError;
    }
    gDetectedGamesLock->Leave();
    return theResult;
}

// FUNCTION: TCP 0x1000192e
// Shuts WinSock down and frees the locks, cipher state, key buffer and random generator
extern "C" long NPIMDeactivate()
{
    if (WSACleanup() == 0) {
        TSScheduler::DestroyCriticalSection(gDetectedGamesLock);
        TSScheduler::DestroyCriticalSection(gSocketLock);
        TSScheduler::DestroyCriticalSection(gCipherLock);
        TSScheduler::DestroyCriticalSection(gXferLock);
        delete gCipher;
        gCipher = NULL;
        delete gRandomBuffer;
        gRandomBuffer = NULL;
        delete gRandom;
        gValid = false;
        return kNPIMOK;
    }
    ESRecordNativeError(WSAGetLastError());
    return kNPIMNativeError;
}

// FUNCTION: TCP 0x100019ae
// Returns a client slot's IP address as text; empty text and kNPIMBadIndex for an out-of-range slot
extern "C" long NPIMGetClientAddress(TCPServer** inHandle, unsigned long inIndex, UWideString& outAddress)
{
    TCPServer* theServer = *inHandle;
    long theResult = kNPIMOK;
    if (inIndex < theServer->mClients.size()) {
        outAddress = inet_ntoa(theServer->mClients[inIndex]->sin_addr);
    } else {
        theResult = kNPIMBadIndex;
        outAddress = "";
    }
    return theResult;
}

// FUNCTION: TCP 0x100019f6
// Returns a heap copy of the protocol configuration
extern "C" long NPIMGetConfiguration(NEProtocolConfiguration** outConfiguration)
{
    *outConfiguration = new NEProtocolConfiguration(gConfiguration);
    return *outConfiguration != NULL ? kNPIMOK : kNPIMOutOfMemory;
}

// FUNCTION: TCP 0x10001a24
// Copies one detected game's name, host and address under the list lock; 10 for an out-of-range index
extern "C" long NPIMGetDetectedGame(unsigned long inIndex, UString& outName, UString& outHost,
                                    UWideString& outAddress)
{
    long theResult;
    gDetectedGamesLock->Enter();
    if (inIndex < gDetectedGames.size()) {
        outName = gDetectedGames[inIndex].mName;
        outHost = gDetectedGames[inIndex].mHost;
        outAddress = gDetectedGames[inIndex].mAddress;
        theResult = kNPIMOK;
    } else {
        theResult = 10;
    }
    gDetectedGamesLock->Leave();
    return theResult;
}

// FUNCTION: TCP 0x10001aa1
// Returns the number of detected games (under the list lock)
extern "C" long NPIMGetDetectedGameCount()
{
    gDetectedGamesLock->Enter();
    long theCount = gDetectedGames.size();
    gDetectedGamesLock->Leave();
    return theCount;
}

// FUNCTION: TCP 0x10001adb
// Returns the index of the given adapter's IP in the local host's address list (0 when not found)
// (frees the walked adapter pointer instead of the buffer start)
// theIndex is first assigned by the address loop's for-init: VC6 then keeps a zero register (ebx) that the NULL
// tests compare against. The walk tests theAdapter before the counter.
static unsigned long FindAdapterAddressIndex(unsigned long inAdapter)
{
    unsigned long theIndex;
    if (inAdapter == 0)
        return 0;
    IP_ADAPTER_INFO* theAdapter = (IP_ADAPTER_INFO*)new char[0x1900];
    unsigned long theSize = 0x1900;
    if (GetAdaptersInfo(theAdapter, &theSize) == ERROR_SUCCESS) {
        for (unsigned long i = 1; theAdapter != NULL && i < inAdapter; i++)
            theAdapter = theAdapter->Next;
        unsigned long theAddress;
        char theHostName[0x400];
        hostent* theHost;
        sockaddr_in theHostAddress;
        if (theAdapter != NULL && theAdapter->IpAddressList.IpAddress.String != NULL
            && (theAddress = inet_addr(theAdapter->IpAddressList.IpAddress.String)) != 0
            && gethostname(theHostName, sizeof(theHostName)) != SOCKET_ERROR
            && (theHost = gethostbyname(theHostName)) != NULL) {
            for (theIndex = 0; theHost->h_addr_list[theIndex] != NULL; theIndex++) {
                theHostAddress.sin_addr.s_addr = 0;
                memcpy(&theHostAddress.sin_addr, theHost->h_addr_list[theIndex], 4);
                if (theHostAddress.sin_addr.s_addr == theAddress)
                    break;
            }
            theIndex = theHost->h_addr_list[theIndex] != NULL ? theIndex : 0;
        } else {
            theIndex = 0;
        }
    } else {
        theIndex = 0;
    }
    delete[] (char*)theAdapter;
    return theIndex;
}

// FUNCTION: TCP 0x10001bcd
// Lists the descriptions of up to 10 network adapters; returns how many
// (frees the walked adapter pointer instead of the buffer start)
extern "C" long NPIMEnumerateNetworkAdapters(std::vector<UWideString>& outAdapters)
{
    unsigned long theCount = 0;
    IP_ADAPTER_INFO* theAdapter = (IP_ADAPTER_INFO*)new char[0x1900];
    unsigned long theSize = 0x1900;
    DWORD theError = GetAdaptersInfo(theAdapter, &theSize);
    outAdapters.clear();
    while (theError == ERROR_SUCCESS && theAdapter != NULL && theCount < 10) {
        theCount++;
        outAdapters.push_back(UWideString(theAdapter->Description));
        theAdapter = theAdapter->Next;
    }
    delete[] (char*)theAdapter;
    return theCount;
}

// FUNCTION: TCP 0x10001c3f
// Returns the chosen adapter's local IP address as text, falling back to the address a UDP socket gets
extern "C" const UWideString& NPIMGetLocalAddress(unsigned long inAdapter)
{
    static UWideString sAddress;
    int theLength = sizeof(sockaddr_in);
    gAdapter = inAdapter;
    gAdapterAddressIndex = FindAdapterAddressIndex(inAdapter);
    char theHostName[0x400];
    sockaddr_in theLocal;
    theLocal.sin_addr.s_addr = 0;
    if (gethostname(theHostName, sizeof(theHostName)) != SOCKET_ERROR) {
        hostent* theHost = gethostbyname(theHostName);
        if (theHost != NULL)
            memcpy(&theLocal.sin_addr, theHost->h_addr_list[gAdapterAddressIndex], 4);
    }
    if (theLocal.sin_addr.s_addr == 0) {
        SOCKET theSocket = socket(AF_INET, SOCK_DGRAM, 0);
        if (theSocket != INVALID_SOCKET) {
            sockaddr_in theRemote;
            theRemote.sin_family = AF_INET;
            theRemote.sin_port = htons(7);
            theRemote.sin_addr.s_addr = inet_addr("128.127.126.125");
            if (connect(theSocket, (sockaddr*)&theRemote, sizeof(theRemote)) != SOCKET_ERROR)
                getsockname(theSocket, (sockaddr*)&theLocal, &theLength);
            closesocket(theSocket);
        }
    }
    sAddress = inet_ntoa(theLocal.sin_addr);
    return sAddress;
}

// FUNCTION: TCP 0x10001d5a
// Returns the protocol's type value (1)
extern "C" unsigned long NPIMGetProtocolType()
{
    return gProtocolKind;
}

// FUNCTION: TCP 0x10001d60
// Fills the 0x105-byte cipher key buffer from a fixed-seed random generator (the name test always passes)
extern "C" long NPIMGetStatus(const UWideString& inName, unsigned long inPort)
{
    UWideString theName;
    long theResult = kNPIMOK;
    theName = inName;
    if (theName == inName) {
        unsigned short thePort = inPort != 0 ? (unsigned short)(inPort >> 16) : htons(33334);
        unsigned long theSeed = 0xffffffff;
        if (gRandomBuffer == NULL)
            gRandomBuffer = new char[kRandomBufferSize];
        URandom* theRandom = new URandom(theSeed);
        for (unsigned long i = 0; i < kRandomBufferSize; i++)
            gRandomBuffer[i] = (char)(theRandom->Generate() * 255.0f);
        delete theRandom;
    } else {
        theResult = kNPIMWouldBlock;
    }
    return theResult;
}

// FUNCTION: TCP 0x10001e48
// Returns whether a stream connection is still open (peeks one byte); datagram connections are always alive
extern "C" bool NPIMIsAlive(TCPConnection** inHandle)
{
    TCPConnection* theConnection = *inHandle;
    if (theConnection->mState != 1)
        return true;
    char theByte;
    int theResult = recv(theConnection->mSocket, &theByte, 1, MSG_PEEK);
    if (theResult != 1) {
        if (theResult == 0)
            return false;
        bool theWouldBlock = WSAGetLastError() == WSAEWOULDBLOCK;
        return theWouldBlock;
    }
    return true;
}

// FUNCTION: TCP 0x10001e87
// Hosts a game: refuses a name already announced at that address, creates the stream/datagram sockets
// and the shared xfer listen socket on port 33335; returns the new server handle
extern "C" long NPIMPublish(const UString& inName, const UWideString& inAddress, TCPServerFlags inFlags,
                            WSAEVENT* inDataEvent, WSAEVENT* inListenEvent, WSAEVENT* inAcceptEvent,
                            TCPServer** outHandle, bool inFlag55, bool inFlag56, unsigned long inAdapter)
{
    SOCKET theSocket;
    long theResult = kNPIMOK;
    UString theName;
    UString theHost;
    UWideString theAddress;
    unsigned long thePort = htons(33334);
    unsigned long theNonBlocking = 1;
    bool theFound = false;
    if (!inFlag55 && QueryGames(inName, NULL) == 0) {
        unsigned long theCount = NPIMGetDetectedGameCount();
        for (unsigned long i = 0; i < theCount; i++) {
            NPIMGetDetectedGame(i, theName, theHost, theAddress);
            if (theAddress == inAddress) {
                theFound = true;
                break;
            }
        }
    }
    if (!theFound) {
        gAdapter = inAdapter;
        gAdapterAddressIndex = FindAdapterAddressIndex(inAdapter);
        TCPServer* theServer = new TCPServer;
        theServer->mSocket = INVALID_SOCKET;
        theServer->mXferSocket = INVALID_SOCKET;
        theServer->mFlags = inFlags;
        theServer->mFlag55 = inFlag55;
        theServer->mAnnounce = inFlag56;
        if (inFlags.mListening == 1) {
            theResult = CreateSocket(1, &theSocket, &thePort);
            if (theResult == kNPIMOK) {
                theServer->mPort = thePort;
                theServer->mSocket = theSocket;
                theServer->mAllowNewConnections = false;
                WSAEventSelect(theSocket, *inDataEvent, FD_READ);
            }
        }
        if (theResult == kNPIMOK && inFlags.mXfer == 1) {
            theResult = CreateSocket(2, &theSocket, &thePort);
            if (theResult == kNPIMOK) {
                theServer->mPort = thePort;
                theServer->mXferSocket = theSocket;
                theServer->mAllowNewConnections = true;
                WSAEventSelect(theSocket, *inDataEvent, FD_READ);
            }
        }
        if (theResult == kNPIMOK) {
            theServer->mName = inName;
            theServer->mAddress = inAddress;
            *outHandle = theServer;
            gSocket7280 = socket(AF_INET, SOCK_STREAM, 0);
            if (gSocket7280 != INVALID_SOCKET && ioctlsocket(gSocket7280, FIONBIO, &theNonBlocking) == 0) {
                sockaddr_in theLocal;
                memset(&theLocal, 0, sizeof(theLocal));
                theLocal.sin_family = AF_INET;
                theLocal.sin_port = htons(33335);
                if (bind(gSocket7280, (sockaddr*)&theLocal, sizeof(theLocal)) == 0 &&
                    listen(gSocket7280, SOMAXCONN) == 0) {
                    WSAEventSelect(gSocket7280, *inListenEvent, FD_READ);
                    WSAEventSelect(gSocket7280, *inAcceptEvent, FD_ACCEPT);
                    theResult = kNPIMOK;
                } else {
                    ESRecordNativeError(WSAGetLastError());
                    theResult = kNPIMNativeError;
                    closesocket(gSocket7280);
                }
            } else {
                ESRecordNativeError(WSAGetLastError());
                theResult = kNPIMNativeError;
                closesocket(gSocket7280);
            }
        } else {
            if (theServer->mSocket != INVALID_SOCKET)
                closesocket(theServer->mSocket);
            if (theServer->mXferSocket != INVALID_SOCKET)
                closesocket(theServer->mXferSocket);
            delete theServer;
        }
    } else {
        ESGetNativeError();
        ESRecordNativeError(ESGetNativeError());
        theResult = kNPIMNativeError;
    }
    return theResult;
}

// FUNCTION: TCP 0x1000211a
// Creates an empty server (members only)
TCPServer::TCPServer()
{
}

// FUNCTION: TCP 0x10002156
// Restarts the whole plug-in, then creates a non-blocking stream or datagram socket bound to *ioPort
// (listening for stream); returns the bound port and the socket, INVALID_SOCKET on failure
// NPIMConnect's error shape: every failing branch records, sets theResult and closes the socket itself (VC6
// tail-merges the four closesocket calls into one). Those references outweigh theResult, so theSocket gets esi
// and theResult edi, shared with the constant 1.
static long CreateSocket(long inType, SOCKET* outSocket, unsigned long* ioPort)
{
    long theResult;
    unsigned long theNonBlocking = 1;
    int theLength = sizeof(sockaddr_in);
    NPIMDeactivate();
    NPIMActivate();
    SOCKET theSocket = socket(AF_INET, inType == 1 ? SOCK_STREAM : SOCK_DGRAM, 0);
    if (theSocket != INVALID_SOCKET) {
        if (ioctlsocket(theSocket, FIONBIO, &theNonBlocking) == 0) {
            sockaddr_in theAddress;
            memset(&theAddress, 0, sizeof(theAddress));
            theAddress.sin_port = (u_short)*ioPort;
            theAddress.sin_family = AF_INET;
            if (bind(theSocket, (sockaddr*)&theAddress, sizeof(theAddress)) == 0) {
                if (getsockname(theSocket, (sockaddr*)&theAddress, &theLength) == 0) {
                    *ioPort = theAddress.sin_port;
                    if (inType != 1 || listen(theSocket, SOMAXCONN) == 0) {
                        *outSocket = theSocket;
                        theResult = kNPIMOK;
                    } else {
                        ESRecordNativeError(WSAGetLastError());
                        theResult = kNPIMNativeError;
                        closesocket(theSocket);
                    }
                } else {
                    ESRecordNativeError(WSAGetLastError());
                    theResult = kNPIMNativeError;
                    closesocket(theSocket);
                }
            } else {
                ESRecordNativeError(WSAGetLastError());
                ESGetNativeError();
                theResult = kNPIMNativeError;
                closesocket(theSocket);
            }
        } else {
            ESRecordNativeError(WSAGetLastError());
            theResult = kNPIMNativeError;
            closesocket(theSocket);
        }
    } else {
        ESRecordNativeError(WSAGetLastError());
        theResult = kNPIMNativeError;
    }
    if (theResult == kNPIMOK)
        *outSocket = theSocket;
    else
        *outSocket = INVALID_SOCKET;
    return theResult;
}

// FUNCTION: TCP 0x1000225f
// Receives from a connection; datagrams are deciphered, discovery packets dropped and other senders skipped
extern "C" long NPIMReceive(TCPConnection** inHandle, unsigned long inSize, char* outBuffer,
                            unsigned long* outReceived)
{
    long theResult = kNPIMOK;
    bool theContinue = true;
    sockaddr_in theFrom;
    int theFromLength;
    TCPConnection* theConnection = *inHandle;
    if (theConnection != NULL) {
        if (theConnection->mState == 1) {
            int theCount = recv(theConnection->mSocket, outBuffer, inSize, 0);
            if (theCount != 0 && theCount != SOCKET_ERROR) {
                *outReceived = theCount;
                theResult = kNPIMOK;
            } else if (theCount == 0) {
                theResult = kNPIMUnavailable;
            } else {
                int theError = WSAGetLastError();
                if (theError == WSAEWOULDBLOCK) {
                    *outReceived = 0;
                    theResult = kNPIMWouldBlock;
                } else {
                    ESRecordNativeError(theError);
                    theResult = kNPIMNativeError;
                }
            }
        } else {
            do {
                theFromLength = sizeof(theFrom);
                int theCount = recvfrom(theConnection->mSocket, outBuffer, inSize, 0,
                                        (sockaddr*)&theFrom, &theFromLength);
                if (theCount != 0 && theCount != SOCKET_ERROR) {
                    Encipher((unsigned char*)outBuffer + 1, theCount - 1, (unsigned char)outBuffer[0]);
                    if ((theCount == 0x88 && outBuffer[1] == 0x16) || (theCount == 0x43 && outBuffer[1] == 0x15)) {
                        *outReceived = 0;
                        theResult = kNPIMWouldBlock;
                    } else if (theFrom.sin_family == theConnection->mAddress.sin_family &&
                               theFrom.sin_port == theConnection->mAddress.sin_port &&
                               theFrom.sin_addr.s_addr == theConnection->mAddress.sin_addr.s_addr) {
                        theResult = kNPIMOK;
                        *outReceived = theCount;
                        theContinue = false;
                    }
                } else if (theCount == 0) {
                    theResult = kNPIMUnavailable;
                    break;
                } else {
                    int theError = WSAGetLastError();
                    if (theError == WSAEWOULDBLOCK) {
                        *outReceived = 0;
                        theResult = kNPIMWouldBlock;
                    } else {
                        ESRecordNativeError(theError);
                        theResult = kNPIMNativeError;
                    }
                    theContinue = false;
                }
            } while (theContinue);
        }
    } else {
        theResult = kNPIMWouldBlock;
    }
    return theResult;
}

// FUNCTION: TCP 0x100023a7
// Deciphers/enciphers data in place with RC4 keyed from the key buffer; returns with the lock held when there is
// no key buffer or the key is 0
static void Encipher(unsigned char* ioData, long inLength, unsigned long inKey)
{
    unsigned char x;
    unsigned char y;
    unsigned char* state;
    unsigned char xorIndex;
    short counter;
    gCipherLock->Enter();
    if (gRandomBuffer == NULL || inKey == 0)
        return;                                 // sic: returns with gCipherLock held (original: jumps past Leave)
    PrepareCipher(inKey - 1, 7);
    x = gCipher->mX;
    y = gCipher->mY;
    state = &gCipher->mState[0];
    for (counter = 0; counter < inLength; counter++) {
        x = (x + 1) % 256;
        y = (state[x] + y) % 256;
        swap_byte(&state[x], &state[y]);
        xorIndex = (state[x] + state[y]) % 256;
        ioData[counter] ^= state[xorIndex];
    }
    gCipher->mX = x;
    gCipher->mY = y;
    gCipherLock->Leave();
}

// FUNCTION: TCP 0x100024a8
// Sets up the RC4 state from inKeyLength bytes of the key buffer at inKeyOffset
static void PrepareCipher(unsigned long inKeyOffset, unsigned long inKeyLength)
{
    if (gRandomBuffer == NULL)
        return;
    unsigned char* theKey = reinterpret_cast<unsigned char*>(gRandomBuffer) + inKeyOffset;
    RC4State* theCipher = gCipher;
    short i;
    for (i = 0; i < 256; i++)
        theCipher->mState[i] = (unsigned char)i;
    gCipher->mX = 0;
    gCipher->mY = 0;
    unsigned char theKeyIndex = 0;
    unsigned char j = 0;
    for (i = 0; i < 256; i++) {
        j = (theCipher->mState[i] + theKey[theKeyIndex] + j) % 256;
        swap_byte(&theCipher->mState[i], &theCipher->mState[j]);
        theKeyIndex = (theKeyIndex + 1) % inKeyLength;
    }
}

// FUNCTION: TCP 0x10002539
// Receives a datagram on a server: answers game queries with an announcement and adds new clients that send a
// join packet; returns the client slot index
extern "C" long NPIMReceiveFrom(TCPServer** inHandle, unsigned long inSize, char* outBuffer,
                                unsigned long* outReceived, unsigned long* outIndex)
{
    unsigned char theKey;
    char* theData;
    sockaddr_in* theClient;
    GameAnnouncement thePacket;
    long theResult = kNPIMOK;
    TCPServer* theServer = *inHandle;
    if (theServer->mFlags.mXfer == 1) {
        sockaddr_in theFrom;
        int theFromLength = sizeof(theFrom);
        int theCount = recvfrom(theServer->mXferSocket, outBuffer, inSize, 0, (sockaddr*)&theFrom, &theFromLength);
        if (theCount != 0 && theCount != SOCKET_ERROR) {
            theKey = outBuffer[0];
            theData = outBuffer + 1;
            Encipher((unsigned char*)theData, theCount - 1, theKey);
            if (theCount == sizeof(GameQuery) && *theData == 0x15) {
                *outReceived = 0;
                theResult = kNPIMWouldBlock;
                if (strcmp(((GameQuery*)outBuffer)->mName, theServer->mName) == 0 &&
                    theServer->mAllowNewConnections == true && theServer->mFlag55 == false &&
                    theServer->mAnnounce == true) {
                    SOCKET theSocket = socket(AF_INET, SOCK_DGRAM, 0);
                    if (theSocket != INVALID_SOCKET) {
                        thePacket.mType = 0x16;
                        strcpy(thePacket.mName, theServer->mName);
                        strcpy(thePacket.mAddress, theServer->mAddress);
                        theKey = (unsigned char)(gRandom->Generate() * 254.0f + 1.0f);
                        memcpy(&thePacket, &theKey, 1);
                        Encipher(&thePacket.mType, sizeof(thePacket) - 1, theKey);
                        theFrom.sin_port = htons(33334);
                        if (sendto(theSocket, reinterpret_cast<char*>(&thePacket), sizeof(thePacket), 0, (sockaddr*)&theFrom,
                                   sizeof(theFrom)) == SOCKET_ERROR) {
                            ESRecordNativeError(WSAGetLastError());
                            ESGetNativeError();
                        }
                        closesocket(theSocket);
                    } else {
                        ESRecordNativeError(WSAGetLastError());
                        ESGetNativeError();
                    }
                }
            } else {
                unsigned long i;
                for (i = 0; i < theServer->mClients.size(); i++) {
                    theClient = theServer->mClients[i];
                    if (theFrom.sin_family == theClient->sin_family &&
                        theFrom.sin_addr.s_addr == theClient->sin_addr.s_addr &&
                        theFrom.sin_port == theClient->sin_port)
                        break;
                }
                if (i == theServer->mClients.size()) {
                    if (theCount >= 4 && (*theData == 0x22 || *theData == 0x37)) {
                        if (theServer->mAllowNewConnections && theFrom.sin_port != 0) {
                            theClient = new sockaddr_in;
                            memcpy(theClient, &theFrom, sizeof(sockaddr_in));
                            theServer->mClients.push_back(theClient);
                            *outIndex = theServer->mClients.size() - 1;
                            *outReceived = theCount;
                            theServer->mConnections.push_back(NULL);
                            theResult = kNPIMOK;
                        }
                    } else {
                        *outReceived = 0;
                        theResult = kNPIMWouldBlock;
                    }
                } else {
                    *outIndex = i;
                    *outReceived = theCount;
                    theResult = kNPIMOK;
                }
            }
        } else if (theCount == 0) {
            theResult = kNPIMUnavailable;
        } else {
            int theError = WSAGetLastError();
            if (theError == WSAEWOULDBLOCK) {
                *outReceived = 0;
                theResult = kNPIMWouldBlock;
            } else {
                ESRecordNativeError(theError);
                theResult = kNPIMNativeError;
            }
        }
    } else {
        theResult = kNPIMUnavailable;
    }
    return theResult;
}

// FUNCTION: TCP 0x10002818
// Marks a client slot as gone (clears its port)
extern "C" void NPIMResetClientConnection(TCPServer** inHandle, unsigned long inIndex)
{
    TCPServer* theServer = *inHandle;
    if (inIndex < theServer->mClients.size())
        theServer->mClients[inIndex]->sin_port = 0;
}

// FUNCTION: TCP 0x10002841
// Frees every file-transfer connection of the server and empties the list
extern "C" void NPIMResetXfer(TCPServer** inHandle)
{
    TCPServer* theServer = *inHandle;
    for (unsigned long i = 0; i < theServer->mConnections.size(); i++)
        delete theServer->mConnections[i];
    theServer->mConnections.clear();
}

// FUNCTION: TCP 0x1000287c
// Starts or stops the background game-search thread for a game name and clears the detected-games list
extern "C" long NPIMSearchForGame(const UString& inHost, const UString& inName, bool inStart)
{
    if (inName.GetLength() > 0) {
        if (inStart == true) {
            if (!gSearcherRunning) {
                gDetectedGames.clear();
                ESVERIFY_AT(TSScheduler::CreateEvent(gSearcherThreadFinishedEvent) == keseOK, 2189);
                ESVERIFY_AT(TSScheduler::CreateEvent(gSearcherThreadExitEvent) == keseOK, 2190);

                UString* theService = new UString(inName);

                ESVERIFY_AT(TSScheduler::CreateThread(gSearcherThread, Searcher, (void*)theService) == keseOK, 2194);
                gSearcherRunning = true;
            }
        } else {
            if (gSearcherRunning == true) {
                gSearcherThreadExitEvent->Set();
                gSearcherThreadFinishedEvent->Wait(INFINITE);
                TSScheduler::DestroyThread(gSearcherThread);
                TSScheduler::DestroyEvent(gSearcherThreadFinishedEvent);
                TSScheduler::DestroyEvent(gSearcherThreadExitEvent);
                gSearcherRunning = false;
            }
            gDetectedGames.clear();
        }
    }
    return 0;
}

// FUNCTION: TCP 0x100029d2
// Sends data on a connection: the whole buffer on a stream, enciphered with a random key on a datagram socket
extern "C" long NPIMSend(TCPConnection** inHandle, char* ioData, unsigned long inLength)
{
    unsigned char theKey;
    long theResult;
    TCPConnection* theConnection = *inHandle;
    if (theConnection->mState == 1) {
        unsigned long theSent = 0;
        theResult = kNPIMOK;
        while (0 < inLength) {
            int theCount = send(theConnection->mSocket, ioData + theSent, inLength, 0);
            if (theCount == SOCKET_ERROR) {
                ESRecordNativeError(WSAGetLastError());
                ESGetNativeError();
                theResult = kNPIMNativeError;
                break;
            }
            inLength -= theCount;
            theSent += theCount;
        }
    } else {
        theKey = (unsigned char)(gRandom->Generate() * 254.0f + 1.0f);
        memcpy(ioData, &theKey, 1);
        Encipher((unsigned char*)ioData + 1, inLength - 1, theKey);
        int theCount = sendto(theConnection->mSocket, ioData, inLength, 0,
                              (sockaddr*)&theConnection->mAddress, sizeof(sockaddr_in));
        if (theCount == inLength) {
            theResult = kNPIMOK;
        } else if (theCount != SOCKET_ERROR) {
            theResult = kNPIMUnavailable;
        } else {
            ESRecordNativeError(WSAGetLastError());
            ESGetNativeError();
            theResult = kNPIMNativeError;
        }
    }
    return theResult;
}

// FUNCTION: TCP 0x10002aab
// Sends one datagram to a host and port from a temporary socket
// Error-flag shape (as NPIMPublish): the address phase sets theResult, the bind/send phase runs under
// `theResult == kNPIMOK` (threaded away; it splits the two `add esp, 0xc`); explicit `= kNPIMOK` on success and
// function-scope locals keep theResult in ebx and theAddress in a fresh slot.
extern "C" long NPIMSendSingle(const UString& inHost, unsigned long inPort, char* inData, unsigned long inLength)
{
    long theResult = kNPIMOK;
    unsigned long theAddress;
    sockaddr_in theTo;
    sockaddr_in theLocal;
    SOCKET theSocket = socket(AF_INET, SOCK_DGRAM, 0);
    if (theSocket != INVALID_SOCKET) {
        hostent* theHost = gethostbyname(inHost);
        if (theHost == NULL) {
            theAddress = inet_addr(inHost);
            if (theAddress != INADDR_NONE)
                theHost = gethostbyaddr((char*)&theAddress, 4, AF_INET);
        }
        if (theHost != NULL) {
            memset(&theTo, 0, sizeof(theTo));
            theTo.sin_family = AF_INET;
            theTo.sin_port = htons((u_short)inPort);
            memcpy(&theTo.sin_addr, theHost->h_addr_list[gAdapterAddressIndex], 4);
        } else {
            theResult = kNPIMUnavailable;
        }
        if (theResult == kNPIMOK) {
            memset(&theLocal, 0, sizeof(theLocal));
            theLocal.sin_family = AF_INET;
            theLocal.sin_port = 0;
            if (bind(theSocket, (sockaddr*)&theLocal, sizeof(theLocal)) == 0) {
                int theCount = sendto(theSocket, inData, inLength, 0, (sockaddr*)&theTo, sizeof(theTo));
                if (theCount == inLength) {
                    theResult = kNPIMOK;
                } else if (theCount != SOCKET_ERROR) {
                    theResult = kNPIMUnavailable;
                } else {
                    ESRecordNativeError(WSAGetLastError());
                    ESGetNativeError();
                    theResult = kNPIMNativeError;
                }
            } else {
                ESRecordNativeError(WSAGetLastError());
                ESGetNativeError();
                theResult = kNPIMNativeError;
            }
        }
        closesocket(theSocket);
    }
    return theResult;
}

// FUNCTION: TCP 0x10002bbe
// Enciphers and sends a datagram to a client slot of the server; 10 for an out-of-range slot
extern "C" long NPIMSendTo(TCPServer** inHandle, unsigned long inIndex, char* ioData, unsigned long inLength)
{
    TCPServer* theServer = *inHandle;
    if (theServer->mFlags.mXfer == 1) {
        if (inIndex < theServer->mClients.size()) {
            unsigned char theKey = (unsigned char)(gRandom->Generate() * 254.0f + 1.0f);
            memcpy(ioData, &theKey, 1);
            Encipher((unsigned char*)ioData + 1, inLength - 1, theKey);
            sockaddr_in*& theClient = theServer->mClients[inIndex];
            int theCount = sendto(theServer->mXferSocket, ioData, inLength, 0,
                                  (sockaddr*)theClient, sizeof(sockaddr_in));
            if (theCount == inLength)
                return kNPIMOK;
            if (theCount == SOCKET_ERROR) {
                ESRecordNativeError(WSAGetLastError());
                ESGetNativeError();
                return kNPIMNativeError;
            }
        } else {
            return 10;
        }
    }
    return kNPIMUnavailable;
}

// FUNCTION: TCP 0x10002c86
// Accepts a configuration only when its protocol type and parameter count match ours (stores nothing)
extern "C" long NPIMSetConfiguration(const NEProtocolConfiguration* inConfiguration)
{
    if (inConfiguration->GetProtocolType() == gConfiguration.GetProtocolType())
        return inConfiguration->GetParameterCount() == gConfiguration.GetParameterCount() ? kNPIMOK : kNPIMUnavailable;
    return kNPIMUnavailable;
}

// FUNCTION: TCP 0x10002caa
// Receives from the file-transfer socket; would-block when nothing is waiting
extern "C" long NPIMXferReceive(unsigned long inSize, char* outBuffer, unsigned long* outReceived)
{
    int theResult = recv(gXferSocket, outBuffer, inSize, 0);
    if (theResult != 0 && theResult != SOCKET_ERROR) {
        *outReceived = theResult;
        return kNPIMOK;
    }
    if (theResult == 0)
        return kNPIMUnavailable;
    int theError = WSAGetLastError();
    if (theError == WSAEWOULDBLOCK) {
        *outReceived = 0;
        return kNPIMWouldBlock;
    }
    ESRecordNativeError(theError);
    return kNPIMNativeError;
}

// FUNCTION: TCP 0x10002cfa
// Sends the whole buffer on a client's file-transfer connection, retrying while it would block (under the xfer lock)
extern "C" long NPIMXferSendTo(TCPServer** inHandle, unsigned long inIndex, char* inData, unsigned long inLength)
{
    long theResult;
    gXferLock->Enter();
    TCPServer* theServer = *inHandle;
    if (inIndex < theServer->mConnections.size() && theServer->mConnections[inIndex] != NULL) {
        unsigned long theSent = 0;
        theResult = kNPIMOK;
        while (0 < inLength) {
            int theCount = send(theServer->mConnections[inIndex]->mSocket,
                                inData + theSent, inLength, 0);
            if (theCount != SOCKET_ERROR) {
                inLength -= theCount;
                theSent += theCount;
            } else if (WSAGetLastError() == WSAEWOULDBLOCK) {
                Sleep(1);
            } else {
                theResult = kNPIMNativeError;
                break;
            }
        }
    } else {
        theResult = 10;
    }
    gXferLock->Leave();
    return theResult;
}

// FUNCTION: TCP 0x10002da4
// Returns the plug-in's name ("TCP/IP Protocol")
extern "C" const UString* PIMGetName()
{
    return &gProtocolName;
}

// FUNCTION: TCP 0x10002daa
// Returns the plug-in type ('ntwk')
extern "C" unsigned long PIMGetType()
{
    return gProtocolType;
}

// FUNCTION: TCP 0x10002db0
// Returns whether WinSock 2.0 started (the plug-in is usable)
extern "C" bool PIMIsValid()
{
    return gValid;
}

// FUNCTION: TCP 0x10002db6
// Broadcasts a game query and collects the announcements that answer within one second into the detected-games list
static long QueryGames(const UString& inName, hostent* inHost)
{
    long theResult = kNPIMOK;
    DetectedGame theGame;
    unsigned char theKey;
    SOCKET theReceiveSocket;
    SOCKET theSendSocket;
    unsigned long theDeadline;
    int theFromLength;
    int theBroadcast;
    unsigned long theNonBlocking = 1;
    if (inName.GetLength() > 0) {
        gSocketLock->Enter();
        theSendSocket = socket(AF_INET, SOCK_DGRAM, 0);
        if (theSendSocket != INVALID_SOCKET) {
            sockaddr_in theTo;
            memset(&theTo, 0, sizeof(theTo));
            theTo.sin_family = AF_INET;
            theTo.sin_port = htons(33334);
            if (inHost == NULL)
                theTo.sin_addr.s_addr = htonl(INADDR_BROADCAST);
            else
                memcpy(&theTo.sin_addr, inHost->h_addr_list[gAdapterAddressIndex], 4);
            theBroadcast = 1;
            if (setsockopt(theSendSocket, SOL_SOCKET, SO_BROADCAST, reinterpret_cast<char*>(&theBroadcast), sizeof(theBroadcast)) == 0) {
                sockaddr_in theLocal;
                memset(&theLocal, 0, sizeof(theLocal));
                theLocal.sin_family = AF_INET;
                theLocal.sin_port = htons(33334);
                theReceiveSocket = socket(AF_INET, SOCK_DGRAM, 0);
                if (bind(theReceiveSocket, (sockaddr*)&theLocal, sizeof(theLocal)) == 0 &&
                    ioctlsocket(theReceiveSocket, FIONBIO, &theNonBlocking) == 0) {
                    GameQuery theQuery;
                    theQuery.mType = kSearchPacketGameQuery;
                    strcpy(theQuery.mName, inName);
                    gDetectedGames2.clear();
                    theKey = (unsigned char)(gRandom->Generate() * 254.0f + 1.0f);
                    memcpy(&theQuery, &theKey, 1);
                    Encipher(&theQuery.mType, sizeof(theQuery) - 1, theKey);
                    if (sendto(theSendSocket, reinterpret_cast<char*>(&theQuery), sizeof(theQuery), 0, (sockaddr*)&theTo,
                               sizeof(theTo)) != SOCKET_ERROR) {
                        theDeadline = TMGetTime() + 1000;
                        theFromLength = sizeof(sockaddr_in);
                        while (theDeadline > TMGetTime()) {
                            char theBuffer[0x200];
                            sockaddr_in theFrom;
                            int theCount = recvfrom(theReceiveSocket, theBuffer, sizeof(theBuffer), 0,
                                                    (sockaddr*)&theFrom, &theFromLength);
                            if (theCount > 0) {
                                theKey = theBuffer[0];
                                Encipher(reinterpret_cast<unsigned char*>(theBuffer) + 1, theCount - 1, theKey);
                            }
                            GameAnnouncement* theAnnouncement = reinterpret_cast<GameAnnouncement*>(theBuffer);
                            if (theCount == sizeof(GameAnnouncement) && theAnnouncement->mType == kSearchPacketGameAnnouncement) {
                                theGame.mName = inet_ntoa(theFrom.sin_addr);
                                bool theNew = true;
                                for (unsigned long i = 0; i < gDetectedGames2.size(); i++) {
                                    DetectedGame& theOther = gDetectedGames2[i];
                                    if (theOther.mName == theGame.mName) {
                                        theNew = false;
                                        break;
                                    }
                                }
                                if (theNew) {
                                    theGame.mAddress = theAnnouncement->mAddress;
                                    theGame.mHost = theAnnouncement->mName;
                                    theGame.mDatagramAddress = theFrom;
                                    gDetectedGames2.push_back(theGame);
                                }
                            }
                            Sleep(1);
                        }
                        gDetectedGamesLock->Enter();
                        gDetectedGames.resize(gDetectedGames2.size());
                        std::copy(gDetectedGames2.begin(), gDetectedGames2.end(), gDetectedGames.begin());
                        gDetectedGamesLock->Leave();
                        closesocket(theReceiveSocket);
                        theResult = kNPIMOK;
                    } else {
                        ESRecordNativeError(WSAGetLastError());
                        ESGetNativeError();
                        theResult = kNPIMNativeError;
                    }
                } else {
                    ESRecordNativeError(WSAGetLastError());
                    ESGetNativeError();
                    theResult = kNPIMNativeError;
                }
            } else {
                ESRecordNativeError(WSAGetLastError());
                ESGetNativeError();
                theResult = kNPIMNativeError;
            }
            closesocket(theSendSocket);
        } else {
            ESRecordNativeError(WSAGetLastError());
            ESGetNativeError();
            theResult = kNPIMNativeError;
        }
        gSocketLock->Leave();
    }
    return theResult;
}

// FUNCTION: TCP 0x100031da
// Search thread: re-queries for the game every 50 ms until the exit event is set, then signals that it finished
static unsigned long Searcher(void* inService)
{
    UString theName;
    bool theDone = false;
    ESVERIFY_AT(inService != NULL, 2845);
    theName = *static_cast<UString*>(inService);
    delete static_cast<UString*>(inService);
    HANDLE theExitEvent = gSearcherThreadExitEvent->mHandle;
    UMSetFloatingPointPrecision(_PC_53);
    while (!theDone) {
        if (UMGetFloatingPointPrecision() != _PC_53)
            UMSetFloatingPointPrecision(_PC_53);
        if (UMGetFloatingPointRoundingMode() != _RC_NEAR)
            UMSetFloatingPointRoundingMode(_RC_NEAR);
        switch (WaitForSingleObject(theExitEvent, 50)) {
        case WAIT_OBJECT_0:
            gSearcherThreadExitEvent->Clear();
            theDone = true;
            break;
        case WAIT_TIMEOUT:
            QueryGames(theName, NULL);
            break;
        case WAIT_FAILED:
            GetLastError();
            break;
        }
    }
    gSearcherThreadFinishedEvent->Set();
    return 0;
}

// FUNCTION: TCP 0x100032be
// Swaps two bytes
static void swap_byte(unsigned char* a, unsigned char* b)
{
    unsigned char swapByte;
    swapByte = *a;
    *a = *b;
    *b = swapByte;
}
