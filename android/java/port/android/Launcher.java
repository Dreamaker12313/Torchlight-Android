// The Android build's first screen: before the engine
// starts, the folder that holds the player's own copy of the game.
//
// The game's files are never in the APK. The player keeps `pak.zip` and
// `music/` in a folder of their choosing; this asks for it once with the
// system's folder picker, remembers it, checks it on every start, and says
// plainly what is missing. The engine (GameActivity, libtorchlight.so) then
// reads it by path, which is why it needs "All files access": a folder chosen
// through the picker is otherwise a content URI, and the engine opens files.
//
// It also unpacks what the APK does carry, which is part of the program and
// not of the game, into the app's own files: OGRE's shader library and the
// port's own interface pack (`ui_pack/`, what a PC pak is drawn with when no
// `console_ui/` is beside it).
//
// Last, before the game starts, its one setting the engine cannot change once
// running: the resolution it renders at, as a share of the screen's (the
// system scales the smaller frame up to the screen, its proportions kept).
// The engine reads it from `render_scale.txt`, a percentage.
package port.android;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.PackageManager;
import android.content.res.AssetManager;
import android.graphics.Rect;
import android.net.Uri;
import android.os.Environment;
import android.provider.DocumentsContract;
import android.provider.Settings;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.Locale;

public final class Launcher extends Activity {
    private static final int PICK_FOLDER = 1;
    private static final String PREFS = "launcher";
    private static final String KEY_FOLDER = "game_data";
    private static final String KEY_NO_MUSIC = "play_without_music";
    private static final String KEY_SCALE = "render_scale";
    /** The resolutions offered, in percent of the screen's width and height. */
    private static final int[] SCALES = {100, 90, 80, 75, 70, 60, 50};

    /**
     * The game is running in this process (in the background, or this activity was started over
     * it): it is brought back rather than started again. The process ends with the game.
     */
    private static boolean gameRunning;

    /** A dialog or a system screen is up; onResume() waits for it. */
    private boolean waiting;
    /** The game has been started; nothing more to do here. */
    private boolean started;

    @Override
    protected void onResume() {
        super.onResume();
        if (gameRunning) {
            started = true;
            startActivity(new Intent(this, GameActivity.class)
                    .addFlags(Intent.FLAG_ACTIVITY_REORDER_TO_FRONT));
            finish();
            return;
        }
        if (!waiting) next();
    }

    private SharedPreferences prefs() {
        return getSharedPreferences(PREFS, MODE_PRIVATE);
    }

    /** One step further towards the game, or the question that stands in the way. */
    private void next() {
        if (started) return;
        if (!Environment.isExternalStorageManager()) {
            askForStorage();
            return;
        }
        String folder = prefs().getString(KEY_FOLDER, null);
        if (folder == null) {
            askForFolder("Choose the folder with your game files",
                    "This program does not include the game. Choose the folder that holds your "
                            + "own copy of it:\n\n  pak.zip   (the Xbox 360 release's)\n  music/\n\n"
                            + "Copy them from your game to a folder on this phone first, "
                            + "for example Download/Torchlight.");
            return;
        }
        File dir = new File(folder);
        if (!new File(dir, "pak.zip").isFile() && !new File(dir, "Pak.zip").isFile()) {
            askForFolder("pak.zip is missing",
                    "There is no pak.zip in\n\n  " + folder + "\n\nChoose the folder that holds "
                            + "pak.zip (the Xbox 360 release's) and music/.");
            return;
        }
        if (!new File(dir, "music").isDirectory() && !folder.equals(prefs().getString(KEY_NO_MUSIC, ""))) {
            askAboutMusic(folder);
            return;
        }
        askForResolution(folder);
    }

    /** The screen's size in landscape, which is how the game is played. */
    private int[] screenSize() {
        Rect bounds = getWindowManager().getMaximumWindowMetrics().getBounds();
        int w = bounds.width();
        int h = bounds.height();
        return new int[] {Math.max(w, h), Math.min(w, h)};
    }

    /**
     * What the engine renders at for a percentage: the screen's size times 1/factor, the factor
     * written and read back as the engine gives it to OGRE, so the figure shown is the frame's.
     */
    private static int scaled(int size, int percent) {
        if (percent >= 100) return size;
        float factor = (float) Double.parseDouble(String.format(Locale.ROOT, "%.6f", 100.0 / percent));
        return (int) (size * (1.0f / factor));
    }

    private void askForResolution(String folder) {
        waiting = true;
        int[] screen = screenSize();
        int chosen = prefs().getInt(KEY_SCALE, 100);
        String[] items = new String[SCALES.length];
        int checked = 0;
        for (int i = 0; i < SCALES.length; ++i) {
            int p = SCALES[i];
            items[i] = p + "%   " + scaled(screen[0], p) + " x " + scaled(screen[1], p)
                    + (p == 100 ? "   (the screen's own)" : "");
            if (p == chosen) checked = i;
        }
        final int[] pick = {SCALES[checked]};
        new AlertDialog.Builder(this)
                .setTitle("Resolution")
                .setSingleChoiceItems(items, checked, (d, which) -> pick[0] = SCALES[which])
                .setCancelable(false)
                .setPositiveButton("Play", (d, w) -> {
                    prefs().edit().putInt(KEY_SCALE, pick[0]).apply();
                    waiting = false;
                    start(folder);
                })
                .setNeutralButton("Game folder", (d, w) ->
                        startActivityForResult(new Intent(Intent.ACTION_OPEN_DOCUMENT_TREE), PICK_FOLDER))
                .setNegativeButton("Quit", (d, w) -> finish())
                .show();
    }

    private void askForStorage() {
        waiting = true;
        new AlertDialog.Builder(this)
                .setTitle("Allow access to your game files")
                .setMessage("The game reads pak.zip and music/ from a folder you choose. Android "
                        + "lets a program read a folder that way only with \"All files access\".\n\n"
                        + "On the next screen, switch it on for this program, then come back.")
                .setCancelable(false)
                .setPositiveButton("Open settings", (d, w) -> {
                    Intent intent = new Intent(Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION,
                            Uri.parse("package:" + getPackageName()));
                    waiting = false;  // checked again when the settings screen returns
                    startActivity(intent);
                })
                .setNegativeButton("Quit", (d, w) -> finish())
                .show();
    }

    private void askForFolder(String title, String message) {
        waiting = true;
        new AlertDialog.Builder(this)
                .setTitle(title)
                .setMessage(message)
                .setCancelable(false)
                .setPositiveButton("Choose folder", (d, w) ->
                        startActivityForResult(new Intent(Intent.ACTION_OPEN_DOCUMENT_TREE), PICK_FOLDER))
                .setNegativeButton("Quit", (d, w) -> finish())
                .show();
    }

    private void askAboutMusic(String folder) {
        waiting = true;
        new AlertDialog.Builder(this)
                .setTitle("music/ is missing")
                .setMessage("There is no music/ folder in\n\n  " + folder + "\n\nThe level themes are "
                        + "not inside pak.zip. Copy the music/ folder from your game beside it, or "
                        + "play without music.")
                .setCancelable(false)
                .setPositiveButton("Play without music", (d, w) -> {
                    prefs().edit().putString(KEY_NO_MUSIC, folder).apply();
                    waiting = false;
                    next();
                })
                .setNeutralButton("Choose folder", (d, w) ->
                        startActivityForResult(new Intent(Intent.ACTION_OPEN_DOCUMENT_TREE), PICK_FOLDER))
                .setNegativeButton("Quit", (d, w) -> finish())
                .show();
    }

    @Override
    protected void onActivityResult(int request, int result, Intent data) {
        super.onActivityResult(request, result, data);
        if (request != PICK_FOLDER) return;
        // onResume(), which follows, takes the next step with the folder chosen.
        waiting = false;
        if (result == RESULT_OK && data != null && data.getData() != null) {
            String path = pathOf(data.getData());
            if (path != null) prefs().edit().putString(KEY_FOLDER, path).apply();
        }
    }

    /**
     * A folder the picker returned, as a path the engine can open: `primary:Download/x` is the
     * phone's own storage, `XXXX-XXXX:x` an SD card's. Null for anything else.
     */
    private static String pathOf(Uri tree) {
        String id = DocumentsContract.getTreeDocumentId(tree);
        int colon = id.indexOf(':');
        if (colon < 0) return null;
        String volume = id.substring(0, colon);
        String rest = id.substring(colon + 1);
        String root = volume.equals("primary")
                ? Environment.getExternalStorageDirectory().getPath()
                : "/storage/" + volume;
        return rest.isEmpty() ? root : root + "/" + rest;
    }

    private void start(String folder) {
        try {
            unpackShaderLibrary();
        } catch (IOException e) {
            waiting = true;
            new AlertDialog.Builder(this)
                    .setTitle("The program could not unpack itself")
                    .setMessage("Its shader library could not be written to the app's own storage ("
                            + e.getMessage() + "). Free some space and try again.")
                    .setPositiveButton("Quit", (d, w) -> finish())
                    .show();
            return;
        }
        File app = getExternalFilesDir(null);
        try (OutputStream out = new FileOutputStream(new File(app, "game_data.txt"))) {
            out.write((folder + "\n").getBytes("UTF-8"));
        } catch (IOException e) {
            // The engine then reads the app's own folder, and says what it does not find there.
        }
        try (OutputStream out = new FileOutputStream(new File(app, "render_scale.txt"))) {
            out.write((prefs().getInt(KEY_SCALE, 100) + "\n").getBytes("UTF-8"));
        } catch (IOException e) {
            // The engine then renders at the screen's own resolution.
        }
        started = true;
        gameRunning = true;
        startActivity(new Intent(this, GameActivity.class));
        finish();
    }

    /**
     * OGRE's shader library (`RTShaderLib/`, `Main/` inside it) and the interface pack
     * (`ui_pack/`), from the APK's assets into the app's files, once for each install or update
     * of the program.
     */
    private void unpackShaderLibrary() throws IOException {
        long installed;
        try {
            installed = getPackageManager().getPackageInfo(getPackageName(), 0).lastUpdateTime;
        } catch (PackageManager.NameNotFoundException e) {
            installed = 0;
        }
        File stamp = new File(getFilesDir(), "RTShaderLib.stamp");
        String want = Long.toString(installed);
        if (stamp.isFile() && want.equals(read(stamp))) return;
        copyAssets(getAssets(), "RTShaderLib", new File(getFilesDir(), "RTShaderLib"));
        // Replaced whole: a file of an older pack (or one spelt in another case) would stay.
        File pack = new File(getFilesDir(), "ui_pack");
        delete(pack);
        copyAssets(getAssets(), "ui_pack", pack);
        try (OutputStream out = new FileOutputStream(stamp)) {
            out.write(want.getBytes("UTF-8"));
        }
    }

    private static void delete(File file) {
        File[] children = file.listFiles();
        if (children != null) {
            for (File child : children) delete(child);
        }
        file.delete();
    }

    private static void copyAssets(AssetManager assets, String from, File to) throws IOException {
        String[] names = assets.list(from);
        if (names == null || names.length == 0) {
            try (InputStream in = assets.open(from); OutputStream out = new FileOutputStream(to)) {
                byte[] buffer = new byte[65536];
                for (int n; (n = in.read(buffer)) > 0; ) out.write(buffer, 0, n);
            }
            return;
        }
        if (!to.isDirectory() && !to.mkdirs()) throw new IOException("cannot make " + to);
        for (String name : names) copyAssets(assets, from + "/" + name, new File(to, name));
    }

    private static String read(File file) throws IOException {
        try (InputStream in = new java.io.FileInputStream(file)) {
            java.io.ByteArrayOutputStream bytes = new java.io.ByteArrayOutputStream();
            byte[] buffer = new byte[256];
            for (int n; (n = in.read(buffer)) > 0; ) bytes.write(buffer, 0, n);
            return new String(bytes.toByteArray(), "UTF-8").trim();
        }
    }
}
