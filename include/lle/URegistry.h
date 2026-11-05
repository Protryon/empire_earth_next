// C:\EEX\Low-Level Engine\Utilities\URegistry.h (RECONSTRUCTION). Registry access under one of the predefined
// root keys, through the platform preferences store (platform/PlatPreferences.h: the registry on Win32; the original
// took the HKEY_* handles, now PlatPrefRoot values). URegistry.cpp (0x10081d93..0x1008268d) is the only engine includer: the
// inline ctor/dtor, the implicit copy ctor and ??_E first, then the members. EE-AOC.exe constructs it inline (it
// imports the vftable, not the ctor). Results: 0 ok, 1 native error (ESRecordNativeError), 3 invalid root key.
#ifndef LLE_UREGISTRY_H
#define LLE_UREGISTRY_H

#include "platform/PlatPreferences.h"

class UWideString;

class LLE_API URegistry {                   // no data: vptr only
public:
    URegistry() {}
    virtual ~URegistry() {}

    long CreateRegistryKey(PlatPrefRoot inRootKey, UWideString& inKey);
    long DeleteRegistryKey(PlatPrefRoot inRootKey, UWideString& inKey);
    long DeleteRegistryValue(PlatPrefRoot inRootKey, UWideString& inKey, UWideString& inValueName);
    long CreateRegistryValue(PlatPrefRoot inRootKey, UWideString& inKey, UWideString& inValueName, UWideString& inValue);
    long CreateRegistryValue(PlatPrefRoot inRootKey, UWideString& inKey, UWideString& inValueName, long& inValue);
    long GetRegistryValue(PlatPrefRoot inRootKey, UWideString& inKey, UWideString& inValueName, long& outValue);
    long GetRegistryValue(PlatPrefRoot inRootKey, UWideString& inKey, UWideString& inValueName, UWideString& outValue);
    long SetRegistryValue(PlatPrefRoot inRootKey, UWideString& inKey, UWideString& inValueName, UWideString& inValue);
    long SetRegistryValue(PlatPrefRoot inRootKey, UWideString& inKey, UWideString& inValueName, long inValue);
#ifdef __clang__
    // VC6 bound temporaries to the non-const reference parameters above; clang does not. These const overloads
    // forward to them (clang build only; remove once the parameters themselves are const).
    long CreateRegistryKey(PlatPrefRoot inRootKey, const UWideString& inKey)
        { return CreateRegistryKey(inRootKey, const_cast<UWideString&>(inKey)); }
    long DeleteRegistryKey(PlatPrefRoot inRootKey, const UWideString& inKey)
        { return DeleteRegistryKey(inRootKey, const_cast<UWideString&>(inKey)); }
    long DeleteRegistryValue(PlatPrefRoot inRootKey, const UWideString& inKey, const UWideString& inValueName)
        { return DeleteRegistryValue(inRootKey, const_cast<UWideString&>(inKey), const_cast<UWideString&>(inValueName)); }
    long CreateRegistryValue(PlatPrefRoot inRootKey, const UWideString& inKey, const UWideString& inValueName,
                             const UWideString& inValue)
        { return CreateRegistryValue(inRootKey, const_cast<UWideString&>(inKey), const_cast<UWideString&>(inValueName), const_cast<UWideString&>(inValue)); }
    long CreateRegistryValue(PlatPrefRoot inRootKey, const UWideString& inKey, const UWideString& inValueName, long& inValue)
        { return CreateRegistryValue(inRootKey, const_cast<UWideString&>(inKey), const_cast<UWideString&>(inValueName), inValue); }
    long GetRegistryValue(PlatPrefRoot inRootKey, const UWideString& inKey, const UWideString& inValueName, long& outValue)
        { return GetRegistryValue(inRootKey, const_cast<UWideString&>(inKey), const_cast<UWideString&>(inValueName), outValue); }
    long GetRegistryValue(PlatPrefRoot inRootKey, const UWideString& inKey, const UWideString& inValueName,
                          UWideString& outValue)
        { return GetRegistryValue(inRootKey, const_cast<UWideString&>(inKey), const_cast<UWideString&>(inValueName), outValue); }
    long SetRegistryValue(PlatPrefRoot inRootKey, const UWideString& inKey, const UWideString& inValueName,
                          const UWideString& inValue)
        { return SetRegistryValue(inRootKey, const_cast<UWideString&>(inKey), const_cast<UWideString&>(inValueName), const_cast<UWideString&>(inValue)); }
    long SetRegistryValue(PlatPrefRoot inRootKey, const UWideString& inKey, const UWideString& inValueName, long inValue)
        { return SetRegistryValue(inRootKey, const_cast<UWideString&>(inKey), const_cast<UWideString&>(inValueName), inValue); }
#endif
};

#endif
