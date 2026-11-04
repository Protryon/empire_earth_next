// C:\EEX\Low-Level Engine\Network Engine\NEProtocolConfiguration.cpp
// Unit flags (docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
#include <windows.h>

#include "lle/LowLevelEngine.h"

// Compiler-generated for this unit (names in analysis/lle/names.txt):
// SYNTHETIC: LLE 0x10044c71
// SYNTHETIC: LLE 0x10044cda
// SYNTHETIC: LLE 0x10044d02

// FUNCTION: LLE 0x10044c75
// Creates an empty protocol configuration (no parameters)
NEProtocolConfiguration::NEProtocolConfiguration()
{
    this->mProtocolType = 0;
    this->mParameterCount = 0;
    this->mParameters = NULL;
}

// FUNCTION: LLE 0x10044c82
// Creates a protocol configuration of a given type with a number of blank parameters
NEProtocolConfiguration::NEProtocolConfiguration(unsigned long inParameterCount, unsigned long inProtocolType)
{
    this->mProtocolType = inProtocolType;
    this->mParameterCount = inParameterCount;
    this->mParameters = new NEProtocolParameter[inParameterCount];
}

// FUNCTION: LLE 0x10044d72
// Copies a protocol configuration: type, and each parameter's name plus either its value or its value-name list
NEProtocolConfiguration::NEProtocolConfiguration(const NEProtocolConfiguration& inConfiguration)
{
    unsigned long i;
    unsigned long j;
    unsigned long theCount;
    unsigned long theNameCount;
    this->mProtocolType = inConfiguration.mProtocolType;
    this->mParameterCount = theCount = inConfiguration.mParameterCount;
    this->mParameters = new NEProtocolParameter[theCount];
    for (i = 0; i < theCount; i++) {
        delete[] this->mParameters[i].mDiscreteValueNames;
        this->mParameters[i].mDiscreteValueNames = NULL;
        this->mParameters[i].mIsDiscrete = inConfiguration.mParameters[i].mIsDiscrete;
        this->mParameters[i].mName = inConfiguration.mParameters[i].mName;
        if (this->mParameters[i].mIsDiscrete) {
            this->mParameters[i].mDiscreteValueCount = theNameCount = inConfiguration.mParameters[i].mDiscreteValueCount;
            this->mParameters[i].mDiscreteValue = inConfiguration.mParameters[i].mDiscreteValue;
            UString::SetDefaultAllocationLength(64);
            UString* theNames = new UString[theNameCount];
            this->mParameters[i].mDiscreteValueNames = theNames;
            UString* theSourceNames = inConfiguration.mParameters[i].mDiscreteValueNames;
            for (j = 0; j < theNameCount; j++)
                theNames[j] = theSourceNames[j];
        } else {
            this->mParameters[i].mValue = inConfiguration.mParameters[i].mValue;
            this->mParameters[i].mDiscreteValueCount = 0;
            this->mParameters[i].mDiscreteValue = 0;
        }
    }
}

// FUNCTION: LLE 0x10044edf
// Destroys the configuration, freeing every parameter's value names and the parameter array
NEProtocolConfiguration::~NEProtocolConfiguration()
{
    unsigned long i;
    for (i = 0; i < this->mParameterCount; i++)
        delete[] this->mParameters[i].mDiscreteValueNames;
    delete[] this->mParameters;
}

// FUNCTION: LLE 0x10044f16
// Returns the selected value index of a discrete parameter; fails for a free-value parameter
long NEProtocolConfiguration::GetParameterDiscreteValue(unsigned long inParameter, unsigned long& outValue) const
{
    if (inParameter < this->mParameterCount) {
        if (this->mParameters[inParameter].mIsDiscrete) {
            outValue = this->mParameters[inParameter].mDiscreteValue;
            return kErrorOK;
        }
        return kErrorFailed;
    }
    return kErrorBeyondEnd;
}

// FUNCTION: LLE 0x10044f46
// Returns how many values a discrete parameter offers; fails for a free-value parameter
long NEProtocolConfiguration::GetParameterDiscreteValueCount(unsigned long inParameter, unsigned long& outCount) const
{
    if (inParameter < this->mParameterCount) {
        if (this->mParameters[inParameter].mIsDiscrete) {
            outCount = this->mParameters[inParameter].mDiscreteValueCount;
            return kErrorOK;
        }
        return kErrorFailed;
    }
    return kErrorBeyondEnd;
}

// FUNCTION: LLE 0x10044f76
// Returns the name of one value of a discrete parameter; fails if not discrete or the value is out of range
long NEProtocolConfiguration::GetParameterDiscreteValueName(unsigned long inParameter, unsigned long inValue,
                                                            UString& outName) const
{
    if (inParameter < this->mParameterCount) {
        if (this->mParameters[inParameter].mIsDiscrete && inValue < this->mParameters[inParameter].mDiscreteValueCount) {
            outName = this->mParameters[inParameter].mDiscreteValueNames[inValue];
            return kErrorOK;
        }
        return kErrorFailed;
    }
    return kErrorBeyondEnd;
}

// FUNCTION: LLE 0x10044fb5
// Returns a parameter's name
long NEProtocolConfiguration::GetParameterName(unsigned long inParameter, UString& outName) const
{
    if (inParameter < this->mParameterCount) {
        outName = this->mParameters[inParameter].mName;
        return kErrorOK;
    }
    return kErrorBeyondEnd;
}

// FUNCTION: LLE 0x10044fde
// Returns a free-value parameter's text value; fails for a discrete parameter
long NEProtocolConfiguration::GetParameterValue(unsigned long inParameter, UString& outValue) const
{
    if (inParameter < this->mParameterCount) {
        if (!this->mParameters[inParameter].mIsDiscrete) {
            outValue = this->mParameters[inParameter].mValue;
            return kErrorOK;
        }
        return kErrorFailed;
    }
    return kErrorBeyondEnd;
}

// FUNCTION: LLE 0x10045012
// Returns whether a parameter is discrete (a choice from named values)
long NEProtocolConfiguration::IsParameterDiscrete(unsigned long inParameter, bool& outIsDiscrete) const
{
    if (inParameter < this->mParameterCount) {
        outIsDiscrete = this->mParameters[inParameter].mIsDiscrete;
        return kErrorOK;
    }
    return kErrorBeyondEnd;
}

// FUNCTION: LLE 0x10045037
// Loads the configuration from a file, replacing the old parameters; rewinds the file and returns failure on error
long NEProtocolConfiguration::Load(FSFileSpec& inFile)
{
    long thePosition;
    long theResult;
    unsigned long i;
    unsigned long j;
    unsigned long theCount;
    for (i = 0; i < this->mParameterCount; i++) {
        delete[] this->mParameters[i].mDiscreteValueNames;
        this->mParameters[i].mDiscreteValueNames = NULL;
    }
    delete[] this->mParameters;
    // Fix: the freed array used to stay in mParameters (with a count possibly overwritten by a failed read) when
    // the load failed before allocating a new one, so the destructor or the next Load freed it again.
    this->mParameters = NULL;
    this->mParameterCount = 0;
    if (inFile.GetPosition(thePosition) == kErrorOK) {
    if (inFile.Read(this->mProtocolType) != kErrorOK || (theResult = inFile.Read(this->mParameterCount)) != kErrorOK) {
        this->mParameterCount = 0;
        theResult = kErrorFailed;
    } else {
        theCount = this->mParameterCount;
        this->mParameters = new NEProtocolParameter[theCount];
        for (i = 0; i < theCount; i++) {
            NEProtocolParameter& theParameter = this->mParameters[i];
            if (theParameter.mName.Load(inFile) != kErrorOK) {
                theResult = kErrorFailed;
                break;
            }
            if (inFile.Read(theParameter.mIsDiscrete) != kErrorOK) {
                theResult = kErrorFailed;
                break;
            }
            if (theParameter.mIsDiscrete) {
                if (inFile.Read(theParameter.mDiscreteValue) != kErrorOK ||
                    (theResult = inFile.Read(theParameter.mDiscreteValueCount)) != kErrorOK) {
                    theResult = kErrorFailed;
                    break;
                }
                UString::SetDefaultAllocationLength(64);
                theParameter.mDiscreteValueNames = new UString[theParameter.mDiscreteValueCount];
                for (j = 0; j < theParameter.mDiscreteValueCount; j++)
                    if ((theResult = theParameter.mDiscreteValueNames[j].Load(inFile)) != kErrorOK)
                        break;
                if (theResult != kErrorOK) {
                    theResult = kErrorFailed;
                    break;
                }
            } else if ((theResult = theParameter.mValue.Load(inFile)) != kErrorOK) {
                theResult = kErrorFailed;
                break;
            }
        }
    }
    if (theResult != kErrorOK)
        inFile.SetPosition(thePosition, kFileOriginBegin, false);
    } else
        theResult = kErrorFailed;
    return theResult;
}

// FUNCTION: LLE 0x10045253
// Writes the configuration (type, count, each parameter) to a file; on error rewinds and returns failure
long NEProtocolConfiguration::Save(FSFileSpec& inFile) const
{
    long thePosition;
    long theResult;
    unsigned long i;
    unsigned long j;
    unsigned long theCount;
    if (inFile.GetPosition(thePosition) == kErrorOK) {
    if (inFile.Write(this->mProtocolType) != kErrorOK || (theResult = inFile.Write(this->mParameterCount)) != kErrorOK) {
        theResult = kErrorFailed;
    } else {
        theCount = this->mParameterCount;
        for (i = 0; i < theCount; i++) {
            const NEProtocolParameter& theParameter = this->mParameters[i];
            if (theParameter.mName.Save(inFile) != kErrorOK) {
                theResult = kErrorFailed;
                break;
            }
            if (inFile.Write(theParameter.mIsDiscrete) != kErrorOK) {
                theResult = kErrorFailed;
                break;
            }
            if (theParameter.mIsDiscrete) {
                if (inFile.Write(theParameter.mDiscreteValue) != kErrorOK ||
                    (theResult = inFile.Write(theParameter.mDiscreteValueCount)) != kErrorOK) {
                    theResult = kErrorFailed;
                    break;
                }
                for (j = 0; j < theParameter.mDiscreteValueCount; j++)
                    if ((theResult = theParameter.mDiscreteValueNames[j].Save(inFile)) != kErrorOK) {
                        theResult = kErrorFailed;
                        break;
                    }
                if (theResult != kErrorOK)
                    break;
            } else if ((theResult = theParameter.mValue.Save(inFile)) != kErrorOK) {
                theResult = kErrorFailed;
                break;
            }
        }
    }
    if (theResult != kErrorOK)
        inFile.SetPosition(thePosition, kFileOriginBegin, false);
    } else
        theResult = kErrorFailed;
    return theResult;
}

// FUNCTION: LLE 0x1004538a
// Selects a value of a discrete parameter; fails if not discrete, beyond-end if the value is out of range
long NEProtocolConfiguration::SetParameterDiscreteValue(unsigned long inParameter, unsigned long inValue)
{
    if (inParameter < this->mParameterCount) {
        if (this->mParameters[inParameter].mIsDiscrete) {
            if (inValue < this->mParameters[inParameter].mDiscreteValueCount) {
                this->mParameters[inParameter].mDiscreteValue = inValue;
                return kErrorOK;
            }
            return kErrorBeyondEnd;
        }
        return kErrorFailed;
    }
    return kErrorBeyondEnd;
}

// FUNCTION: LLE 0x100453bd
// Renames a parameter
long NEProtocolConfiguration::SetParameterName(unsigned long inParameter, const UString& inName)
{
    if (inParameter < this->mParameterCount) {
        this->mParameters[inParameter].mName = inName;
        return kErrorOK;
    }
    return kErrorBeyondEnd;
}

// FUNCTION: LLE 0x100453e5
// Turns a parameter into a free-value one, dropping its value-name list
long NEProtocolConfiguration::SetParameterToArbitrary(unsigned long inParameter)
{
    if (inParameter < this->mParameterCount) {
        if (this->mParameters[inParameter].mIsDiscrete) {
            delete[] this->mParameters[inParameter].mDiscreteValueNames;
            this->mParameters[inParameter].mDiscreteValueNames = NULL;
            this->mParameters[inParameter].mDiscreteValueCount = 0;
            this->mParameters[inParameter].mDiscreteValue = 0;
            this->mParameters[inParameter].mIsDiscrete = false;
        }
        return kErrorOK;
    }
    return kErrorBeyondEnd;
}

// FUNCTION: LLE 0x10045436
// Turns a parameter into a discrete one with the given value names, selecting the first
long NEProtocolConfiguration::SetParameterToDiscrete(unsigned long inParameter, unsigned long inCount,
                                                     UString* inNames)
{
    unsigned long i;
    if (inParameter < this->mParameterCount) {
        if (this->mParameters[inParameter].mIsDiscrete)
            delete[] this->mParameters[inParameter].mDiscreteValueNames;
        this->mParameters[inParameter].mDiscreteValueCount = inCount;
        this->mParameters[inParameter].mDiscreteValue = 0;
        this->mParameters[inParameter].mIsDiscrete = true;
        UString::SetDefaultAllocationLength(64);
        this->mParameters[inParameter].mDiscreteValueNames = new UString[inCount];
        for (i = 0; i < inCount; i++)
            this->mParameters[inParameter].mDiscreteValueNames[i] = inNames[i];
        return kErrorOK;
    }
    return kErrorBeyondEnd;
}

// FUNCTION: LLE 0x10045502
// Sets a free-value parameter's text value; fails for a discrete parameter
long NEProtocolConfiguration::SetParameterValue(unsigned long inParameter, const UString& inValue)
{
    if (inParameter < this->mParameterCount) {
        if (!this->mParameters[inParameter].mIsDiscrete) {
            this->mParameters[inParameter].mValue = inValue;
            return kErrorOK;
        }
        return kErrorFailed;
    }
    return kErrorBeyondEnd;
}
