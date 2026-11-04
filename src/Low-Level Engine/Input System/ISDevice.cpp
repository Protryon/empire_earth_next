// C:\EEX\Low-Level Engine\Input System\ISDevice.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// 0x10039715..0x10039a8d: the sDevices init chain, the ctor (registers the device), dtor and GetDevice, then the
// vector<ISDevice*> COMDATs (push_back, insert(P, M, X)).
#include <windows.h>

#include "lle/LowLevelEngineCore.h"
#include "lle/UString.h"
#include "lle/ISDevice.h"

std::vector<ISDevice*> ISDevice::sDevices;

// FUNCTION: LLE 0x1003975e
// Creates an input device with a name and adds it to the global device list
ISDevice::ISDevice(const UString& inName)
    : mName(inName)
{
    this->mSuspended = false;
    ISDevice::sDevices.push_back(this);
}

// FUNCTION: LLE 0x10039791
// Destroys the input device and removes it from the device list
ISDevice::~ISDevice()
{
    // Fixed: the destroyed device used to stay in the list, where GetDevice handed out a dangling pointer
    for (unsigned long i = 0; i < ISDevice::sDevices.size(); i++)
        if (ISDevice::sDevices[i] == this) {
            ISDevice::sDevices.erase(ISDevice::sDevices.begin() + i);
            break;
        }
}

// FUNCTION: LLE 0x1003979f
// Returns the input device at an index in the device list; beyond-end error past it
long ISDevice::GetDevice(unsigned long inIndex, ISDevice*& outDevice)
{
    if (inIndex < ISDevice::sDevices.size()) {
        outDevice = ISDevice::sDevices[inIndex];
        return kErrorOK;
    }
    return kErrorBeyondEnd;
}

// Compiler-generated for this unit (names in analysis/lle/names.txt):

// Header-inline members emitted here (markers at the end):
// SYNTHETIC: LLE 0x10039715
// SYNTHETIC: LLE 0x1003971f
// SYNTHETIC: LLE 0x10039737
// SYNTHETIC: LLE 0x10039743
// SYNTHETIC: LLE 0x100397cb
// SYNTHETIC: LLE 0x100397f7
