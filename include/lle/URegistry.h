// C:\EEX\Low-Level Engine\Utilities\URegistry.h (RECONSTRUCTION). Win32 registry access under one of the predefined
// root keys (HKEY_* values passed as long). URegistry.cpp (0x10081d93..0x1008268d) is the only engine includer: the
// inline ctor/dtor, the implicit copy ctor and ??_E first, then the members. EE-AOC.exe constructs it inline (it
// imports the vftable, not the ctor). Results: 0 ok, 1 native error (ESRecordNativeError), 3 invalid root key.
#ifndef LLE_UREGISTRY_H
#define LLE_UREGISTRY_H

class UWideString;

class LLE_API URegistry {                   // no data: vptr only
public:
    URegistry() {}
    virtual ~URegistry() {}

    long CreateRegistryKey(long inRootKey, UWideString& inKey);
    long DeleteRegistryKey(long inRootKey, UWideString& inKey);
    long DeleteRegistryValue(long inRootKey, UWideString& inKey, UWideString& inValueName);
    long CreateRegistryValue(long inRootKey, UWideString& inKey, UWideString& inValueName, UWideString& inValue);
    long CreateRegistryValue(long inRootKey, UWideString& inKey, UWideString& inValueName, long& inValue);
    long GetRegistryValue(long inRootKey, UWideString& inKey, UWideString& inValueName, long& outValue);
    long GetRegistryValue(long inRootKey, UWideString& inKey, UWideString& inValueName, UWideString& outValue);
    long SetRegistryValue(long inRootKey, UWideString& inKey, UWideString& inValueName, UWideString& inValue);
    long SetRegistryValue(long inRootKey, UWideString& inKey, UWideString& inValueName, long inValue);
#ifdef __clang__
    // VC6 bound temporaries to the non-const reference parameters above; clang does not. These const overloads
    // forward to them (clang build only; remove once the parameters themselves are const).
    long CreateRegistryKey(long inRootKey, const UWideString& inKey)
        { return CreateRegistryKey(inRootKey, const_cast<UWideString&>(inKey)); }
    long DeleteRegistryKey(long inRootKey, const UWideString& inKey)
        { return DeleteRegistryKey(inRootKey, const_cast<UWideString&>(inKey)); }
    long DeleteRegistryValue(long inRootKey, const UWideString& inKey, const UWideString& inValueName)
        { return DeleteRegistryValue(inRootKey, const_cast<UWideString&>(inKey), const_cast<UWideString&>(inValueName)); }
    long CreateRegistryValue(long inRootKey, const UWideString& inKey, const UWideString& inValueName,
                             const UWideString& inValue)
        { return CreateRegistryValue(inRootKey, const_cast<UWideString&>(inKey), const_cast<UWideString&>(inValueName), const_cast<UWideString&>(inValue)); }
    long CreateRegistryValue(long inRootKey, const UWideString& inKey, const UWideString& inValueName, long& inValue)
        { return CreateRegistryValue(inRootKey, const_cast<UWideString&>(inKey), const_cast<UWideString&>(inValueName), inValue); }
    long GetRegistryValue(long inRootKey, const UWideString& inKey, const UWideString& inValueName, long& outValue)
        { return GetRegistryValue(inRootKey, const_cast<UWideString&>(inKey), const_cast<UWideString&>(inValueName), outValue); }
    long GetRegistryValue(long inRootKey, const UWideString& inKey, const UWideString& inValueName,
                          UWideString& outValue)
        { return GetRegistryValue(inRootKey, const_cast<UWideString&>(inKey), const_cast<UWideString&>(inValueName), outValue); }
    long SetRegistryValue(long inRootKey, const UWideString& inKey, const UWideString& inValueName,
                          const UWideString& inValue)
        { return SetRegistryValue(inRootKey, const_cast<UWideString&>(inKey), const_cast<UWideString&>(inValueName), const_cast<UWideString&>(inValue)); }
    long SetRegistryValue(long inRootKey, const UWideString& inKey, const UWideString& inValueName, long inValue)
        { return SetRegistryValue(inRootKey, const_cast<UWideString&>(inKey), const_cast<UWideString&>(inValueName), inValue); }
#endif
};

#endif
