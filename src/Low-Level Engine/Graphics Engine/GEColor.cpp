// C:\EEX\Low-Level Engine\Graphics Engine\GEColor.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// 0x10015151..0x100151b7: only Load/Save (every other GEColor member is header-inline). No init chains: the unit
// includes no header with static objects.
#include <windows.h>

#include "lle/LowLevelEngineCore.h"
#include "lle/FSFile.h"
#include "lle/GEColor.h"
#include "GERustIO.h"

// Reads the colour's red, green and blue from the file; returns the first read error
long GEColor::Load(FSFile& inFile)
{
    const float* theFields[] = {&this->mRed, &this->mGreen, &this->mBlue};
    return GERustFields(inFile, theFields, 3, -1, false, false);
}

// Writes the colour's red, green and blue to the file; returns the first write error
long GEColor::Save(FSFile& inFile)
{
    const float* theFields[] = {&this->mRed, &this->mGreen, &this->mBlue};
    return GERustFields(inFile, theFields, 3, -1, true, false);
}
