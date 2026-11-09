/* The game's window icon off Windows (Windows: the exe's resource): the pixels of the generated gen/PlatIconData.h
   (tools/xbuild.py), handed to the window layer (rust/crates/ee_plat_sdl) before any window exists. Compiled only into
   the POSIX game; a program without the generated header gets no icon. */
#if defined(__has_include)
#if __has_include("PlatIconData.h")
#include "PlatIconData.h"
#include "platform/PlatWindow.h"

__attribute__((constructor)) static void RegisterIcon(void)
{
    PlatWindowSetIcon(kPlatIconBGRA, kPlatIconSize);
}
#endif
#endif
