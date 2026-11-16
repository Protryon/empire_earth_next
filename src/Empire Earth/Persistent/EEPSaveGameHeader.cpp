// C:\EEX\Empire Earth\Persistent\EEPSaveGameHeader.cpp (file name inferred: no __FILE__; the class family and its
// keys "EEPSaveGameHeader-..."). The header of a saved game: 0x14b7a5..0x14bd64. Unit flags: the exe's module set.
// Data: the vtable 0x4422a8; the five class static keys share the guard byte 0x531b58 (bits 1..0x10, `$S`).
#include <windows.h>

#include "lle/LowLevelEngine.h"
#include "lle/FSRestorable.h"

#include "ee/EEGameOptions.h"
#include "ee/EESMMediaStrings.h"
#include "ee/EEPSaveGameHeader.h"
#include "ee/EESaveGameFiles.h"

const UString EEPSaveGameHeader::sMajorVersion("EEPSaveGameHeader-MajorVersion");
const UString EEPSaveGameHeader::sMinorVersion("EEPSaveGameHeader-MinorVersion");
const UString EEPSaveGameHeader::sNumChunks("EEPSaveGameHeader-NumChunks");
const UString EEPSaveGameHeader::sChunkID("EEPSaveGameHeader-ChunkID");
const UString EEPSaveGameHeader::sCompressionType("EEPSaveGameHeader-CompressionType");

// Creates a save header with placeholder name and description, version 0.1 and three players
EEPSaveGameHeader::EEPSaveGameHeader()
{
    this->mName = "Load game name";
    this->mDescription = "Load game description";
    this->mMajorVersion = 0;
    this->mL4E4 = 0;
    this->mChecksum = 0;
    this->mMinorVersion = 1;
    this->mNumPlayers = 3;
}

// Reads the header's name/version part, then its body; returns the first read error
long EEPSaveGameHeader::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    long theResult = this->LoadHeader(inFile);
    if (theResult == kErrorOK) {
        // port (decision D6): the body of a save older than 0.255 holds Windows-1252 text, converted as it loads
        // (the name and description, read before the version, were converted by UString::Load's own test)
        USetLegacyTextLoad(!EESaveTextIsUtf8(this->mMajorVersion, this->mMinorVersion));
        theResult = this->LoadBody(inFile);
        USetLegacyTextLoad(false);
    }
    return theResult;
}

// Reads the save's name, description and version; fails for saves older than version 0.197
long EEPSaveGameHeader::LoadHeader(FSFile& inFile)
{
    long theResult = this->mName.Load(inFile);
    if (theResult != kErrorOK)
        return theResult;
    theResult = this->mDescription.Load(inFile);
    if (theResult != kErrorOK)
        return theResult;
    theResult = inFile.Read(this->mMajorVersion);
    if (theResult != kErrorOK)
        return theResult;
    theResult = inFile.Read(this->mMinorVersion);
    if (theResult == kErrorOK && this->mMajorVersion <= 0 && this->mMinorVersion < 0xc5)
        theResult = kErrorFailed;
    return theResult;
}

// Reads the player names, date, game options, player count, chunk tag list, media strings and checksum; returns the
// first read error
long EEPSaveGameHeader::LoadBody(FSFile& inFile)
{
    long theCount = 0;
    long theID = 0;
    unsigned long theValue = 0;
    long theResult = this->mPlayerNames.Load(inFile);
    if (theResult == kErrorOK) {
        theResult = this->mDate.Load(inFile);
        if (theResult == kErrorOK) {
            theResult = inFile.Read(this->mL4E4);
            if (theResult == kErrorOK) {
                theResult = this->mGameOptions.Load(this->mMajorVersion, this->mMinorVersion, inFile);
                if (theResult == kErrorOK) {
                    theResult = inFile.Read(this->mNumPlayers);
                    if (theResult == kErrorOK) {
                        this->mChunkTypes.clear();
                        theResult = inFile.Read(theCount);
                        if (theResult == kErrorOK) {
                            for (long i = 0; i < theCount && theResult == kErrorOK; i++) {
                                theResult = inFile.Read(theValue);
                                if (theResult == kErrorOK) {
                                    theID = theValue;
                                    this->mChunkTypes.push_back(theID);
                                }
                            }
                            if (theResult == kErrorOK) {
                                theResult = this->mMediaStrings.Load(this->mMajorVersion, this->mMinorVersion, inFile);
                                if (theResult == kErrorOK)
                                    theResult = inFile.Read(this->mChecksum);
                            }
                        }
                    }
                }
            }
        }
    }
    return theResult;
}

// Opens a save file, copies it into memory and reads its header from there; returns the first error
long EEPSaveGameHeader::LoadFromFile(FSFile& inFile)
{
    FSMemoryFile theMemoryFile(1000, true, 1);
    long theResult = inFile.Open(kFileAccessRead);
    if (theResult != kErrorOK)
        return theResult;
    theResult = theMemoryFile.Load(inFile);
    if (theResult == kErrorOK) {
        inFile.Close();
        theResult = this->Load(this->mMajorVersion, this->mMinorVersion, theMemoryFile);
        theMemoryFile.Close();
    }
    return theResult;
}

// Writes the header's name/version part, then its body; returns the first write error
long EEPSaveGameHeader::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    long theResult = this->SaveHeader(outFile);
    if (theResult == kErrorOK)
        theResult = this->SaveBody(outFile);
    return theResult;
}

// Writes the save's name, description and version; returns the first write error
long EEPSaveGameHeader::SaveHeader(FSFile& outFile)
{
    long theResult = this->mName.Save(outFile);
    if (theResult != kErrorOK)
        return theResult;
    theResult = this->mDescription.Save(outFile);
    if (theResult != kErrorOK)
        return theResult;
    theResult = outFile.Write(this->mMajorVersion);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mMinorVersion);
    return theResult;
}

// Writes the player names, date, game options, player count, chunk tag list, media strings and checksum; returns the
// first write error
long EEPSaveGameHeader::SaveBody(FSFile& outFile)
{
    long theResult = this->mPlayerNames.Save(outFile);
    if (theResult != kErrorOK)
        return theResult;
    theResult = this->mDate.Save(outFile);
    if (theResult != kErrorOK)
        return theResult;
    theResult = outFile.Write(this->mL4E4);
    if (theResult != kErrorOK)
        return theResult;
    theResult = this->mGameOptions.Save(outFile);
    if (theResult != kErrorOK)
        return theResult;
    theResult = outFile.Write(this->mNumPlayers);
    if (theResult != kErrorOK)
        return theResult;
    theResult = outFile.Write((long)this->mChunkTypes.size());
    if (theResult != kErrorOK)
        return theResult;
    for (std::vector<long>::iterator it = this->mChunkTypes.begin(); it != this->mChunkTypes.end() && theResult == kErrorOK; ++it)
        theResult = outFile.Write((unsigned long)*it);
    if (theResult != kErrorOK)
        return theResult;
    theResult = this->mMediaStrings.Save(this->mMajorVersion, this->mMinorVersion, outFile);
    if (theResult == kErrorOK)
        theResult = outFile.Write(this->mChecksum);
    return theResult;
}

// Adds nothing to the checksum (empty: folded into a shared empty function)
void EEPSaveGameHeader::GetChecksum(ESChecksum& ioChecksum) const
{
}
