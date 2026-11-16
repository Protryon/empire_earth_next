package net.eenext.aoc.controls;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;

/** The whole control layout and its settings; one JSON file per (game, orientation). A blank profile is valid. */
public final class Profile {
    public static final int SCHEMA = 1;

    public String id = "empire-earth/landscape";
    public String gameId = "empire-earth";
    public String name = "Landscape profile";
    public int revision = 1;

    public float gridDp = 10f;
    public boolean snap = true;

    public float globalScale = 1f;
    public float globalOpacity = 1f;
    public boolean haptics = false;   // kept in the JSON; presses never vibrate

    public float pointerSensitivity = 1f;
    public boolean pointerAccelEnabled = true;   // finger speed changes how far the cursor goes
    public float pointerAcceleration = 0.5f;     // how strongly
    public boolean gyroEnabled = false;
    public float gyroSensitivity = 1f;
    public float pointerRatioY = 1f;      // vertical cursor speed relative to horizontal
    public float gyroScaleX = 1f, gyroScaleY = 1f;
    public boolean gyroInvertX = false, gyroInvertY = false;
    public float gyroDeadzone = 0.03f;    // rad/s below which the phone counts as still
    public float gyroSmoothing = 0.3f;    // 0 raw .. 0.9 heavy
    public boolean gyroYawAxis = false;   // horizontal cursor from yaw (turning) instead of the default axis
    public boolean releaseAllOnPause = true;

    public final ArrayList<Widget> widgets = new ArrayList<>();
    public JSONObject meta = new JSONObject();

    private static final Comparator<Widget> BY_Z = (a, b) -> Integer.compare(a.z, b.z);

    public void sortByZ() { Collections.sort(widgets, BY_Z); }

    public Widget find(String id) {
        for (Widget w : widgets) if (w.id.equals(id)) return w;
        return null;
    }

    public boolean hasEditorButton() {
        for (Widget w : widgets) if (Widget.BIND_EDITOR.equals(w.bindKind)) return true;
        return false;
    }

    public String newId(String prefix) {
        int n = widgets.size() + 1;
        while (find(prefix + "-" + n) != null) n++;
        return prefix + "-" + n;
    }

    public int topZ() {
        int z = 0;
        for (Widget w : widgets) z = Math.max(z, w.z);
        return z + 1;
    }

    public JSONObject toJson() throws JSONException {
        JSONObject o = new JSONObject();
        o.put("schemaVersion", SCHEMA).put("id", id).put("gameId", gameId).put("name", name)
                .put("orientation", "landscape").put("revision", revision)
                .put("updatedAtEpochMs", System.currentTimeMillis());
        o.put("grid", new JSONObject().put("unitDp", gridDp).put("snap", snap));
        o.put("ui", new JSONObject().put("globalScale", globalScale).put("globalOpacity", globalOpacity)
                .put("haptics", haptics));
        o.put("runtime", new JSONObject().put("cursorMode", "relative").put("pointerSensitivity", pointerSensitivity)
                .put("pointerAcceleration", pointerAcceleration).put("pointerAccelEnabled", pointerAccelEnabled).put("gyroEnabled", gyroEnabled)
                .put("gyroSensitivity", gyroSensitivity).put("releaseAllOnPause", releaseAllOnPause)
                .put("pointerRatioY", pointerRatioY).put("gyroScaleX", gyroScaleX).put("gyroScaleY", gyroScaleY)
                .put("gyroInvertX", gyroInvertX).put("gyroInvertY", gyroInvertY).put("gyroDeadzone", gyroDeadzone)
                .put("gyroSmoothing", gyroSmoothing).put("gyroAxis", gyroYawAxis ? "yaw" : "pitch"));
        o.put("widgets", Widget.toJsonArray(widgets));
        o.put("macros", new JSONArray());
        o.put("meta", meta);
        return o;
    }

    private static float num(JSONObject o, String k, float def, float lo, float hi) {
        double v = o == null ? def : o.optDouble(k, def);
        if (Double.isNaN(v) || Double.isInfinite(v)) v = def;
        return (float) Math.max(lo, Math.min(hi, v));
    }

    public static Profile fromJson(JSONObject o) throws JSONException {
        Profile p = new Profile();
        p.id = o.optString("id", p.id);
        p.gameId = o.optString("gameId", p.gameId);
        p.name = o.optString("name", p.name);
        p.revision = o.optInt("revision", 1);
        JSONObject grid = o.optJSONObject("grid");
        p.gridDp = num(grid, "unitDp", 10f, 2f, 200f);
        p.snap = grid == null || grid.optBoolean("snap", true);
        JSONObject ui = o.optJSONObject("ui");
        p.globalScale = num(ui, "globalScale", 1f, 0.3f, 3f);
        p.globalOpacity = num(ui, "globalOpacity", 1f, 0.1f, 1f);
        p.haptics = false;
        JSONObject rt = o.optJSONObject("runtime");
        p.pointerSensitivity = num(rt, "pointerSensitivity", 1f, 0.05f, 20f);
        p.pointerAcceleration = num(rt, "pointerAcceleration", 0.5f, 0f, 3f);
        p.pointerAccelEnabled = rt == null || rt.optBoolean("pointerAccelEnabled", true);
        p.gyroEnabled = rt != null && rt.optBoolean("gyroEnabled", false);
        p.gyroSensitivity = num(rt, "gyroSensitivity", 1f, -50f, 50f);
        p.releaseAllOnPause = rt == null || rt.optBoolean("releaseAllOnPause", true);
        p.pointerRatioY = num(rt, "pointerRatioY", 1f, 0.25f, 2f);
        p.gyroScaleX = num(rt, "gyroScaleX", 1f, 0f, 3f);
        p.gyroScaleY = num(rt, "gyroScaleY", 1f, 0f, 3f);
        p.gyroInvertX = rt != null && rt.optBoolean("gyroInvertX", false);
        p.gyroInvertY = rt != null && rt.optBoolean("gyroInvertY", false);
        p.gyroDeadzone = num(rt, "gyroDeadzone", 0.03f, 0f, 0.5f);
        p.gyroSmoothing = num(rt, "gyroSmoothing", 0.3f, 0f, 0.9f);
        p.gyroYawAxis = rt != null && "yaw".equals(rt.optString("gyroAxis", "pitch"));
        if (p.gyroSensitivity < 0) {   // older profiles flipped both axes with a negative sensitivity
            p.gyroSensitivity = -p.gyroSensitivity;
            p.gyroInvertX = !p.gyroInvertX;
            p.gyroInvertY = !p.gyroInvertY;
        }
        p.gyroSensitivity = Math.max(0.1f, p.gyroSensitivity);
        JSONArray arr = o.optJSONArray("widgets");
        if (arr != null) {
            for (int i = 0; i < arr.length(); i++) {
                JSONObject wo = arr.optJSONObject(i);
                if (wo != null) p.widgets.add(Widget.fromJson(wo));
            }
        }
        JSONObject meta = o.optJSONObject("meta");
        p.meta = meta != null ? meta : new JSONObject();
        p.sortByZ();
        return p;
    }

    /** Takes over everything from another profile, keeping this object (the overlay holds a reference to it). */
    public void assign(Profile o) {
        id = o.id; gameId = o.gameId; name = o.name; revision = o.revision;
        gridDp = o.gridDp; snap = o.snap; globalScale = o.globalScale; globalOpacity = o.globalOpacity;
        haptics = o.haptics; pointerSensitivity = o.pointerSensitivity; pointerAcceleration = o.pointerAcceleration;
        pointerAccelEnabled = o.pointerAccelEnabled;
        gyroEnabled = o.gyroEnabled; gyroSensitivity = o.gyroSensitivity; releaseAllOnPause = o.releaseAllOnPause;
        pointerRatioY = o.pointerRatioY; gyroScaleX = o.gyroScaleX; gyroScaleY = o.gyroScaleY;
        gyroInvertX = o.gyroInvertX; gyroInvertY = o.gyroInvertY; gyroDeadzone = o.gyroDeadzone;
        gyroSmoothing = o.gyroSmoothing; gyroYawAxis = o.gyroYawAxis;
        widgets.clear();
        widgets.addAll(o.widgets);
        meta = o.meta;
    }

    public Profile copy() {
        try { return fromJson(toJson()); } catch (JSONException e) { return new Profile(); }
    }

    // ---- files: atomic write, one .bak, recovery from it

    private static String readAll(File f) throws IOException {
        try (InputStream in = new FileInputStream(f)) {
            ByteArrayOutputStream out = new ByteArrayOutputStream();
            byte[] buf = new byte[8192];
            for (int n; (n = in.read(buf)) > 0; ) out.write(buf, 0, n);
            return new String(out.toByteArray(), StandardCharsets.UTF_8);
        }
    }

    /** Loads the file, falls back to its .bak, then to a blank profile. */
    public static Profile load(File f) {
        for (File c : new File[]{f, new File(f.getPath() + ".bak")}) {
            if (!c.isFile()) continue;
            try { return fromJson(new JSONObject(readAll(c))); } catch (IOException | JSONException ignored) { }
        }
        return new Profile();
    }

    /** As load(File), but a first run (no saved profile and no backup) starts from the assets' controls-default.json. */
    public static Profile load(File f, android.content.Context ctx) {
        if (!f.isFile() && !new File(f.getPath() + ".bak").isFile()) {
            try (InputStream in = ctx.getAssets().open("controls-default.json")) {
                java.io.ByteArrayOutputStream out = new java.io.ByteArrayOutputStream();
                byte[] buf = new byte[4096];
                for (int n; (n = in.read(buf)) > 0; ) out.write(buf, 0, n);
                return fromJson(new JSONObject(new String(out.toByteArray(), StandardCharsets.UTF_8)));
            } catch (IOException | JSONException ignored) { }
        }
        return load(f);
    }

    public void save(File f) throws IOException {
        String text;
        try { text = toJson().toString(2); } catch (JSONException e) { throw new IOException(e); }
        File dir = f.getParentFile();
        if (dir != null) dir.mkdirs();
        File tmp = new File(f.getPath() + ".tmp");
        try (FileOutputStream out = new FileOutputStream(tmp)) {
            out.write(text.getBytes(StandardCharsets.UTF_8));
            out.getFD().sync();
        }
        if (f.isFile()) {
            File bak = new File(f.getPath() + ".bak");
            bak.delete();
            f.renameTo(bak);
        }
        if (!tmp.renameTo(f)) throw new IOException("rename failed: " + f);
    }

    /** Imports a profile written by export; throws if it is not a profile. */
    public static Profile importFrom(File f) throws IOException, JSONException {
        return fromJson(new JSONObject(readAll(f)));
    }
}
