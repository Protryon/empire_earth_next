package net.eenext.aoc.controls;

/** JNI entry points of the game library (src/Platform/SDL/PlatAndroid.cpp). */
public final class NativeInput {
    private NativeInput() {}

    public static native void key(String sdlName, boolean down);

    public static native void button(int sdlButton, boolean down);

    public static native void move(float dx, float dy);

    public static native void wheel(float delta);

    public static native void text(String text);
}
