// C:\EEX\Low-Level Engine\Utilities\USingleton.h (RECONSTRUCTION; file name and place inferred: the U prefix is the
// engine's Utilities, but the only instantiation in any module is the game's USingleton<EECalamityManager>, RTTI
// .?AV?$USingleton@VEECalamityManager@@@@, vtable 0x43c724). A class with one instance, created on first use.
// - GetInstance is defined outside the class without `inline`: every caller calls the COMDAT (0xc44d) out of line.
// - The virtual dtor is inline: ~EECalamityManager (0xadfe9) ends with the base's vptr store and `sInstance = 0`,
//   and the base's own ??_G (0xaea30) expands it.
#ifndef LLE_USINGLETON_H
#define LLE_USINGLETON_H

template<class T> class USingleton {
public:
    virtual ~USingleton() { USingleton::sInstance = NULL; }
    static T* GetInstance();
#ifdef LLE_EXE_HEADERS
    // exe view only: the instance deleted through its virtual dtor, then cleared (EEDeleteDatabase's COMDAT 0x144a68,
    // Empire Earth.cpp) (provisional name)
    static void DestroyInstance() { delete USingleton::sInstance; USingleton::sInstance = NULL; }
#endif

    static T* sInstance;                    // 0x4e5fb0 for EECalamityManager
};

template<class T> T* USingleton<T>::GetInstance()
{
    if (USingleton::sInstance == NULL)
        USingleton::sInstance = new T;
    return USingleton::sInstance;
}

template<class T> T* USingleton<T>::sInstance = 0;

#endif
