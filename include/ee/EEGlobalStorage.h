// The construction of a global object whose storage is Rust's (ee_game gamecpp global_objects.rs: zeroed bytes,
// aligned to 16, under the object's own symbol: MSVC's mangled name on Windows, the plain name elsewhere).
//
// The headers keep declaring `extern Type name;`, so every user reads the object as before. EE_CONSTRUCT_GLOBAL(Type,
// name, (args)) is the dynamic initializer `Type name(args);` would have had: it constructs the object in place and
// registers its destructor with atexit (not for a trivially destructible type), so the objects are made and destroyed
// in the order of the EE_CONSTRUCT_GLOBAL lines. It also checks that the type fits the Rust storage
// (EE_GAME_SIZE_<name> x EE_GAME_STORAGE_FACTOR bytes, alignment 16 at most). Why the storage is apart: the unit
// that defines a global must name its constructor, so a data unit holding gWorld depended on EEWorld.cpp, and every
// unit that reads gWorld sat in one dependency cycle with it (tools/cpp_deps.py; PORT_LEARNINGS "Cycle cuts").
#ifndef EE_EEGLOBALSTORAGE_H
#define EE_EEGLOBALSTORAGE_H

#include <new>
#include <stdlib.h>
#include <type_traits>


#define EE_GLOBAL_FITS(Type, tag) \
    static_assert(sizeof(Type) <= EE_GAME_SIZE_##tag * EE_GAME_STORAGE_FACTOR && alignof(Type) <= 16, \
                  #tag ": the Rust storage is too small (ee_game gamecpp global_objects.rs EE_GAME_SIZE_" #tag ")")

#define EE_CONSTRUCT_GLOBAL(Type, name, args) \
    EE_GLOBAL_FITS(Type, name); \
    static const bool name##Constructed = (::new (static_cast<void*>(&name)) Type args, \
        std::is_trivially_destructible<Type>::value ? 0 : atexit([] { name.Type::~Type(); }), true)

// The same for an object whose name is not a plain identifier (a class's static member) or whose type is a
// template-id: tag names the storage's size and the constructed flag; the destructor is called through a local alias.
#define EE_CONSTRUCT_GLOBAL_AS(Type, tag, object, args) \
    EE_GLOBAL_FITS(Type, tag); \
    static const bool tag##Constructed = (::new (static_cast<void*>(&(object))) Type args, \
        std::is_trivially_destructible<Type>::value ? 0 : atexit([] { using T = Type; (object).~T(); }), true)

#endif
