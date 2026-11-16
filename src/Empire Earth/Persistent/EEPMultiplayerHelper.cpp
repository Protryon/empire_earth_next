// C:\EEX\Empire Earth\Persistent\EEPMultiplayerHelper.cpp (file name inferred: no __FILE__; the class family and its
// keys "EEPMultiplayerHelper-..."). The saved multiplayer state: 0x14ac35..0x14b22d. Unit flags: the exe's module set.
// Data: the vtable 0x44225c; the eight class static keys share the guard byte 0x531ab8.
// Load/Save keep the synchronous communications' tick counters next to the physics state; the counters come from the
// server or the client (dynamic_cast of gCommunications, slot +0x10 tells which).
#include <windows.h>

#include "lle/LowLevelEngine.h"
#include "lle/FSRestorable.h"
#include "lle/FSPersistent.h"
#include "lle/NEMessage.h"
#include "lle/NESynchronousCommunications.h"
#include "lle/NESynchronousServer.h"
#include "lle/NESynchronousClient.h"

#include "ee/EECommunications.h"
#include "ee/EEPhysics.h"
#include "ee/EEPMultiplayerHelper.h"
#include "ee/EEPersistentTag.h"

const UString EEPMultiplayerHelper::sNemmMessageNumber("EEPMultiplayerHelper-NemmMessageNumber");
const UString EEPMultiplayerHelper::sNescCt("EEPMultiplayerHelper-NescCt");
const UString EEPMultiplayerHelper::sNescPt("EEPMultiplayerHelper-NescPt");
const UString EEPMultiplayerHelper::sNescommCt("EEPMultiplayerHelper-NescommCt");
const UString EEPMultiplayerHelper::sNescommLastTickRecvd("EEPMultiplayerHelper-NescommLastTickRecvd");
const UString EEPMultiplayerHelper::sSNessLastCt("EEPMultiplayerHelper-sNessLastCt");
const UString EEPMultiplayerHelper::sNessCt("EEPMultiplayerHelper-NessCt");
const UString EEPMultiplayerHelper::sNessPt("EEPMultiplayerHelper-NessPt");

// Creates the saved multiplayer-state chunk
EEPMultiplayerHelper::EEPMultiplayerHelper()
    : FSPersistent(kPersistentMultiplayer)
{
}

// Destroys the saved multiplayer-state chunk
EEPMultiplayerHelper::~EEPMultiplayerHelper()
{
}

// Creates an empty saved multiplayer-state chunk (persistent factory)
FSPersistent* EEPMultiplayerHelper::CreateDefault() const
{
    return new EEPMultiplayerHelper;
}

// Reads the physics state and the eight network tick counters; restores only the server's or client's physics tick
// from them (the other counters are read and dropped); returns the first read error
long EEPMultiplayerHelper::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    unsigned long theNescommCt = 0;
    unsigned long theNescommLastTickRecvd = 0;
    unsigned long theSNessLastCt = 0;
    unsigned long theNessCt = 0;
    unsigned long theNessPt = 0;
    unsigned long theNescCt = 0;
    unsigned long theNescPt = 0;
    unsigned long theNemmMessageNumber = 0;
    long theResult = gPhysics->Load(inMajorVersion, inMinorVersion, inFile);

    if (theResult == kErrorOK) {
        theResult = inFile.Read(theNemmMessageNumber);
        if (theResult == kErrorOK) {
            theResult = inFile.Read(theNescCt);
            if (theResult == kErrorOK) {
                theResult = inFile.Read(theNescPt);
                if (theResult == kErrorOK) {
                    theResult = inFile.Read(theNescommCt);
                    if (theResult == kErrorOK) {
                        theResult = inFile.Read(theNescommLastTickRecvd);
                        if (theResult == kErrorOK) {
                            theResult = inFile.Read(theSNessLastCt);
                            if (theResult == kErrorOK) {
                                theResult = inFile.Read(theNessCt);
                                if (theResult == kErrorOK) {
                                    theResult = inFile.Read(theNessPt);
                                    if (theResult == kErrorOK && gCommunications != NULL) {
                                        NESynchronousCommunications* theCommunications =
                                            dynamic_cast<NESynchronousCommunications*>(gCommunications);
                                        if (gCommunications->IsServer())
                                            dynamic_cast<NESynchronousServer*>(gCommunications)->
                                                SetCurrentPhysicsTick(theNessPt);
                                        else
                                            dynamic_cast<NESynchronousClient*>(gCommunications)->
                                                SetCurrentPhysicsTick(theNescPt);
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    return theResult;
}

// Writes the physics state and the server's or client's network tick counters to a save (nothing for versions up to
// 0.12); returns the first write error
long EEPMultiplayerHelper::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    unsigned long theNescommCt = 0;
    unsigned long theNescommLastTickRecvd = 0;
    unsigned long theSNessLastCt = 0;
    unsigned long theNessCt = 0;
    unsigned long theNessPt = 0;
    unsigned long theNescCt = 0;
    unsigned long theNescPt = 0;
    unsigned long theNemmMessageNumber = 0;
    long theResult;
    NESynchronousCommunications* theCommunications;
    NESynchronousServer* theServer;
    NESynchronousClient* theClient;
    if (inMajorVersion <= 0 && inMinorVersion <= 12)
        return kErrorOK;
    // (A branch for exactly version 0.12, unreachable after the test above, was removed.)
    theCommunications = dynamic_cast<NESynchronousCommunications*>(gCommunications);
    theNescommCt = theCommunications->GetNumberOfCTsProcessedInPhysics();
    theNescommLastTickRecvd = theCommunications->GetLastTickReceived();
    if (gCommunications->IsServer()) {
        theServer = dynamic_cast<NESynchronousServer*>(gCommunications);
        theSNessLastCt = theServer->GetLastCTMessageID();
        theNessCt = theServer->GetCurrentCommunicationsTick();
        theNessPt = theServer->GetCurrentPhysicsTick();
        theNemmMessageNumber = theServer->GetMessageManagerCurrentMessageNumber();
    } else {
        theClient = dynamic_cast<NESynchronousClient*>(gCommunications);
        theNescCt = theClient->GetCurrentCommunicationsTick();
        theNescPt = theClient->GetCurrentPhysicsTick();
    }
    theResult = gPhysics->Save(inMajorVersion, inMinorVersion, outFile);
    if (theResult != kErrorOK)
        return theResult;
    theResult = outFile.Write(theNemmMessageNumber);
    if (theResult != kErrorOK)
        return theResult;
    theResult = outFile.Write(theNescCt);
    if (theResult != kErrorOK)
        return theResult;
    theResult = outFile.Write(theNescPt);
    if (theResult != kErrorOK)
        return theResult;
    theResult = outFile.Write(theNescommCt);
    if (theResult != kErrorOK)
        return theResult;
    theResult = outFile.Write(theNescommLastTickRecvd);
    if (theResult != kErrorOK)
        return theResult;
    theResult = outFile.Write(theSNessLastCt);
    if (theResult != kErrorOK)
        return theResult;
    theResult = outFile.Write(theNessCt);
    if (theResult != kErrorOK)
        return theResult;
    return outFile.Write(theNessPt);
}
