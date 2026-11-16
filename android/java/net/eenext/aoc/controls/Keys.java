package net.eenext.aoc.controls;

import java.util.ArrayList;

/** SDL scancode names the key picker offers (any other SDL name can still be typed in), plus combos and the wheel. */
public final class Keys {
    private Keys() {}

    public static final String WHEEL_UP = "Wheel Up";
    public static final String WHEEL_DOWN = "Wheel Down";

    public static final String[] ALL = build();

    private static String[] build() {
        ArrayList<String> k = new ArrayList<>();
        for (char c = 'A'; c <= 'Z'; c++) k.add(String.valueOf(c));
        for (char c = '0'; c <= '9'; c++) k.add(String.valueOf(c));
        for (int i = 1; i <= 12; i++) k.add("F" + i);
        String[] more = {"Up", "Down", "Left", "Right", "Space", "Return", "Escape", "Tab", "Backspace", "Delete",
                "Insert", "Home", "End", "PageUp", "PageDown", "Left Ctrl", "Left Shift", "Left Alt", "Right Ctrl",
                "Right Shift", "Right Alt", "Left GUI", "Menu", "CapsLock", "NumLockClear", "ScrollLock",
                ",", ".", "/", ";", "'", "[", "]", "\\", "-", "=", "`",
                "Keypad 0", "Keypad 1", "Keypad 2", "Keypad 3", "Keypad 4", "Keypad 5", "Keypad 6", "Keypad 7",
                "Keypad 8", "Keypad 9", "Keypad +", "Keypad -", "Keypad *", "Keypad /", "Keypad .", "Keypad Enter",
                "Pause", "PrintScreen", WHEEL_UP, WHEEL_DOWN};
        for (String s : more) k.add(s);
        // combos: control groups (Ctrl+n makes, Shift+n adds), and Ctrl/Shift/Alt with each letter and F-key
        String[] mods = {"Left Ctrl", "Left Shift", "Left Alt"};
        for (String m : mods) {
            for (char c = '0'; c <= '9'; c++) k.add(m + "+" + c);
            for (char c = 'A'; c <= 'Z'; c++) k.add(m + "+" + c);
            for (int i = 1; i <= 12; i++) k.add(m + "+F" + i);
        }
        k.add("Left Ctrl+Left Shift+Delete");
        return k.toArray(new String[0]);
    }

    /** The keys of a combo ("Left Ctrl+1" -> Left Ctrl, 1); a single key (also "Keypad +") is one part. */
    public static String[] split(String key) {
        if (key == null) return new String[0];
        if (key.length() < 2 || !key.contains("+") || key.endsWith(" +")) return new String[]{key};
        ArrayList<String> out = new ArrayList<>();
        int start = 0;
        for (int i = 1; i < key.length(); i++) {
            // a '+' joins two names; "Keypad ++" style (a '+' right after a joiner) is the key itself
            if (key.charAt(i) == '+' && i > start) {
                out.add(key.substring(start, i));
                start = i + 1;
            }
        }
        out.add(key.substring(start));
        return out.toArray(new String[0]);
    }

    /** 0 ctrl, 1 shift, 2 alt, -1 not a modifier. */
    public static int modifierIndex(String key) {
        if (key == null) return -1;
        if (key.endsWith("Ctrl")) return 0;
        if (key.endsWith("Shift")) return 1;
        if (key.endsWith("Alt")) return 2;
        return -1;
    }
}
