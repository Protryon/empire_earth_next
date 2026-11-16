package net.eenext.aoc.controls;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.util.Iterator;

/** One control. Geometry is centre-based, in dp, relative to an anchor point of the screen. */
public final class Widget {
    public static final String BUTTON = "button";
    public static final String MODIFIER = "modifier";
    public static final String JOYSTICK = "joystick";
    public static final String POINTER = "pointerLayer";

    public static final String BIND_NONE = "none";
    public static final String BIND_KEY = "key";
    public static final String BIND_MOUSE = "mouseButton";
    public static final String BIND_GYRO = "toggleGyro";
    public static final String BIND_EDITOR = "editor";
    public static final String BIND_KEYBOARD = "keyboard";   // shows / hides the Android keyboard

    public String id = "";
    public String type = BUTTON;
    public String name = "";
    public int z;
    public boolean locked;
    public boolean visible = true;
    public float alpha = 1f;

    public String ax = "left", ay = "top";
    public float x, y, w = 64, h = 64;

    public String shape = "box";   // box | circle
    public String label = "";

    public String bindKind = BIND_NONE;
    public String bindValue = "";   // an SDL key name, or LEFT / RIGHT / MIDDLE

    public String holdMode = "normal";   // button: normal | toggle; modifier: latch | hold
    public boolean dragLook;             // a finger on this button also moves the pointer

    public String stickMode = "keys";    // joystick: keys (digital 8-way) | pointer (analog cursor / camera edge push)
    public boolean floating;
    public String keyUp = "Up", keyDown = "Down", keyLeft = "Left", keyRight = "Right";
    public float stickSpeed = 1f;        // pointer mode: screen widths per second at full deflection * 0.5

    public JSONObject meta = new JSONObject();   // fields this version does not know, kept as they were

    // runtime state, not saved
    public boolean latched;
    public boolean toggled;
    public boolean down;

    public boolean isModifier() { return MODIFIER.equals(type); }

    public float minSize() { return JOYSTICK.equals(type) ? 48f : 24f; }

    public Widget copy() {
        try { return fromJson(toJson()); } catch (JSONException e) { return new Widget(); }
    }

    private static final String[] KNOWN = {"id", "type", "name", "z", "locked", "visible", "alpha", "geometry", "style",
            "behavior", "bindings", "stick", "meta"};

    public JSONObject toJson() throws JSONException {
        JSONObject o = new JSONObject();
        o.put("id", id).put("type", type).put("name", name).put("z", z).put("locked", locked)
                .put("visible", visible).put("alpha", alpha);
        o.put("geometry", new JSONObject().put("ax", ax).put("ay", ay).put("x", x).put("y", y).put("w", w).put("h", h));
        o.put("style", new JSONObject().put("shape", shape).put("label", label));
        o.put("behavior", new JSONObject().put("holdMode", holdMode).put("dragLook", dragLook));
        JSONObject press = new JSONObject().put("kind", bindKind);
        if (BIND_KEY.equals(bindKind)) press.put("key", bindValue);
        if (BIND_MOUSE.equals(bindKind)) press.put("mouseButton", bindValue);
        o.put("bindings", new JSONObject().put("press", press));
        if (JOYSTICK.equals(type)) {
            o.put("stick", new JSONObject().put("mode", stickMode).put("floating", floating)
                    .put("up", keyUp).put("down", keyDown).put("left", keyLeft).put("right", keyRight)
                    .put("speed", stickSpeed));
        }
        o.put("meta", meta);
        return o;
    }

    private static float num(JSONObject o, String k, float def, float lo, float hi) {
        double v = o.optDouble(k, def);
        if (Double.isNaN(v) || Double.isInfinite(v)) v = def;
        return (float) Math.max(lo, Math.min(hi, v));
    }

    private static String oneOf(String v, String def, String... allowed) {
        for (String a : allowed) if (a.equals(v)) return v;
        return def;
    }

    public static Widget fromJson(JSONObject o) throws JSONException {
        Widget w = new Widget();
        w.id = o.optString("id", "w" + System.nanoTime());
        w.type = oneOf(o.optString("type"), BUTTON, BUTTON, MODIFIER, JOYSTICK, POINTER);
        w.name = o.optString("name", "");
        w.z = o.optInt("z", 0);
        w.locked = o.optBoolean("locked", false);
        w.visible = o.optBoolean("visible", true);
        w.alpha = num(o, "alpha", 1f, 0.05f, 1f);
        JSONObject g = o.optJSONObject("geometry");
        if (g != null) {
            w.ax = oneOf(g.optString("ax"), "left", "left", "center", "right");
            w.ay = oneOf(g.optString("ay"), "top", "top", "center", "bottom");
            w.x = num(g, "x", 0, -4000, 4000);
            w.y = num(g, "y", 0, -4000, 4000);
            w.w = num(g, "w", 64, 8, 4000);
            w.h = num(g, "h", 64, 8, 4000);
        }
        w.w = Math.max(w.w, w.minSize());
        w.h = Math.max(w.h, w.minSize());
        JSONObject s = o.optJSONObject("style");
        if (s != null) {
            w.shape = oneOf(s.optString("shape"), "box", "box", "circle");
            w.label = s.optString("label", "");
        }
        JSONObject b = o.optJSONObject("behavior");
        if (b != null) {
            w.holdMode = b.optString("holdMode", "normal");
            w.dragLook = b.optBoolean("dragLook", false);
        }
        JSONObject bind = o.optJSONObject("bindings");
        JSONObject press = bind == null ? null : bind.optJSONObject("press");
        if (press != null) {
            w.bindKind = oneOf(press.optString("kind"), BIND_NONE, BIND_NONE, BIND_KEY, BIND_MOUSE, BIND_GYRO, BIND_EDITOR, BIND_KEYBOARD);
            w.bindValue = BIND_MOUSE.equals(w.bindKind) ? press.optString("mouseButton", "LEFT") : press.optString("key", "");
        }
        JSONObject st = o.optJSONObject("stick");
        if (st != null) {
            w.stickMode = oneOf(st.optString("mode"), "keys", "keys", "pointer");
            w.floating = st.optBoolean("floating", false);
            w.keyUp = st.optString("up", "Up");
            w.keyDown = st.optString("down", "Down");
            w.keyLeft = st.optString("left", "Left");
            w.keyRight = st.optString("right", "Right");
            w.stickSpeed = num(st, "speed", 1f, 0.05f, 10f);
        }
        JSONObject meta = o.optJSONObject("meta");
        w.meta = meta != null ? meta : new JSONObject();
        // unknown top-level fields are kept inside meta
        Iterator<String> keys = o.keys();
        outer:
        while (keys.hasNext()) {
            String k = keys.next();
            for (String known : KNOWN) if (known.equals(k)) continue outer;
            w.meta.put(k, o.get(k));
        }
        return w;
    }

    public static JSONArray toJsonArray(Iterable<Widget> list) throws JSONException {
        JSONArray a = new JSONArray();
        for (Widget w : list) a.put(w.toJson());
        return a;
    }
}
