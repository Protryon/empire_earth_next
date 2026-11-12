// C:\EEX\Empire Earth\WONAPI\WONLobby\GameCode\EmpireEarth\EELobbyFileTransfer.cpp (file name inferred: no __FILE__)
// Class EELobbyFileTransfer (RTTI, vtable 0x84c440) and its WON send operation; the unit sorts between
// EELobbyFileDialog.cpp and EELobbyGame.cpp. Transfers a saved game from the host to the clients in a lobby game
// (0x691463..0x69222b).
#include <windows.h>
#include "platform/PlatFile.h"
#include <string>
#include <list>
#include <vector>
#include <map>
#include <set>
#include <stdio.h>

#include "lle/LowLevelEngine.h"

#include "ee/EEWonNet.h"
#include "ee/wonapi/WONRouting.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/EELobbyGameCode.h"
#include "ee/wonapi/WONStatus.h"

using namespace WONAPI;

// The host's side of a saved game's transfer: sends the file in 0x2000-byte chunks over one client's socket (RTTI
// SendFileOp > SocketOp, vtable 0x84c408; 0x207c bytes)
class SendFileOp : public SocketOp {
public:
    SendFileOp(const std::string& inPath, AsyncSocket* inSocket);
    virtual void CleanupHook() { this->CloseFile(); }  // +0x08 0x2916b3
    virtual WONStatus Start();              // +0x0c 0x2915d2
    virtual WONStatus Continue();           // +0x10 0x291613
    void CloseFile();                       // 0x29157e
    bool ReadChunk();                       // 0x291596

    FILE* mFile;                            // +0x5c
    std::string mPath;                      // +0x60
    char mBuffer[0x2000];                   // +0x70
    ReadBuffer mReadBuffer;                 // +0x2070
};

// FUNCTION: EXE 0x691463
// Creates the send operation for the saved game at the given path, with no file open yet
SendFileOp::SendFileOp(const std::string& inPath, AsyncSocket* inSocket)
    : SocketOp(inSocket), mPath(inPath), mReadBuffer(mBuffer, 0x2000)
{
    this->mFile = NULL;
    this->SetWaitForEvent(1,true);
}

// TEMPLATE: EXE 0x6914c2
// (SocketOp::SocketOp, inline)

// TEMPLATE: EXE 0x69151c
// (SocketOp::Start, inline)

// SYNTHETIC: EXE 0x691521
// (SocketOp's scalar deleting destructor)

// TEMPLATE: EXE 0x69153d
// (SocketOp::~SocketOp, inline)

// SYNTHETIC: EXE 0x691555
// (SendFileOp's scalar deleting destructor)

// FUNCTION: EXE 0x69157e
// Closes the saved game file if it is open
void SendFileOp::CloseFile()
{
    if (this->mFile != NULL) {
        fclose(this->mFile);
        this->mFile = NULL;
    }
}

// FUNCTION: EXE 0x691596
// Reads the next chunk of the saved game into the send buffer; returns whether anything was read
bool SendFileOp::ReadChunk()
{
    int theCount = fread(this->mBuffer, 1, 0x2000, this->mFile);
    if (theCount < 0)
        theCount = 0;
    this->mReadBuffer.SetData(this->mBuffer, theCount);
    return theCount > 0;
}

// FUNCTION: EXE 0x6915d2
// Opens the saved game and starts sending it; fails when there is no path or the file cannot be opened
WONStatus SendFileOp::Start()
{
    if (this->mPath.size() == 0)
        return WS_FailedToOpenFile;
    if ((this->mFile = PlatFOpen(this->mPath.c_str(), "rb")) == NULL)
        return WS_FailedToOpenFile;
    this->ReadChunk();
    return this->Continue();
}

// FUNCTION: EXE 0x691613
// Sends the saved game chunk by chunk until the file ends, a socket error occurs or the time runs out
WONStatus SendFileOp::Continue()
{
    for (;;) {
        if ((unsigned long)this->mReadBuffer.pos() < (unsigned long)this->mReadBuffer.length()) {
            this->GetSocket()->WaitForWrite(this->TimeLeft());
            int theSent = 0;
            WONStatus theStatus = this->GetSocket()->Send(this->mReadBuffer.data() + this->mReadBuffer.pos(),
                this->mReadBuffer.length() - this->mReadBuffer.pos(), &theSent);
            if ((long)theSent > 0)
                this->mReadBuffer.ReadBytes(theSent);
            if (theStatus == WS_TimedOut && this->TimeLeft() == 0)
                return WS_TimedOut;
            if (theStatus != WS_Success)
                return theStatus;
        } else if (!this->ReadChunk()) {
            this->CloseFile();
            return WS_Success;
        }
    }
}

// TEMPLATE: EXE 0x6916b3
// (SendFileOp::CleanupHook, inline)

// FUNCTION: EXE 0x6916b8
// Creates an idle saved-game transfer with no local or host file known
EELobbyFileTransfer::EELobbyFileTransfer()
{
    this->mLocalSize = 0;
    this->mLocalChecksum = 0;
    this->mSize = 0;
    this->mChecksum = 0;
    this->mLastProgressTime = 0;
}

// SYNTHETIC: EXE 0x69173f
// (the scalar deleting destructor)

// FUNCTION: EXE 0x69175b
// Destroys the transfer (members only)
EELobbyFileTransfer::~EELobbyFileTransfer()
{
}

// FUNCTION: EXE 0x6917a2
// Sets the local saved game's path and reads its size and checksum
void EELobbyFileTransfer::SetPath(const std::string& inPath)
{
    this->mPath = inPath;
    this->mLocalChecksum = 0;
    this->mLocalSize = 0;
    EELobbyGetSavedGameInfo(this->mPath.c_str(), this->mLocalSize, this->mLocalChecksum);
}

// FUNCTION: EXE 0x6917e9
// Records the name, size and checksum of the host's saved game
void EELobbyFileTransfer::SetFileInfo(const GUIString& inName, unsigned long inSize, unsigned long inChecksum)
{
    this->mName = inName;
    this->mSize = inSize;
    this->mChecksum = inChecksum;
}

// FUNCTION: EXE 0x69180c
// Sets the address of the host to download the saved game from
void EELobbyFileTransfer::SetHost(const IPAddr& inAddress)
{
    this->mHost = inAddress;
}

// TEMPLATE: EXE 0x69181b
// (IPAddr's implicit operator=)

// FUNCTION: EXE 0x691859
// Returns whether the local saved game has the host's size and checksum
bool EELobbyFileTransfer::IsFileHere()
{
    return this->mSize == this->mLocalSize && this->mChecksum == this->mLocalChecksum;
}

// FUNCTION: EXE 0x691870
// Forgets a client's socket once its connection closes
void EELobbyFileTransfer::OnClientClosed(AsyncOp* inOp)
{
    SmartPtr<AsyncSocket> theSocket = static_cast<SocketOp*>(inOp)->GetSocket();
    this->mClients.erase(theSocket);
}

// FUNCTION: EXE 0x6918b2
// Keeps a newly connected client's socket and queues the saved game's send, then a close, on it
void EELobbyFileTransfer::OnClientAccepted(AsyncOp* inOp)
{
    if (!inOp->Succeeded())
        return;
    SmartPtr<AsyncSocket> theSocket = static_cast<AcceptOp*>(inOp)->GetAcceptedSocket();
    this->mClients.insert(theSocket);
    static_cast<QueueSocket*>(theSocket.get())->SetCloseCompletion(new LobbyCompletion<EELobbyFileTransfer>(&EELobbyFileTransfer::OnClientClosed, this));
    static_cast<QueueSocket*>(theSocket.get())->QueueOp(new SendFileOp(this->mPath, NULL), -1);
    static_cast<QueueSocket*>(theSocket.get())->QueueOp(new CloseOp(NULL), -1);
}

// TEMPLATE: EXE 0x691993
// (CloseOp::Continue, inline)

// FUNCTION: EXE 0x69199e
// Reports the download's bytes received to the lobby game, at most once a second
// Called on each chunk the receiving operation completes
void EELobbyFileTransfer::OnProgress(AsyncOp* inOp)
{
    EELobbyGame* theGame = static_cast<EELobbyGame*>(GetMyLobbyGame());
    if (theGame == NULL)
        return;
    unsigned long theTime = (unsigned long)PlatTimeMilliseconds64();
    if (theTime - this->mLastProgressTime < 1000)
        return;
    this->mLastProgressTime = theTime;
    theGame->UpdateTransferProgress(static_cast<RecvChunkOp*>(inOp)->GetRecvOp()->GetBytesRead(), this->mSize);
}

// FUNCTION: EXE 0x6919dc
// Shows the download as complete in the lobby game once the saved game has arrived
void EELobbyFileTransfer::OnFileReceived(AsyncOp* inOp)
{
    EELobbyGame* theGame = static_cast<EELobbyGame*>(GetMyLobbyGame());
    if (theGame != NULL && inOp->Succeeded())
        theGame->UpdateTransferProgress(this->mSize, this->mSize);
}

// FUNCTION: EXE 0x691a09
// Starts serving the saved game on a port: listens and sends the file to every client that connects
WONStatus EELobbyFileTransfer::Serve(unsigned short inPort)
{
    this->Stop();
    this->mConnection = new QueueSocket(0);
    WONStatus theStatus = this->mConnection->Fn2B0D90(inPort, 0);
    if (theStatus != 0)
        return theStatus;
    theStatus = this->mConnection->Listen(1000);
    if (theStatus != 0)
        return theStatus;
    this->mConnection->SetRepeatCompletion(new LobbyCompletion<EELobbyFileTransfer>(&EELobbyFileTransfer::OnClientAccepted, this));
    this->mConnection->SetRepeatOp(new AcceptOp(NULL));
    return 0;
}

// TEMPLATE: EXE 0x691ab7
// (AcceptOp::Duplicate, inline: emitted with its vtable)

// SYNTHETIC: EXE 0x691aea
// (AcceptOp's scalar deleting destructor)

// TEMPLATE: EXE 0x691b06
// (AcceptOp::~AcceptOp, inline)

// FUNCTION: EXE 0x691b1e
// Stops serving: closes every client's connection, then the listening one
void EELobbyFileTransfer::Stop()
{
    this->CloseClients();
    if (this->mConnection.mPtr != NULL) {
        this->mConnection->Kill();
        this->mConnection = NULL;
    }
}

// FUNCTION: EXE 0x691b40
// Kills every client connection and forgets them
void EELobbyFileTransfer::CloseClients()
{
    for (std::set<SmartPtr<AsyncSocket> >::iterator it = this->mClients.begin(); it != this->mClients.end(); ++it)
        (*it)->Kill();
    this->mClients.clear();
}

// FUNCTION: EXE 0x691b79
// Downloads the host's saved game into the local path, reporting progress and the end to the lobby game
// Does nothing without a local path or a valid host address; the download socket joins the clients
void EELobbyFileTransfer::Download()
{
    if (this->mPath.empty() || !this->mHost.IsValid())
        return;
    this->mLastProgressTime = (unsigned long)PlatTimeMilliseconds64();
    SmartPtr<AsyncSocket> theSocket = new QueueSocket(0);
    static_cast<QueueSocket*>(theSocket.get())->SetCloseCompletion(new LobbyCompletion<EELobbyFileTransfer>(&EELobbyFileTransfer::OnClientClosed, this));
    SmartPtr<RecvBytesOp> theOp = new RecvBytesOp(0, NULL);
    theOp->SetCompletion(new LobbyCompletion<EELobbyFileTransfer>(&EELobbyFileTransfer::OnFileReceived, this));
    theOp->SetFilePath(this->mPath);
    theOp->SetProgressCompletion(new LobbyCompletion<EELobbyFileTransfer>(&EELobbyFileTransfer::OnProgress, this), 10000);
    static_cast<QueueSocket*>(theSocket.get())->QueueOp(new ConnectOp(this->mHost), -1);
    static_cast<QueueSocket*>(theSocket.get())->QueueOp(theOp.mPtr, -1);
    this->mClients.insert(theSocket);
}

// TEMPLATE: EXE 0x691d59
// (IPAddr's implicit copy ctor, for ConnectOp's inline ctor)

// TEMPLATE: EXE 0x691da9
// (compiler-generated; name in analysis/exe/names.txt)

// TEMPLATE: EXE 0x691dd4
// (compiler-generated; name in analysis/exe/names.txt)

// TEMPLATE: EXE 0x691e8a
// (compiler-generated; name in analysis/exe/names.txt)

// TEMPLATE: EXE 0x691edb
// (compiler-generated; name in analysis/exe/names.txt)

// TEMPLATE: EXE 0x691f6f
// (compiler-generated; name in analysis/exe/names.txt)

// TEMPLATE: EXE 0x691f8c
// (compiler-generated; name in analysis/exe/names.txt)

// TEMPLATE: EXE 0x691ffd
// (compiler-generated; name in analysis/exe/names.txt)

// TEMPLATE: EXE 0x692191
// (set<SmartPtr<AsyncSocket> >::_Erase; compiler-generated; name in analysis/exe/names.txt)

// TEMPLATE: EXE 0x6921cd
// (compiler-generated; name in analysis/exe/names.txt)

// TEMPLATE: EXE 0x691eef
// (LobbyCompletion<EELobbyFileTransfer>'s ctor, inline)

// TEMPLATE: EXE 0x691f2a
// (LobbyCompletion<EELobbyFileTransfer>::Run, inline)

// SYNTHETIC: EXE 0x691f39
// (LobbyCompletion<EELobbyFileTransfer>'s scalar deleting destructor)

// TEMPLATE: EXE 0x692202
// (compiler-generated; name in analysis/exe/names.txt)
