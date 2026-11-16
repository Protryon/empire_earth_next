package net.eenext.aoc.controls;

/** Where the overlay's input goes. The host owns the translation into the game. Key names are SDL scancode names. */
public interface InputSink {
    void key(boolean down, String key);

    /** Relative pointer motion in screen pixels. */
    void pointerMove(float dx, float dy);

    /** button: LEFT, RIGHT or MIDDLE. */
    void pointerButton(String button, boolean down);

    void wheel(float delta);

    void text(String text);

    /** Release every key and button this sink may be holding. */
    void panicRelease();
}
