/* TEST ONLY: a recovering handler set for clang's minimal UBSan runtime (-fsanitize-minimal-runtime), for the
 * sanitizer builds of tools/clang_link.py --sanitize-null (LLVM ships no 32-bit Windows UBSan runtime).
 *
 * Each check calls __ubsan_handle_<check>_minimal() with no arguments; the site is the return address. Every
 * distinct site is reported once through OutputDebugStringA as
 *     EE-UBSAN <check> <module>+<rva hex>
 * (the live-game harness records debug strings in its raw log) and execution continues, so one game collects every
 * site it reaches. Linked into every image of the build; not part of any shipping build.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

static void* volatile seen[4096];

static void report(const char* check, void* site)
{
    char line[300], path[MAX_PATH];
    const char* name = "?";
    HMODULE module = NULL;
    unsigned i, h = (unsigned)(((ULONG_PTR)site >> 2) * 2654435761u) & 4095;
    for (i = 0; i < 4096; i++, h = (h + 1) & 4095) {
        void* old = InterlockedCompareExchangePointer((PVOID volatile*)&seen[h], site, NULL);
        if (old == NULL)
            break;
        if (old == site)
            return;
    }
    if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           (LPCSTR)site, &module) && GetModuleFileNameA(module, path, sizeof path)) {
        const char* p;
        name = path;
        for (p = path; *p; p++)
            if (*p == '\\' || *p == '/')
                name = p + 1;
    }
    {   /* "EE-UBSAN <check> <module>+<rva hex>\n" without user32's wsprintf (not every image links user32) */
        char* o = line;
        const char* s;
        unsigned long rva = (unsigned long)((char*)site - (char*)module);
        int k;
        for (s = "EE-UBSAN "; *s; ) *o++ = *s++;
        for (s = check; *s && o < line + 60; ) *o++ = *s++;
        *o++ = ' ';
        for (s = name; *s && o < line + 280; ) *o++ = *s++;
        *o++ = '+';
        for (k = 28; k >= 0; k -= 4) *o++ = "0123456789abcdef"[(rva >> k) & 15];
        *o++ = '\n';
        *o = 0;
    }
    OutputDebugStringA(line);
}

#define HANDLER(check)                                                                                               \
    void __ubsan_handle_##check##_minimal(void) { report(#check, __builtin_return_address(0)); }                    \
    void __ubsan_handle_##check##_minimal_abort(void) { report(#check, __builtin_return_address(0)); }

HANDLER(type_mismatch)
HANDLER(pointer_overflow)
HANDLER(nonnull_arg)
HANDLER(nonnull_return)
HANDLER(nullability_arg)
HANDLER(nullability_return)
HANDLER(invalid_builtin)
