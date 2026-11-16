package net.eenext.aoc.controls;

import java.util.HashSet;

/** Sends the overlay's input to the game through NativeInput; remembers what is held so panicRelease can undo it. */
public final class NativeSink implements InputSink {
    private final HashSet<String> heldKeys = new HashSet<>();
    private final boolean[] heldButtons = new boolean[4];

    private static int buttonNumber(String name) {
        if ("RIGHT".equals(name)) return 3;
        if ("MIDDLE".equals(name)) return 2;
        return 1;
    }

    @Override
    public synchronized void key(boolean down, String key) {
        if (key == null || key.isEmpty()) return;
        // "Wheel Up" / "Wheel Down": one wheel notch on press (zoom), nothing on release
        if (Keys.WHEEL_UP.equals(key) || Keys.WHEEL_DOWN.equals(key)) {
            if (down) wheel(Keys.WHEEL_UP.equals(key) ? 1f : -1f);
            return;
        }
        // a combo ("Left Ctrl+1"): press left to right, release right to left
        String[] parts = Keys.split(key);
        if (parts.length > 1) {
            if (down) for (int i = 0; i < parts.length; i++) key(true, parts[i]);
            else for (int i = parts.length - 1; i >= 0; i--) key(false, parts[i]);
            return;
        }
        if (down) heldKeys.add(key); else heldKeys.remove(key);
        try { NativeInput.key(key, down); } catch (UnsatisfiedLinkError ignored) { }
    }

    @Override
    public void pointerMove(float dx, float dy) {
        try { NativeInput.move(dx, dy); } catch (UnsatisfiedLinkError ignored) { }
    }

    @Override
    public synchronized void pointerButton(String button, boolean down) {
        int n = buttonNumber(button);
        heldButtons[n] = down;
        try { NativeInput.button(n, down); } catch (UnsatisfiedLinkError ignored) { }
    }

    @Override
    public void wheel(float delta) {
        try { NativeInput.wheel(delta); } catch (UnsatisfiedLinkError ignored) { }
    }

    @Override
    public void text(String text) {
        try { NativeInput.text(text); } catch (UnsatisfiedLinkError ignored) { }
    }

    @Override
    public synchronized void panicRelease() {
        for (String k : heldKeys.toArray(new String[0])) {
            try { NativeInput.key(k, false); } catch (UnsatisfiedLinkError ignored) { }
        }
        heldKeys.clear();
        for (int n = 1; n < heldButtons.length; n++) {
            if (heldButtons[n]) {
                try { NativeInput.button(n, false); } catch (UnsatisfiedLinkError ignored) { }
                heldButtons[n] = false;
            }
        }
    }
}
