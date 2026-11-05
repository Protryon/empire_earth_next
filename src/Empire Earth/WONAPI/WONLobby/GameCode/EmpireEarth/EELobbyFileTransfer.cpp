// C:\EEX\Empire Earth\WONAPI\WONLobby\GameCode\EmpireEarth\EELobbyFileTransfer.cpp (name PROVISIONAL: no __FILE__; the
// class EELobbyFileTransfer (RTTI, vtable 0x84c440) and its WON send/receive operations; it sorts between
// EELobbyFileDialog.cpp and EELobbyGame.cpp). A saved game's transfer between the host and the clients:
// 0x291463..0x29222b. Module flags. r23-ai-planner: the transfer object's own members; the SendFileOp/receive
// operation classes 0x291463..0x2916b3 and 0x291870..0x291b06, Download and the LobbyCompletion/set COMDATs after
// 0x291d59 are not written yet.
#include <windows.h>
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

using namespace WONAPI;

// the host's side of a saved game's transfer: the file in 0x2000-byte chunks over one client's socket (RTTI
// SendFileOp > SocketOp, vtable 0x84c408; 0x207c bytes; r23)
class SendFileOp : public SocketOp {
public:
    SendFileOp(const std::string& inPath, AsyncSocket* inSocket);
    virtual void Virtual08() { CloseFile(); }   // +0x08 0x2916b3
    virtual WONStatus Virtual0C();          // +0x0c 0x2915d2
    virtual WONStatus Virtual10();          // +0x10 0x291613
    void CloseFile();                       // 0x29157e
    bool ReadChunk();                       // 0x291596

    FILE* mFile;                            // +0x5c
    std::string mPath;                      // +0x60
    char mBuffer[0x2000];                   // +0x70
    ReadBuffer mReadBuffer;                 // +0x2070
};

// FUNCTION: EXE 0x691463
SendFileOp::SendFileOp(const std::string& inPath, AsyncSocket* inSocket)
    : SocketOp(inSocket), mPath(inPath), mReadBuffer(mBuffer, 0x2000)
{
    mFile = NULL;
    mB58[1] = true;
}

// TEMPLATE: EXE 0x6914c2
// (SocketOp::SocketOp, inline)

// TEMPLATE: EXE 0x69151c
// (SocketOp::Virtual0C, inline)

// SYNTHETIC: EXE 0x691521
// (SocketOp's scalar deleting destructor)

// TEMPLATE: EXE 0x69153d
// (SocketOp::~SocketOp, inline)

// SYNTHETIC: EXE 0x691555
// (SendFileOp's scalar deleting destructor)

// FUNCTION: EXE 0x69157e
void SendFileOp::CloseFile()
{
    if (mFile != NULL) {
        fclose(mFile);
        mFile = NULL;
    }
}

// FUNCTION: EXE 0x691596
bool SendFileOp::ReadChunk()
{
    int theCount = fread(mBuffer, 1, 0x2000, mFile);
    if (theCount < 0)
        theCount = 0;
    mReadBuffer.SetData(mBuffer, theCount);
    return theCount > 0;
}

// FUNCTION: EXE 0x6915d2
WONStatus SendFileOp::Virtual0C()
{
    if (mPath.size() == 0)
        return 100007;
    if ((mFile = fopen(mPath.c_str(), "rb")) == NULL)
        return 100007;
    ReadChunk();
    return Virtual10();
}

// FUNCTION: EXE 0x691613
WONStatus SendFileOp::Virtual10()
{
    for (;;) {
        if ((unsigned long)mReadBuffer.mL00 < (unsigned long)mReadBuffer.mL08) {
            mSocket->Fn2B1360(TimeLeft());
            unsigned long theSent = 0;
            WONStatus theStatus = mSocket->Send(mReadBuffer.mData + mReadBuffer.mL00,
                mReadBuffer.mL08 - mReadBuffer.mL00, &theSent);
            if ((long)theSent > 0)
                mReadBuffer.ReadBytes(theSent);
            if (theStatus == 100001 && TimeLeft() == 0)
                return 100001;
            if (theStatus != 0)
                return theStatus;
        } else if (!ReadChunk()) {
            CloseFile();
            return 0;
        }
    }
}

// TEMPLATE: EXE 0x6916b3
// (SendFileOp::Virtual08, inline)

// FUNCTION: EXE 0x6916b8
EELobbyFileTransfer::EELobbyFileTransfer()
{
    mLocalSize = 0;
    mLocalChecksum = 0;
    mSize = 0;
    mChecksum = 0;
    mLastProgressTime = 0;
}

// SYNTHETIC: EXE 0x69173f
// (the scalar deleting destructor)

// FUNCTION: EXE 0x69175b
EELobbyFileTransfer::~EELobbyFileTransfer()
{
}

// FUNCTION: EXE 0x6917a2
// the local copy of the saved game: its path, size and checksum
void EELobbyFileTransfer::SetPath(const std::string& inPath)
{
    mPath = inPath;
    mLocalChecksum = 0;
    mLocalSize = 0;
    EELobbyGetSavedGameInfo(mPath.c_str(), mLocalSize, mLocalChecksum);
}

// FUNCTION: EXE 0x6917e9
// the host's saved game: its name, size and checksum
void EELobbyFileTransfer::SetFileInfo(const GUIString& inName, unsigned long inSize, unsigned long inChecksum)
{
    mName = inName;
    mSize = inSize;
    mChecksum = inChecksum;
}

// FUNCTION: EXE 0x69180c
void EELobbyFileTransfer::SetHost(const IPAddr& inAddress)
{
    mHost = inAddress;
}

// TEMPLATE: EXE 0x69181b
// (IPAddr's implicit operator=)

// FUNCTION: EXE 0x691859
bool EELobbyFileTransfer::IsFileHere()
{
    return mSize == mLocalSize && mChecksum == mLocalChecksum;
}

// FUNCTION: EXE 0x691870
// a client's socket closed: it leaves the set (r23)
void EELobbyFileTransfer::OnClientClosed(AsyncOp* inOp)
{
    SmartPtr<AsyncSocket> theSocket = ((SocketOp*)inOp)->mSocket;
    mClients.erase(theSocket);
}

// FUNCTION: EXE 0x6918b2
// a client connected: keep its socket, send it the saved game, then close it (r23)
void EELobbyFileTransfer::OnClientAccepted(AsyncOp* inOp)
{
    if (!inOp->Succeeded())
        return;
    SmartPtr<AsyncSocket> theSocket = ((AcceptOp*)inOp)->mAcceptedSocket;
    mClients.insert(theSocket);
    theSocket->SetCloseCompletion(new LobbyCompletion<EELobbyFileTransfer>(&EELobbyFileTransfer::OnClientClosed, this));
    theSocket->Fn2B4740(new SendFileOp(mPath, NULL), -1);
    theSocket->Fn2B4740(new CloseOp(NULL), -1);
}

// FUNCTION: EXE 0x691983
bool AsyncOp::Succeeded() { return mStatus == 0 && !mB39; }   // defined here, not inline: OnFileReceived calls it out of line

// TEMPLATE: EXE 0x691993
// (CloseOp::Virtual10, inline)

// FUNCTION: EXE 0x69199e
// the transfer's progress (the receiving op's chunk completion), reported to the lobby game at most once a second
void EELobbyFileTransfer::OnProgress(AsyncOp* inOp)
{
    EELobbyGame* theGame = (EELobbyGame*)GetMyLobbyGame();
    if (theGame == NULL)
        return;
    unsigned long theTime = (unsigned long)PlatTimeMilliseconds64();
    if (theTime - mLastProgressTime < 1000)
        return;
    mLastProgressTime = theTime;
    theGame->UpdateTransferProgress(((RecvChunkOp*)inOp)->mRecvOp->mBytesRead, mSize);
}

// FUNCTION: EXE 0x6919dc
void EELobbyFileTransfer::OnFileReceived(AsyncOp* inOp)
{
    EELobbyGame* theGame = (EELobbyGame*)GetMyLobbyGame();
    if (theGame != NULL && inOp->Succeeded())
        theGame->UpdateTransferProgress(mSize, mSize);
}

// FUNCTION: EXE 0x691a09
// serves the saved game on inPort: a listening socket whose accepts start OnClientAccepted (r23)
WONStatus EELobbyFileTransfer::Serve(unsigned short inPort)
{
    Stop();
    mConnection = new AsyncSocket(0);
    WONStatus theStatus = mConnection->Fn2B0D90(inPort, 0);
    if (theStatus != 0)
        return theStatus;
    theStatus = mConnection->Fn2B0FC0(1000);
    if (theStatus != 0)
        return theStatus;
    mConnection->Fn2B3BF0(new LobbyCompletion<EELobbyFileTransfer>(&EELobbyFileTransfer::OnClientAccepted, this));
    mConnection->Fn2B4500(new AcceptOp(NULL));
    return 0;
}

// TEMPLATE: EXE 0x691ab7
// (AcceptOp::Duplicate, inline: emitted with its vtable)

// SYNTHETIC: EXE 0x691aea
// (AcceptOp's scalar deleting destructor)

// TEMPLATE: EXE 0x691b06
// (AcceptOp::~AcceptOp, inline)

// FUNCTION: EXE 0x691b1e
// stops serving: every client's connection, then the listening one
void EELobbyFileTransfer::Stop()
{
    CloseClients();
    if (mConnection.mPtr != NULL) {
        mConnection->Close();
        mConnection = NULL;
    }
}

// FUNCTION: EXE 0x691b40
void EELobbyFileTransfer::CloseClients()
{
    for (std::set<SmartPtr<AsyncSocket> >::iterator it = mClients.begin(); it != mClients.end(); ++it)
        (*it)->Close();
    mClients.clear();
}

// FUNCTION: EXE 0x691b79
// fetches the host's saved game: a socket that connects to the host and receives the file into mPath, reporting
// progress (OnProgress) and the end (OnFileReceived); the socket joins the clients (r24)
void EELobbyFileTransfer::Download()
{
    if (mPath.empty() || !mHost.IsValid())
        return;
    mLastProgressTime = (unsigned long)PlatTimeMilliseconds64();
    SmartPtr<AsyncSocket> theSocket = new AsyncSocket(0);
    theSocket->SetCloseCompletion(new LobbyCompletion<EELobbyFileTransfer>(&EELobbyFileTransfer::OnClientClosed, this));
    SmartPtr<RecvBytesOp> theOp = new RecvBytesOp(0, NULL);
    theOp->mCompletion = new LobbyCompletion<EELobbyFileTransfer>(&EELobbyFileTransfer::OnFileReceived, this);
    theOp->mPath = mPath;
    theOp->Fn2B4F90(new LobbyCompletion<EELobbyFileTransfer>(&EELobbyFileTransfer::OnProgress, this), 10000);
    theSocket->Fn2B4740(new ConnectOp(mHost), -1);
    theSocket->Fn2B4740(theOp.mPtr, -1);
    mClients.insert(theSocket);
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
