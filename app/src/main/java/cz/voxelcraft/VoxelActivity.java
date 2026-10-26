package cz.voxelcraft;

import android.app.AlertDialog;
import android.app.NativeActivity;
import android.content.Context;
import android.content.Intent;
import android.provider.MediaStore;
import android.os.Environment;
import android.content.ContentValues;
import android.graphics.Typeface;
import android.media.AudioAttributes;
import android.media.MediaPlayer;
import android.media.SoundPool;
import android.net.Uri;
import android.net.wifi.WifiManager;
import android.os.Build;
import android.os.Bundle;
import android.os.VibrationEffect;
import android.os.Vibrator;
import android.os.VibratorManager;
import android.provider.OpenableColumns;
import android.database.Cursor;
import android.view.Gravity;
import android.text.InputType;
import android.view.WindowManager.LayoutParams;
import android.widget.EditText;
import android.window.OnBackInvokedDispatcher;
import android.view.WindowInsets;
import android.view.WindowInsetsController;
import android.view.WindowManager;
import android.widget.Toast;

import java.io.BufferedInputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.StandardCopyOption;
import java.util.Enumeration;
import java.util.HashMap;
import java.util.Map;
import java.util.Locale;
import java.util.zip.ZipEntry;
import java.util.zip.ZipFile;
import java.util.zip.ZipInputStream;

/**
 * Thin wrapper around NativeActivity: immersive fullscreen, text input dialogs,
 * texture pack and mod pack installation, vibration.
 */
public class VoxelActivity extends NativeActivity {
    static {
        // Loaded here as well so the JNI callback below resolves against our library.
        System.loadLibrary("voxelcraft");
    }

    private static final int REQUEST_PICK_PACK = 1;
    private static final int REQUEST_IMPORT_MODPACK = 2;
    private static final int REQUEST_IMPORT_SKIN = 3;
    /** Request id for "a skin PNG was copied into files/skins" (Game::PromptSkinImported). */
    private static final int RESULT_SKIN_IMPORTED = 7;
    private static final int REQUEST_IMPORT_STRUCTURE = 4;
    /** Request id for "a structure file was copied into files/structures" (Game::PromptStructureImported). */
    private static final int RESULT_STRUCTURE_IMPORTED = 9;
    private static final int REQUEST_IMPORT_WORLD = 10;
    private static final int REQUEST_EXPORT_WORLD = 11;
    /** Request id for "a .vcworld was copied into the cache" (Game::PromptWorldImported). */
    private static final int RESULT_WORLD_IMPORTED = 17;
    /** The .vcworld the game wrote, waiting for the player to choose where it goes. */
    private String pendingExport;

    private SoundPool soundPool;
    private final Map<String, Integer> sounds = new HashMap<>();
    private MediaPlayer music;
    private float musicVolume = 0;
    /** Request id the native side uses for "a mod pack was imported" (Game::PromptPackImported). */
    private static final int RESULT_PACK_IMPORTED = 5;
    private static final long MAX_ENTRY_BYTES = 16L << 20;  // guards against zip bombs
    private static final int MAX_FILES = 4000;

    /** Tells the native game to rebuild its texture atlas. */
    private static native void nativeTexturePackChanged();
    /** Delivers text typed into a dialog, as UTF-8 bytes. */
    private static native void nativeTextResult(int requestId, byte[] utf8);
    /** Mod packs on disk changed. */
    private static native void nativePacksChanged();

    private static void sendText(int requestId, String text) {
        nativeTextResult(requestId, text.getBytes(StandardCharsets.UTF_8));
    }
    /** System back gesture; the game decides what it means. */
    private static native void nativeBack();

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        getWindow().getAttributes().layoutInDisplayCutoutMode =
                WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_ALWAYS;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            // Predictive back (Android 13+) never sends KEYCODE_BACK to the native side.
            getOnBackInvokedDispatcher().registerOnBackInvokedCallback(
                    OnBackInvokedDispatcher.PRIORITY_DEFAULT, VoxelActivity::nativeBack);
        }
    }

    @Override
    @SuppressWarnings("deprecation")
    public void onBackPressed() {
        nativeBack();  // Android 11-12
    }

    /** Called from native code: asks for a line of text (world name, seed). */
    @SuppressWarnings("unused")
    public void promptText(int requestId, String title, String initial) {
        runOnUiThread(() -> {
            EditText input = new EditText(this);
            input.setInputType(InputType.TYPE_CLASS_TEXT);
            input.setSingleLine(true);
            input.setText(initial);
            input.setSelection(input.getText().length());
            AlertDialog dialog = new AlertDialog.Builder(this, android.R.style.Theme_DeviceDefault_Dialog_Alert)
                    .setTitle(title)
                    .setView(input)
                    .setPositiveButton(android.R.string.ok,
                            (d, which) -> sendText(requestId, input.getText().toString()))
                    .setNegativeButton(android.R.string.cancel, null)
                    .create();
            dialog.getWindow().setSoftInputMode(LayoutParams.SOFT_INPUT_STATE_VISIBLE);
            dialog.show();
            input.requestFocus();
        });
    }

    /** Texts of Java dialogs follow the language chosen in the game, not the system's. */
    private volatile android.content.Context langContext;

    private String str(int id, Object... args) {
        android.content.Context c = langContext != null ? langContext : this;
        return args.length == 0 ? c.getString(id) : c.getString(id, args);
    }

    /** Called from native code when the game language changes ("en", "cs", "es", "ru", "de"). */
    @SuppressWarnings("unused")
    // LAN games: some phones drop broadcast datagrams unless a multicast lock is held.
    private WifiManager.MulticastLock lanLock;

    public void setLanActive(boolean active) {
        runOnUiThread(() -> {
            try {
                if (active && lanLock == null) {
                    WifiManager wifi = (WifiManager) getApplicationContext().getSystemService(Context.WIFI_SERVICE);
                    if (wifi == null) return;
                    lanLock = wifi.createMulticastLock("VoxelCraftLan");
                    lanLock.setReferenceCounted(false);
                    lanLock.acquire();
                } else if (!active && lanLock != null) {
                    lanLock.release();
                    lanLock = null;
                }
            } catch (RuntimeException e) {
                lanLock = null;  // no Wi-Fi: the game still works over other networks
            }
        });
    }

    public void setLanguage(String code) {
        android.content.res.Configuration config = new android.content.res.Configuration(getResources().getConfiguration());
        config.setLocale(new Locale(code));
        langContext = createConfigurationContext(config);
    }

    /** Called from native code: a masked one-line input (developer password). */
    @SuppressWarnings("unused")
    public void promptPassword(int requestId, String title, String unused) {
        runOnUiThread(() -> {
            EditText input = new EditText(this);
            input.setInputType(InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_PASSWORD);
            input.setSingleLine(true);
            AlertDialog dialog = new AlertDialog.Builder(this, android.R.style.Theme_DeviceDefault_Dialog_Alert)
                    .setTitle(title)
                    .setView(input)
                    .setPositiveButton(android.R.string.ok,
                            (d, which) -> sendText(requestId, input.getText().toString()))
                    .setNegativeButton(android.R.string.cancel, null)
                    .create();
            dialog.getWindow().setSoftInputMode(LayoutParams.SOFT_INPUT_STATE_VISIBLE);
            dialog.show();
            input.requestFocus();
        });
    }

    /** Called from native code: a full-size multi-line editor for JSON files. */
    @SuppressWarnings("unused")
    public void editText(int requestId, String title, String text) {
        runOnUiThread(() -> {
            EditText input = new EditText(this);
            input.setInputType(InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_FLAG_MULTI_LINE
                    | InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS);
            input.setSingleLine(false);
            input.setHorizontallyScrolling(false);
            input.setGravity(Gravity.TOP | Gravity.START);
            input.setTypeface(Typeface.MONOSPACE);
            input.setTextSize(13);
            input.setMinLines(14);
            input.setText(text);
            AlertDialog dialog = new AlertDialog.Builder(this, android.R.style.Theme_DeviceDefault_Dialog_Alert)
                    .setTitle(title)
                    .setView(input)
                    .setPositiveButton(str(R.string.save), (d, which) -> sendText(requestId, input.getText().toString()))
                    .setNegativeButton(android.R.string.cancel, null)
                    .create();
            dialog.show();
            dialog.getWindow().setLayout(WindowManager.LayoutParams.MATCH_PARENT, WindowManager.LayoutParams.MATCH_PARENT);
        });
    }

    /** Called from native code: short haptic feedback. */
    @SuppressWarnings("unused")
    public void vibrate(int ms) {
        Vibrator vibrator;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            VibratorManager vm = (VibratorManager) getSystemService(Context.VIBRATOR_MANAGER_SERVICE);
            vibrator = vm != null ? vm.getDefaultVibrator() : null;
        } else {
            vibrator = (Vibrator) getSystemService(Context.VIBRATOR_SERVICE);
        }
        if (vibrator != null && vibrator.hasVibrator())
            vibrator.vibrate(VibrationEffect.createOneShot(ms, VibrationEffect.DEFAULT_AMPLITUDE));
    }

    // ---- audio: the native side synthesises files/sounds/*.wav, we just play them

    private synchronized void ensureSounds() {
        if (soundPool != null) return;
        soundPool = new SoundPool.Builder()
                .setMaxStreams(6)
                .setAudioAttributes(new AudioAttributes.Builder()
                        .setUsage(AudioAttributes.USAGE_GAME)
                        .setContentType(AudioAttributes.CONTENT_TYPE_SONIFICATION)
                        .build())
                .build();
        File dir = new File(getFilesDir(), "sounds");
        File[] files = dir.listFiles();
        if (files == null) return;
        for (File f : files) {
            String name = f.getName();
            if (name.endsWith(".wav") && !name.equals("music.wav") && !name.startsWith("biome_"))
                sounds.put(name.substring(0, name.length() - 4), soundPool.load(f.getPath(), 1));
        }
    }

    /** Called from native code (game thread). */
    @SuppressWarnings("unused")
    public void playSound(String name, float volume) {
        ensureSounds();
        Integer id = sounds.get(name);
        if (id != null) soundPool.play(id, volume, volume, 1, 0, 1f);
    }

    /** Request id for "the music track finished" (Game::PromptMusicEnded). */
    private static final int RESULT_MUSIC_ENDED = 12;
    private String musicSource = "";
    private MediaPlayer fadingOut;
    private final android.os.Handler fader = new android.os.Handler(android.os.Looper.getMainLooper());

    /** Called from native code: changes the music volume (0 pauses it). */
    @SuppressWarnings("unused")
    public void setMusicVolume(float volume) {
        runOnUiThread(() -> {
            musicVolume = volume;
            if (music == null) return;
            try {
                music.setVolume(volume, volume);
                if (volume > 0 && !music.isPlaying()) music.start();
                if (volume <= 0 && music.isPlaying()) music.pause();
            } catch (IllegalStateException e) {
                music = null;
            }
        });
    }

    /**
     * Called from native code: plays a track, crossfading from the current one.
     * source is "asset:music/x.mp3" (inside the APK) or a file path; an empty source stops
     * the music. When a track that does not loop ends, the game is told (RESULT_MUSIC_ENDED).
     */
    @SuppressWarnings("unused")
    public void playMusic(String source, boolean loop) {
        runOnUiThread(() -> {
            if (source.equals(musicSource) && music != null) return;
            musicSource = source;
            fadeOut(music);
            music = null;
            if (source.isEmpty()) return;
            try {
                MediaPlayer p = new MediaPlayer();
                if (source.startsWith("asset:")) {
                    try (android.content.res.AssetFileDescriptor fd = getAssets().openFd(source.substring(6))) {
                        p.setDataSource(fd.getFileDescriptor(), fd.getStartOffset(), fd.getLength());
                    }
                } else {
                    p.setDataSource(source);
                }
                p.setLooping(loop);
                p.prepare();
                p.setOnCompletionListener(mp -> {
                    if (mp == music) sendText(RESULT_MUSIC_ENDED, source);
                });
                music = p;
                fadeIn(p, 0);
            } catch (IOException | IllegalStateException e) {
                music = null;  // no music then, the game works without it
                sendText(RESULT_MUSIC_ENDED, source);
            }
        });
    }

    private void fadeIn(MediaPlayer p, int step) {
        if (p != music) return;
        try {
            float v = musicVolume * Math.min(1f, step / 20f);
            p.setVolume(v, v);
            if (step == 0 && musicVolume > 0 && !isFinishing()) p.start();
            if (step < 20) fader.postDelayed(() -> fadeIn(p, step + 1), 75);
        } catch (IllegalStateException ignored) {
            // released meanwhile
        }
    }

    private void fadeOut(MediaPlayer p) {
        if (p == null) return;
        if (fadingOut != null) fadingOut.release();
        fadingOut = p;
        fadeOutStep(p, 20);
    }

    private void fadeOutStep(MediaPlayer p, int step) {
        if (p != fadingOut) return;
        try {
            float v = musicVolume * step / 20f;
            p.setVolume(v, v);
            if (step > 0) {
                fader.postDelayed(() -> fadeOutStep(p, step - 1), 75);
                return;
            }
        } catch (IllegalStateException ignored) {
            // fall through and release
        }
        p.release();
        fadingOut = null;
    }

    @Override
    protected void onPause() {
        super.onPause();
        if (music != null && music.isPlaying()) music.pause();
    }

    @Override
    protected void onResume() {
        super.onResume();
        if (music != null && musicVolume > 0) music.start();
    }

    @Override
    protected void onDestroy() {
        if (music != null) music.release();
        if (fadingOut != null) fadingOut.release();
        if (soundPool != null) soundPool.release();
        super.onDestroy();
    }

    /** Called from native code: pick a skin PNG (64x64 or 64x32). */
    @SuppressWarnings("unused")
    public void importSkin() {
        runOnUiThread(() -> {
            Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
            intent.addCategory(Intent.CATEGORY_OPENABLE);
            intent.setType("image/png");
            startActivityForResult(intent, REQUEST_IMPORT_SKIN);
        });
    }

    /** Called from native code: pick a .mcstructure / .vcstructure file. */
    @SuppressWarnings("unused")
    public void importStructure() {
        runOnUiThread(() -> {
            Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
            intent.addCategory(Intent.CATEGORY_OPENABLE);
            intent.setType("*/*");  // neither format has a registered MIME type
            startActivityForResult(intent, REQUEST_IMPORT_STRUCTURE);
        });
    }

    /** Called from native code: pick a .vcworld to add to the world list. */
    @SuppressWarnings("unused")
    public void importWorld() {
        runOnUiThread(() -> {
            Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
            intent.addCategory(Intent.CATEGORY_OPENABLE);
            intent.setType("*/*");
            startActivityForResult(intent, REQUEST_IMPORT_WORLD);
        });
    }

    /** Called from native code: let the player save the exported world file somewhere. */
    @SuppressWarnings("unused")
    public void exportWorld(String path, String name) {
        // Straight into Downloads/VoxelCraft, where the Files app shows it; no permission needed.
        new Thread(() -> {
            try {
                ContentValues values = new ContentValues();
                values.put(MediaStore.Downloads.DISPLAY_NAME, name);
                values.put(MediaStore.Downloads.MIME_TYPE, "application/zip");
                values.put(MediaStore.Downloads.RELATIVE_PATH, Environment.DIRECTORY_DOWNLOADS + "/VoxelCraft");
                Uri uri = getContentResolver().insert(MediaStore.Downloads.EXTERNAL_CONTENT_URI, values);
                if (uri == null) throw new IOException("MediaStore");
                try (OutputStream out = getContentResolver().openOutputStream(uri)) {
                    if (out == null) throw new IOException("cannot write file");
                    Files.copy(new File(path).toPath(), out);
                }
                runOnUiThread(() -> Toast.makeText(this, str(R.string.world_exported, name), Toast.LENGTH_LONG).show());
            } catch (IOException | RuntimeException e) {
                exportWorldTo(path, name);  // let the player pick a place instead
            }
        }, "world-export").start();
    }

    private void exportWorldTo(String path, String name) {
        runOnUiThread(() -> {
            pendingExport = path;
            Intent intent = new Intent(Intent.ACTION_CREATE_DOCUMENT);
            intent.addCategory(Intent.CATEGORY_OPENABLE);
            intent.setType("application/zip");
            intent.putExtra(Intent.EXTRA_TITLE, name);
            startActivityForResult(intent, REQUEST_EXPORT_WORLD);
        });
    }

    /** Called from native code: pick a .zip / .mcpack mod pack to install. */
    @SuppressWarnings("unused")
    public void importPack() {
        runOnUiThread(() -> {
            Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
            intent.addCategory(Intent.CATEGORY_OPENABLE);
            intent.setType("*/*");
            startActivityForResult(intent, REQUEST_IMPORT_MODPACK);
        });
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (!hasFocus) return;
        WindowInsetsController controller = getWindow().getInsetsController();
        if (controller != null) {
            controller.hide(WindowInsets.Type.systemBars());
            controller.setSystemBarsBehavior(WindowInsetsController.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE);
        }
    }

    /** Called from native code (game thread) when the texture button is tapped. */
    @SuppressWarnings("unused")
    public void showTexturePackMenu() {
        runOnUiThread(() -> new AlertDialog.Builder(this, android.R.style.Theme_DeviceDefault_Dialog_Alert)
                .setTitle(str(R.string.pack_title))
                .setItems(new CharSequence[] {str(R.string.pack_load), str(R.string.pack_reset)},
                        (dialog, which) -> {
                            if (which == 0) pickPack();
                            else resetPack();
                        })
                .show());
    }

    private File packDir() {
        return new File(getFilesDir(), "texturepack");
    }

    private void pickPack() {
        Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
        intent.addCategory(Intent.CATEGORY_OPENABLE);
        intent.setType("*/*");  // .mcpack has no registered MIME type
        startActivityForResult(intent, REQUEST_PICK_PACK);
    }

    private void resetPack() {
        deleteRecursive(packDir());
        nativeTexturePackChanged();
        Toast.makeText(this, str(R.string.pack_reset_done), Toast.LENGTH_SHORT).show();
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (resultCode != RESULT_OK || data == null || data.getData() == null) return;
        Uri uri = data.getData();
        if (requestCode == REQUEST_IMPORT_WORLD) {
            try {
                File target = new File(getCacheDir(), "import.vcworld");
                try (InputStream in = getContentResolver().openInputStream(uri)) {
                    if (in == null) throw new IOException("cannot open file");
                    Files.copy(in, target.toPath(), StandardCopyOption.REPLACE_EXISTING);
                }
                sendText(RESULT_WORLD_IMPORTED, target.getAbsolutePath());  // the game unpacks and checks it
            } catch (IOException e) {
                Toast.makeText(this, str(R.string.pack_error, e.getMessage()), Toast.LENGTH_LONG).show();
            }
            return;
        }
        if (requestCode == REQUEST_EXPORT_WORLD) {
            if (pendingExport == null) return;
            try (OutputStream out = getContentResolver().openOutputStream(uri)) {
                if (out == null) throw new IOException("cannot write file");
                Files.copy(new File(pendingExport).toPath(), out);
            } catch (IOException e) {
                Toast.makeText(this, str(R.string.pack_error, e.getMessage()), Toast.LENGTH_LONG).show();
            }
            pendingExport = null;
            return;
        }
        if (requestCode == REQUEST_IMPORT_SKIN) {
            try {
                File dir = new File(getFilesDir(), "skins");
                if (!dir.isDirectory() && !dir.mkdirs()) throw new IOException("mkdir");
                String base = slug(displayName(uri).replaceAll("\\.png$", ""));
                File target = new File(dir, base + ".png");
                for (int n = 2; target.exists(); n++) target = new File(dir, base + "_" + n + ".png");
                try (InputStream in = getContentResolver().openInputStream(uri)) {
                    if (in == null) throw new IOException("cannot open file");
                    copy(in, target);
                }
                sendText(RESULT_SKIN_IMPORTED, target.getName());  // the game checks the size and switches to it
            } catch (IOException e) {
                Toast.makeText(this, str(R.string.pack_error, e.getMessage()), Toast.LENGTH_LONG).show();
            }
            return;
        }
        if (requestCode == REQUEST_IMPORT_STRUCTURE) {
            File target = null;
            try {
                File dir = new File(getFilesDir(), "structures");
                if (!dir.isDirectory() && !dir.mkdirs()) throw new IOException("mkdir");
                String name = displayName(uri).toLowerCase(Locale.ROOT);
                // The game picks the reader by extension; VoxelCraft files start with "VCST".
                byte[] head = new byte[4];
                int got;
                try (InputStream in = getContentResolver().openInputStream(uri)) {
                    if (in == null) throw new IOException("cannot open file");
                    got = in.read(head);
                }
                boolean vc = got == 4 && head[0] == 'V' && head[1] == 'C' && head[2] == 'S' && head[3] == 'T';
                String ext = vc ? ".vcstructure" : ".mcstructure";
                String base = slug(name.replaceAll("\\.(mc|vc)structure$", ""));
                target = new File(dir, base + ext);
                for (int n = 2; target.exists(); n++) target = new File(dir, base + "_" + n + ext);
                try (InputStream in = getContentResolver().openInputStream(uri)) {
                    if (in == null) throw new IOException("cannot open file");
                    copy(in, target);
                }
                sendText(RESULT_STRUCTURE_IMPORTED, target.getName());  // the game checks it and keeps it if it loads
            } catch (IOException e) {
                if (target != null) target.delete();
                Toast.makeText(this, str(R.string.pack_error, e.getMessage()), Toast.LENGTH_LONG).show();
            }
            return;
        }
        if (requestCode == REQUEST_IMPORT_MODPACK) {
            new Thread(() -> {
                String message;
                try {
                    String id = installModPack(uri);
                    sendText(RESULT_PACK_IMPORTED, id);
                    message = str(R.string.modpack_imported);
                } catch (IOException e) {
                    message = str(R.string.pack_error, e.getMessage());
                }
                final String text = message;
                runOnUiThread(() -> Toast.makeText(this, text, Toast.LENGTH_LONG).show());
            }, "modpack-install").start();
            return;
        }
        if (requestCode != REQUEST_PICK_PACK) return;
        new Thread(() -> {
            String message;
            try {
                int count = installPack(uri);
                message = count > 0 ? str(R.string.pack_loaded, count) : str(R.string.pack_empty);
                if (count > 0) nativeTexturePackChanged();
            } catch (IOException e) {
                message = str(R.string.pack_error, e.getMessage());
            }
            final String text = message;
            runOnUiThread(() -> Toast.makeText(this, text, Toast.LENGTH_LONG).show());
        }, "texture-pack-install").start();
    }

    /**
     * Unpacks a picked file into files/texturepack:
     * a bare PNG becomes terrain.png (classic atlas); a .zip/.mcpack contributes
     * terrain.png, blocks/* and items/* (Bedrock and Java pack layouts).
     * Returns how many texture files were installed.
     */
    private int installPack(Uri uri) throws IOException {
        File tmp = new File(getFilesDir(), "texturepack.tmp");
        deleteRecursive(tmp);
        if (!new File(tmp, "blocks").mkdirs() || !new File(tmp, "items").mkdirs()) throw new IOException("mkdir");

        int count = 0;
        try (InputStream raw = getContentResolver().openInputStream(uri)) {
            if (raw == null) throw new IOException("cannot open file");
            BufferedInputStream in = new BufferedInputStream(raw);
            in.mark(8);
            byte[] magic = new byte[4];
            int n = in.read(magic);
            in.reset();
            if (n == 4 && (magic[0] & 0xff) == 0x89 && magic[1] == 'P' && magic[2] == 'N' && magic[3] == 'G') {
                copy(in, new File(tmp, "terrain.png"));
                count = 1;
            } else {
                try (ZipInputStream zip = new ZipInputStream(in)) {
                    ZipEntry entry;
                    while ((entry = zip.getNextEntry()) != null && count < MAX_FILES) {
                        if (entry.isDirectory()) continue;
                        File target = targetFor(tmp, entry.getName());
                        if (target == null || target.exists()) continue;
                        copy(zip, target);
                        count++;
                    }
                }
            }
        }
        if (count == 0) {
            // Nothing usable: keep the pack that is installed now.
            deleteRecursive(tmp);
            return 0;
        }
        File dir = packDir();
        deleteRecursive(dir);
        if (!tmp.renameTo(dir)) throw new IOException("rename");
        return count;
    }

    /**
     * Installs a mod pack archive into files/packs/<id>. The pack root is the
     * folder holding manifest.json (or biomes/, dimensions/, recipes/).
     * Returns the new pack id.
     */
    private String installModPack(Uri uri) throws IOException {
        File archive = new File(getCacheDir(), "import.zip");
        try (InputStream in = getContentResolver().openInputStream(uri)) {
            if (in == null) throw new IOException("cannot open file");
            Files.copy(in, archive.toPath(), StandardCopyOption.REPLACE_EXISTING);
        }
        try (ZipFile zip = new ZipFile(archive)) {
            String prefix = null;
            boolean java = false, shaders = false;
            for (Enumeration<? extends ZipEntry> e = zip.entries(); e.hasMoreElements(); ) {
                String name = e.nextElement().getName().replace('\\', '/');
                // A Java Edition mod (.jar) or datapack / resource pack (.zip).
                if (name.startsWith("assets/") || name.startsWith("data/") || name.equals("fabric.mod.json")
                        || name.equals("quilt.mod.json") || name.equals("pack.mcmeta") || name.startsWith("META-INF/mods.toml")
                        || name.equals("META-INF/neoforge.mods.toml")) java = true;
                String candidate = null;
                if (name.endsWith("manifest.json")) candidate = name.substring(0, name.length() - "manifest.json".length());
                if (candidate == null && name.endsWith("content.json")) candidate = name.substring(0, name.length() - "content.json".length());
                // 1.5.5: shader packs, ours (shaders/shader.json) or old ones of other games (shaders/*.fsh).
                if (name.startsWith("shaders/") || name.contains("/shaders/")) shaders = true;
                for (String dir : new String[] {"biomes/", "dimensions/", "recipes/", "shaders/"}) {
                    int i = name.indexOf(dir);
                    if (candidate == null && i >= 0 && (i == 0 || name.charAt(i - 1) == '/')) candidate = name.substring(0, i);
                }
                if (candidate != null && (prefix == null || candidate.length() < prefix.length())) prefix = candidate;
            }
            String base = slug(displayName(uri).replaceAll("\\.(zip|mcpack|mcaddon|jar)$", ""));
            if (prefix == null && java && !shaders) return installJavaMod(zip, base);
            if (prefix == null) throw new IOException(str(R.string.modpack_invalid));

            File packs = new File(getFilesDir(), "packs");
            String id = base;
            for (int n = 2; new File(packs, id).exists(); n++) id = base + "_" + n;
            File root = new File(packs, id);
            int count = 0;
            for (Enumeration<? extends ZipEntry> e = zip.entries(); e.hasMoreElements() && count < MAX_FILES; ) {
                ZipEntry entry = e.nextElement();
                String name = entry.getName().replace('\\', '/');
                if (entry.isDirectory() || !name.startsWith(prefix)) continue;
                String rel = name.substring(prefix.length());
                String lower = rel.toLowerCase(Locale.ROOT);
                // Only data and images, and nothing that could escape the pack folder.
                if (rel.isEmpty() || rel.contains("..") || rel.startsWith("/")) continue;
                boolean shaderFile = lower.startsWith("shaders/") && (lower.endsWith(".fsh") || lower.endsWith(".vsh")
                        || lower.endsWith(".glsl") || lower.endsWith(".properties") || lower.endsWith(".frag") || lower.endsWith(".vert"));
                if (!(lower.endsWith(".json") || lower.endsWith(".png") || lower.endsWith(".tga") || lower.endsWith(".txt") || shaderFile)) continue;
                File target = new File(root, rel);
                File parent = target.getParentFile();
                if (parent != null && !parent.isDirectory() && !parent.mkdirs()) throw new IOException("mkdir");
                try (InputStream in = zip.getInputStream(entry)) {
                    copy(in, target);
                }
                count++;
            }
            if (count == 0) throw new IOException(str(R.string.modpack_invalid));
            File manifest = new File(root, "manifest.json");
            if (!manifest.exists()) {
                String json = "{\n  \"name\": \"" + base + "\",\n  \"description\": \"\",\n  \"version\": \"1.0.0\"\n}\n";
                Files.write(manifest.toPath(), json.getBytes(StandardCharsets.UTF_8));
            }
            return shaders ? "shaders:" + id : id;
        } finally {
            //noinspection ResultOfMethodCallIgnored
            archive.delete();
        }
    }

    /**
     * Unpacks what VoxelCraft can use from a Java Edition mod or datapack into
     * files/packs/<id>/java/: textures, language files, models, block states, recipes,
     * block loot tables and the mod's metadata. Compiled code is only counted (it never
     * runs); the game converts the rest (JavaImport.cpp). Returns "java:<id>".
     */
    private String installJavaMod(ZipFile zip, String base) throws IOException {
        File packs = new File(getFilesDir(), "packs");
        String id = base;
        for (int n = 2; new File(packs, id).exists(); n++) id = base + "_" + n;
        File root = new File(new File(packs, id), "java");
        int count = 0, classes = 0;
        for (Enumeration<? extends ZipEntry> e = zip.entries(); e.hasMoreElements() && count < MAX_FILES; ) {
            ZipEntry entry = e.nextElement();
            String name = entry.getName().replace('\\', '/');
            if (entry.isDirectory() || name.contains("..") || name.startsWith("/")) continue;
            if (name.endsWith(".class")) {
                classes++;
                continue;
            }
            boolean meta = name.equals("fabric.mod.json") || name.equals("quilt.mod.json") || name.equals("pack.mcmeta")
                    || name.equals("META-INF/mods.toml") || name.equals("META-INF/neoforge.mods.toml");
            boolean asset = name.startsWith("assets/") && (name.endsWith(".png") && name.contains("/textures/")
                    || name.endsWith(".json") && (name.contains("/lang/") || name.contains("/models/") || name.contains("/blockstates/")));
            boolean data = name.startsWith("data/") && name.endsWith(".json")
                    && (name.contains("/recipe") || name.contains("/loot_table"));
            if (!meta && !asset && !data) continue;
            File target = new File(root, name);
            File parent = target.getParentFile();
            if (parent != null && !parent.isDirectory() && !parent.mkdirs()) throw new IOException("mkdir");
            try (InputStream in = zip.getInputStream(entry)) {
                copy(in, target);
            }
            count++;
        }
        if (count == 0) throw new IOException(str(R.string.modpack_invalid));
        Files.write(new File(root, "classes.txt").toPath(), Integer.toString(classes).getBytes(StandardCharsets.UTF_8));
        return "java:" + id;
    }

    private String displayName(Uri uri) {
        try (Cursor c = getContentResolver().query(uri, new String[] {OpenableColumns.DISPLAY_NAME}, null, null, null)) {
            if (c != null && c.moveToFirst()) return c.getString(0);
        } catch (RuntimeException ignored) {
            // fall through to a generic name
        }
        return "balicek";
    }

    private static String slug(String name) {
        StringBuilder sb = new StringBuilder();
        for (char ch : name.toLowerCase(Locale.ROOT).toCharArray()) {
            if ((ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9')) sb.append(ch);
            else if (sb.length() > 0 && sb.charAt(sb.length() - 1) != '_') sb.append('_');
        }
        while (sb.length() > 0 && sb.charAt(sb.length() - 1) == '_') sb.setLength(sb.length() - 1);
        return sb.length() == 0 ? "balicek" : sb.toString();
    }

    /** Maps a path inside the archive to where the engine looks for it, or null to skip it. */
    private static File targetFor(File root, String entryName) {
        String path = "/" + entryName.replace('\\', '/').toLowerCase(Locale.ROOT);
        String name = path.substring(path.lastIndexOf('/') + 1);
        // Only the file name is kept, so nothing can escape the pack folder.
        if (name.isEmpty() || name.startsWith(".") || !(name.endsWith(".png") || name.endsWith(".tga"))) return null;
        if (name.equals("terrain.png") && !path.contains("/block")) return new File(root, name);
        if (path.contains("/textures/blocks/") || path.contains("/textures/block/")) return new File(root, "blocks/" + name);
        if (path.contains("/textures/items/") || path.contains("/textures/item/")) return new File(root, "items/" + name);
        return null;
    }

    private static void copy(InputStream in, File target) throws IOException {
        byte[] buf = new byte[16384];
        long total = 0;
        try (OutputStream out = new FileOutputStream(target)) {
            int n;
            while ((n = in.read(buf)) > 0) {
                total += n;
                if (total > MAX_ENTRY_BYTES) throw new IOException("file too large: " + target.getName());
                out.write(buf, 0, n);
            }
        }
    }

    private static void deleteRecursive(File f) {
        File[] children = f.listFiles();
        if (children != null) for (File c : children) deleteRecursive(c);
        //noinspection ResultOfMethodCallIgnored
        f.delete();
    }
}
