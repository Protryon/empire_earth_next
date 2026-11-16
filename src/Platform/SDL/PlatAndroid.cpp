// Android glue (compiled for android-arm64 only): the entry SDLActivity calls, and the JNI input bridge the Java
// touch-controls overlay (android/java/net/eenext/aoc/controls) drives. The bridge pushes ordinary SDL events, so
// PlatWindow.cpp translates them exactly like mouse and keyboard events from a desktop.
#ifdef __ANDROID__

#include <jni.h>
#include <limits.h>
#include <SDL3/SDL.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <atomic>

#include "platform/PlatPath.h"

extern "C" int main(int argc, char** argv);

// SDL's internal input entry points (SDL_mouse_c.h, SDL_keyboard_c.h, SDL_android.h; SDL is linked in statically).
// Unlike SDL_PushEvent they also update the state the game polls (SDL_GetMouseState for edge scrolling and the
// cursor, SDL_GetKeyboardState for held keys), exactly as SDL's own Android mouse and keyboard code does.
#define SDL_DEFAULT_MOUSE_ID 1      // as in SDL_mouse_c.h
#define SDL_DEFAULT_KEYBOARD_ID 1   // as in SDL_keyboard_c.h
extern "C" {
void SDL_SendMouseMotion(Uint64 timestamp, SDL_Window* window, SDL_MouseID mouseID, bool relative, float x, float y);
void SDL_SendMouseButton(Uint64 timestamp, SDL_Window* window, SDL_MouseID mouseID, Uint8 button, bool down);
void SDL_SendMouseWheel(Uint64 timestamp, SDL_Window* window, SDL_MouseID mouseID, float x, float y,
                        SDL_MouseWheelDirection direction);
bool SDL_SendKeyboardKey(Uint64 timestamp, SDL_KeyboardID keyboardID, int rawcode, SDL_Scancode scancode, bool down);
void SDL_SendKeyboardText(const char* text);
void Android_LockActivityMutex(void);
void Android_UnlockActivityMutex(void);
}

// SDLActivity loads libmain.so and calls this by name (SDL_main). The game's own main stays unchanged.
extern "C" __attribute__((visibility("default"))) int SDL_main(int argc, char** argv)
{
    // the data root is the app's external files folder (/sdcard/Android/data/<package>/files): adb push reaches it
    // with no permission. EE_DATA_ROOT set by the launcher wins.
    if (!getenv("EE_DATA_ROOT")) {
        const char* thePath = SDL_GetAndroidExternalStoragePath();
        if (thePath && *thePath)
            setenv("EE_DATA_ROOT", thePath, 1);
    }
    // The game's paths are Windows paths with a case-insensitive lookup, which lists each parent folder. Android 11
    // forbids listing .../Android/data, so the game's C: volume is the data root itself: every game path then starts
    // inside a folder the app may list ("C:\Data\db\...").
    if (const char* theRoot = getenv("EE_DATA_ROOT")) {
        char theReal[PATH_MAX + 1];
        PlatPathSetVolumeRoot('C', realpath(theRoot, theReal) ? theReal : theRoot);
    }
    SDL_SetHint(SDL_HINT_ENABLE_SCREEN_KEYBOARD, "0");   // the game starts text input at launch: no Android keyboard
    SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");  // touches belong to the Java overlay, never become the mouse
    return main(argc, argv);
}

namespace {

std::atomic<float> sCursorX{-1.0f};   // negative: not placed yet, the first use puts it at the window's centre
std::atomic<float> sCursorY{-1.0f};

SDL_Window* FocusWindow(int* outW, int* outH)
{
    SDL_Window* theWindow = SDL_GetKeyboardFocus();
    if (!theWindow) {
        int theCount = 0;
        SDL_Window** theWindows = SDL_GetWindows(&theCount);
        if (theWindows && theCount > 0)
            theWindow = theWindows[0];
        SDL_free(theWindows);
    }
    *outW = 1;
    *outH = 1;
    if (theWindow)
        SDL_GetWindowSize(theWindow, outW, outH);
    return theWindow;
}

// the Java UI thread calls in; SDL's own Android input holds the activity mutex the same way
struct ActivityLock {
    ActivityLock() { Android_LockActivityMutex(); }
    ~ActivityLock() { Android_UnlockActivityMutex(); }
};

void PlaceCursor(int w, int h)
{
    if (sCursorX.load() < 0) {   // start where a player expects the cursor: the middle, not the corner
        sCursorX.store(w * 0.5f);
        sCursorY.store(h * 0.5f);
    }
}

}   // namespace

extern "C" {

// NativeInput.key(String sdlName, boolean down): SDL scancode names ("A", "Left Ctrl", "Return", "F5", "Up" ...)
JNIEXPORT void JNICALL Java_net_eenext_aoc_controls_NativeInput_key(JNIEnv* env, jclass, jstring inName, jboolean inDown)
{
    const char* theName = env->GetStringUTFChars(inName, nullptr);
    SDL_Scancode theCode = theName ? SDL_GetScancodeFromName(theName) : SDL_SCANCODE_UNKNOWN;
    if (theName)
        env->ReleaseStringUTFChars(inName, theName);
    if (theCode == SDL_SCANCODE_UNKNOWN)
        return;
    ActivityLock theLock;
    SDL_SendKeyboardKey(0, SDL_DEFAULT_KEYBOARD_ID, 0, theCode, inDown);   // also keeps SDL's key and modifier state
}

// NativeInput.button(int sdlButton, boolean down): 1 left, 2 middle, 3 right
JNIEXPORT void JNICALL Java_net_eenext_aoc_controls_NativeInput_button(JNIEnv*, jclass, jint inButton, jboolean inDown)
{
    ActivityLock theLock;
    int w, h;
    SDL_Window* theWindow = FocusWindow(&w, &h);
    if (!theWindow)
        return;
    PlaceCursor(w, h);
    if (!SDL_GetWindowRelativeMouseMode(theWindow))   // in relative mode a position would read as a jump
        SDL_SendMouseMotion(0, theWindow, SDL_DEFAULT_MOUSE_ID, false, sCursorX.load(), sCursorY.load());
    SDL_SendMouseButton(0, theWindow, SDL_DEFAULT_MOUSE_ID, (Uint8)inButton, inDown);
}

// NativeInput.move(float dx, float dy): relative motion; the cursor stays inside the window
JNIEXPORT void JNICALL Java_net_eenext_aoc_controls_NativeInput_move(JNIEnv*, jclass, jfloat inDX, jfloat inDY)
{
    ActivityLock theLock;
    int w, h;
    SDL_Window* theWindow = FocusWindow(&w, &h);
    if (!theWindow)
        return;
    PlaceCursor(w, h);
    // the game takes the mouse in relative mode (ISMouse::Resume) and moves its own pointer by the deltas, clamping
    // it itself: send every delta unclamped (clamping here loses motion at the edges and the two pointers drift)
    bool theRelative = SDL_GetWindowRelativeMouseMode(theWindow);
    if (theRelative) {
        // the game reads whole pixels (PlatWindow truncates xrel): carry the fractions so slow moves add up
        static float sRestX = 0.0f, sRestY = 0.0f;
        sRestX += inDX;
        sRestY += inDY;
        float theStepX = truncf(sRestX), theStepY = truncf(sRestY);
        sRestX -= theStepX;
        sRestY -= theStepY;
        if (theStepX != 0.0f || theStepY != 0.0f)
            SDL_SendMouseMotion(0, theWindow, SDL_DEFAULT_MOUSE_ID, true, theStepX, theStepY);
    }
    float theX = sCursorX.load() + inDX;
    float theY = sCursorY.load() + inDY;
    theX = theX < 0 ? 0 : (theX > (float)(w - 1) ? (float)(w - 1) : theX);
    theY = theY < 0 ? 0 : (theY > (float)(h - 1) ? (float)(h - 1) : theY);
    sCursorX.store(theX);
    sCursorY.store(theY);
    if (!theRelative)
        SDL_SendMouseMotion(0, theWindow, SDL_DEFAULT_MOUSE_ID, false, theX, theY);
}

JNIEXPORT void JNICALL Java_net_eenext_aoc_controls_NativeInput_wheel(JNIEnv*, jclass, jfloat inDelta)
{
    ActivityLock theLock;
    int w, h;
    SDL_Window* theWindow = FocusWindow(&w, &h);
    if (theWindow)
        SDL_SendMouseWheel(0, theWindow, SDL_DEFAULT_MOUSE_ID, 0.0f, inDelta, SDL_MOUSEWHEEL_NORMAL);
}

JNIEXPORT void JNICALL Java_net_eenext_aoc_controls_NativeInput_text(JNIEnv* env, jclass, jstring inText)
{
    const char* theText = env->GetStringUTFChars(inText, nullptr);
    if (!theText)
        return;
    {
        ActivityLock theLock;
        SDL_SendKeyboardText(theText);
    }
    env->ReleaseStringUTFChars(inText, theText);
}

}   // extern "C"

#endif   // __ANDROID__
