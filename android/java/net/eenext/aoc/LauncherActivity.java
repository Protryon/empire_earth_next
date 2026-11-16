package net.eenext.aoc;

import android.app.Activity;
import android.content.Intent;
import android.content.res.AssetManager;
import android.graphics.Color;
import android.os.Bundle;
import android.view.Gravity;
import android.view.WindowManager;
import android.widget.LinearLayout;
import android.widget.ProgressBar;
import android.widget.TextView;

import java.io.BufferedInputStream;
import java.io.BufferedReader;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.util.zip.ZipEntry;
import java.util.zip.ZipInputStream;

/**
 * The launcher: on the first run (or after an update that changed the data) it unpacks the game's data from the APK
 * (assets/data.zip) into the app's external files folder, which the native side uses as the data root, then starts
 * the game. Saved games and settings already there are never deleted.
 */
public class LauncherActivity extends Activity {
    private TextView status;
    private ProgressBar bar;
    private volatile boolean destroyed;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        LinearLayout col = new LinearLayout(this);
        col.setOrientation(LinearLayout.VERTICAL);
        col.setGravity(Gravity.CENTER);
        col.setBackgroundColor(Color.BLACK);
        int pad = (int) (32 * getResources().getDisplayMetrics().density);
        col.setPadding(pad, pad, pad, pad);
        status = new TextView(this);
        status.setTextColor(Color.WHITE);
        status.setGravity(Gravity.CENTER);
        status.setText("Empire Earth");
        col.addView(status);
        bar = new ProgressBar(this, null, android.R.attr.progressBarStyleHorizontal);
        bar.setMax(1000);
        col.addView(bar, new LinearLayout.LayoutParams(LinearLayout.LayoutParams.MATCH_PARENT,
                LinearLayout.LayoutParams.WRAP_CONTENT));
        setContentView(col);

        final File root = getExternalFilesDir(null);
        if (root == null) {
            status.setText("No external storage available for the game data.");
            return;
        }
        new Thread(new Runnable() {
            @Override public void run() { prepare(root); }
        }, "data-unpack").start();
    }

    @Override
    protected void onDestroy() {
        destroyed = true;
        super.onDestroy();
    }

    private String readAsset(String name) {
        try (InputStream in = getAssets().open(name);
             BufferedReader r = new BufferedReader(new InputStreamReader(in, "UTF-8"))) {
            String s = r.readLine();
            return s == null ? "" : s.trim();
        } catch (IOException e) {
            return null;
        }
    }

    private void ui(final String text, final int progress) {
        runOnUiThread(new Runnable() {
            @Override public void run() {
                if (text != null) status.setText(text);
                if (progress >= 0) bar.setProgress(progress);
            }
        });
    }

    private void prepare(File root) {
        String version = readAsset("data.version");
        File marker = new File(root, ".data-version");
        if (version != null) {
            String have = null;
            if (marker.isFile()) {
                try (BufferedReader r = new BufferedReader(new java.io.FileReader(marker))) { have = r.readLine(); }
                catch (IOException ignored) { }
            }
            if (!version.equals(have)) {
                long total = 0;
                String sizeText = readAsset("data.size");
                try { total = Long.parseLong(sizeText == null ? "0" : sizeText); } catch (NumberFormatException ignored) { }
                if (root.getUsableSpace() < total + (64L << 20)) {
                    ui("Not enough free space: the game data needs " + (total >> 20) + " MB.", 0);
                    return;
                }
                try {
                    unpack(root, total);
                    copyConfigIfMissing(root);
                    new File(root, "Data/Saved Games").mkdirs();
                    try (FileOutputStream out = new FileOutputStream(marker)) { out.write(version.getBytes("UTF-8")); }
                } catch (IOException e) {
                    ui("Unpacking the game data failed: " + e.getMessage(), -1);
                    return;
                }
            }
        }
        if (destroyed) return;
        runOnUiThread(new Runnable() {
            @Override public void run() {
                startActivity(new Intent(LauncherActivity.this, EEActivity.class));
                finish();
            }
        });
    }

    private void unpack(File root, long total) throws IOException {
        ui("Unpacking game data (first run only)...", 0);
        String rootPath = root.getCanonicalPath() + File.separator;
        long done = 0;
        byte[] buf = new byte[1 << 16];
        try (InputStream raw = getAssets().open("data.zip", AssetManager.ACCESS_STREAMING);
             ZipInputStream zin = new ZipInputStream(new BufferedInputStream(raw, 1 << 20))) {
            ZipEntry e;
            int lastShown = -1;
            while ((e = zin.getNextEntry()) != null) {
                File out = new File(root, e.getName());
                if (!out.getCanonicalPath().startsWith(rootPath)) continue;   // never write outside the data root
                if (e.isDirectory()) { out.mkdirs(); continue; }
                File dir = out.getParentFile();
                if (dir != null) dir.mkdirs();
                try (OutputStream os = new FileOutputStream(out)) {
                    for (int n; (n = zin.read(buf)) > 0; ) { os.write(buf, 0, n); done += n; }
                }
                int shown = total > 0 ? (int) Math.min(1000, done * 1000 / total) : 0;
                if (shown != lastShown) {
                    lastShown = shown;
                    ui("Unpacking game data (first run only)... " + shown / 10 + "%", shown);
                }
            }
        }
    }

    private void copyConfigIfMissing(File root) throws IOException {
        File cfg = new File(root, "config.yaml");
        if (cfg.exists()) return;   // the player's own settings win
        try (InputStream in = getAssets().open("config.yaml"); OutputStream out = new FileOutputStream(cfg)) {
            byte[] buf = new byte[4096];
            for (int n; (n = in.read(buf)) > 0; ) out.write(buf, 0, n);
        } catch (IOException e) {
            // no bundled config: the game writes its defaults
        }
    }
}
