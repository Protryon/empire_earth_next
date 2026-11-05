// ee/EERulesWorld.h (new): the value of a rule number (ee/EERules.h): a constant, or a dbworld value read through
// gWorldTable when it is used. Requires ee/EEDatabaseTables.h.
#ifndef EE_EERULESWORLD_H
#define EE_EERULESWORLD_H

#include "ee/EERules.h"

// as a float (a world L value converts as the original's float reads of GetLongValue did)
inline float EERuleFloat(const EERuleNumber& inNumber)
{
    if (inNumber.mWorldRow < 0)
        return inNumber.mFloat;
    EEWorldConstant theID = static_cast<EEWorldConstant>(inNumber.mWorldRow);
    switch (inNumber.mWorldValue) {
    case 0: return gWorldTable.GetFloatValue(theID);
    case 1: return gWorldTable.GetFloatValue2(theID);
    case 2: return (float)gWorldTable.GetLongValue(theID);
    default: return (float)gWorldTable.GetLongValue2(theID);
    }
}

// as an unsigned integer (GetLongValue's type: the original compared the unit limit and wonders unsigned)
inline unsigned long EERuleLong(const EERuleNumber& inNumber)
{
    if (inNumber.mWorldRow < 0)
        return (unsigned long)inNumber.mLong;
    EEWorldConstant theID = static_cast<EEWorldConstant>(inNumber.mWorldRow);
    switch (inNumber.mWorldValue) {
    case 0: return (unsigned long)(long)gWorldTable.GetFloatValue(theID);
    case 1: return (unsigned long)(long)gWorldTable.GetFloatValue2(theID);
    case 2: return gWorldTable.GetLongValue(theID);
    default: return gWorldTable.GetLongValue2(theID);
    }
}

#endif
