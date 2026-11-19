// The storage of a global object apart from its construction (EEGlobals.cpp, Empire Earth.cpp).
//
// EE_GLOBAL_STORAGE(Type, name, "msvc symbol") defines zeroed, aligned bytes under the object's own symbol: what
// `Type name;` would define, without the reference to Type's constructor and destructor. The headers keep declaring
// `extern Type name;`, so every user reads the object as before. EE_CONSTRUCT_GLOBAL(Type, name, (args)) is the
// dynamic initializer `Type name(args);` would have had: it constructs the object in place and registers its
// destructor with atexit (not for a trivially destructible type), so the objects are made and destroyed in the order
// of the EE_CONSTRUCT_GLOBAL lines. Why: the unit that defines a global must name its constructor, so a data unit
// holding gWorld depended on EEWorld.cpp, and every unit that reads gWorld sat in one dependency cycle with it
// (tools/cpp_deps.py; PORT_LEARNINGS "Cycle cuts"). The symbol is the platform's: MSVC's mangled name on Windows, the
// plain name (with Mach-O's underscore on macOS) elsewhere. Globals of the global namespace only.
#ifndef EE_EEGLOBALSTORAGE_H
#define EE_EEGLOBALSTORAGE_H

#include <new>
#include <stdlib.h>
#include <type_traits>

#if defined(_WIN32)
#define EE_GLOBAL_SYMBOL(msvc, plain) __asm__(msvc)
#elif defined(__APPLE__)
#define EE_GLOBAL_SYMBOL(msvc, plain) __asm__("_" plain)
#else
#define EE_GLOBAL_SYMBOL(msvc, plain) __asm__(plain)
#endif

#define EE_GLOBAL_STORAGE(Type, name, msvc) \
    alignas(Type) unsigned char name##Storage[sizeof(Type)] EE_GLOBAL_SYMBOL(msvc, #name)

#define EE_CONSTRUCT_GLOBAL(Type, name, args) \
    static const bool name##Constructed = (::new (static_cast<void*>(&name)) Type args, \
        std::is_trivially_destructible<Type>::value ? 0 : atexit([] { name.Type::~Type(); }), true)

#endif
