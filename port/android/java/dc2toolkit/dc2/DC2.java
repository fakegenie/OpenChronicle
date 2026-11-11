package dc2toolkit.dc2;

import android.app.AlertDialog;
import android.content.ContentResolver;
import android.content.Intent;
import android.database.Cursor;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.ParcelFileDescriptor;
import android.provider.DocumentsContract;
import android.provider.OpenableColumns;
import android.util.Log;
import android.view.View;
import android.view.Window;
import android.view.WindowInsets;
import android.view.WindowInsetsController;
import android.view.WindowManager;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.Future;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicBoolean;
import org.libsdl.app.SDLActivity;

public class DC2 extends SDLActivity {
    private static final int REQUEST_SAVE_FOLDER = 0xDC20;
    private static final String TAG = "DC2";

    private static final class ChildDoc {
        final String docId;
        final String name;
        final String mimeType;
        final long modified;
        final long size;
        final Uri uri;

        ChildDoc(Uri treeUri, String docId, String name, String mimeType, long modified, long size) {
            this.docId = docId;
            this.name = name;
            this.mimeType = mimeType;
            this.modified = modified;
            this.size = size;
            this.uri = DocumentsContract.buildDocumentUriUsingTree(treeUri, docId);
        }

        boolean isDir() {
            return DocumentsContract.Document.MIME_TYPE_DIR.equals(mimeType);
        }
    }

    private static final class Stamp {
        final long modified;
        final long size;

        Stamp(long modified, long size) {
            this.modified = modified;
            this.size = size;
        }
    }

    private final Object saveLock = new Object();
    private final Map<String, Stamp> syncedStamps = new HashMap<>();
    private final AtomicBoolean syncDirty = new AtomicBoolean(false);
    private final AtomicBoolean syncRunning = new AtomicBoolean(false);
    private final ExecutorService syncExecutor = Executors.newSingleThreadExecutor(runnable -> {
        Thread thread = new Thread(runnable, "dc2-save-sync");
        thread.setDaemon(true);
        return thread;
    });
    private Uri activeTreeUri;
    private String activeSavesDocId;
    private File activeMirrorDir;

    private static native void nativePickedFolder(String uri);

    private void applyImmersiveFullscreen() {
        Window window = getWindow();
        if (window == null) {
            return;
        }
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            window.setDecorFitsSystemWindows(false);
            WindowManager.LayoutParams attrs = window.getAttributes();
            attrs.layoutInDisplayCutoutMode =
                    WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_ALWAYS;
            window.setAttributes(attrs);
            WindowInsetsController controller = window.getInsetsController();
            if (controller != null) {
                controller.hide(WindowInsets.Type.systemBars());
                controller.setSystemBarsBehavior(
                        WindowInsetsController.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE);
            }
        } else {
            window.getDecorView().setSystemUiVisibility(
                    View.SYSTEM_UI_FLAG_FULLSCREEN
                            | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                            | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
                            | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                            | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                            | View.SYSTEM_UI_FLAG_LAYOUT_STABLE);
        }
    }

    @Override
    protected String[] getLibraries() {
        return new String[] {"SDL3", "main"};
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        applyImmersiveFullscreen();
    }

    @Override
    protected void onResume() {
        super.onResume();
        applyImmersiveFullscreen();
    }

    @Override
    protected void onPause() {
        flushSavesOnPause();
        super.onPause();
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (hasFocus) {
            applyImmersiveFullscreen();
        }
    }

    private void launchSaveFolderPicker() {
        Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT_TREE);
        intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION
                | Intent.FLAG_GRANT_WRITE_URI_PERMISSION
                | Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION
                | Intent.FLAG_GRANT_PREFIX_URI_PERMISSION);
        try {
            startActivityForResult(intent, REQUEST_SAVE_FOLDER);
        } catch (RuntimeException failed) {
            nativePickedFolder("");
        }
    }

    public void pickSaveFolder(boolean hasCustom) {
        runOnUiThread(() -> {
            if (!hasCustom) {
                launchSaveFolderPicker();
                return;
            }
            new AlertDialog.Builder(this, android.R.style.Theme_DeviceDefault_Dialog_Alert)
                    .setTitle("Saves")
                    .setItems(new CharSequence[] {"Choose folder", "On this device"}, (dialog, which) -> {
                        if (which == 0) launchSaveFolderPicker();
                        else nativePickedFolder("-");
                    })
                    .setOnCancelListener(dialog -> nativePickedFolder(""))
                    .show();
        });
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        Uri picked = data == null ? null : data.getData();
        if (requestCode == REQUEST_SAVE_FOLDER) {
            if (resultCode == RESULT_OK && picked != null) {
                try {
                    getContentResolver().takePersistableUriPermission(
                            picked,
                            Intent.FLAG_GRANT_READ_URI_PERMISSION | Intent.FLAG_GRANT_WRITE_URI_PERMISSION);
                } catch (SecurityException refused) {
                }
                nativePickedFolder(picked.toString());
            } else {
                nativePickedFolder("");
            }
            return;
        }
        if (picked != null) {
            try {
                getContentResolver().takePersistableUriPermission(picked, Intent.FLAG_GRANT_READ_URI_PERMISSION);
            } catch (SecurityException refused) {
            }
        }
        super.onActivityResult(requestCode, resultCode, data);
    }

    public String displayName(String uri) {
        Uri item = Uri.parse(uri);
        if (DocumentsContract.isTreeUri(item) && !DocumentsContract.isDocumentUri(this, item)) {
            item = DocumentsContract.buildDocumentUriUsingTree(item, DocumentsContract.getTreeDocumentId(item));
        }
        try (Cursor cursor = getContentResolver().query(item, new String[] {OpenableColumns.DISPLAY_NAME}, null, null, null)) {
            if (cursor != null && cursor.moveToFirst() && !cursor.isNull(0)) {
                String name = cursor.getString(0);
                if (name != null && !name.isEmpty()) return name;
            }
        } catch (RuntimeException refused) {
        }
        String last = item.getLastPathSegment();
        if (last == null) return "";
        int cut = Math.max(last.lastIndexOf('/'), last.lastIndexOf(':'));
        return cut >= 0 && cut + 1 < last.length() ? last.substring(cut + 1) : last;
    }

    private static boolean validEntryName(String name) {
        return name != null
                && !name.isEmpty()
                && !name.equals(".")
                && !name.equals("..")
                && name.indexOf('/') < 0
                && name.indexOf('\\') < 0
                && !name.endsWith(".tmp");
    }

    private static boolean hasLocalSaveFiles(File root) {
        if (!root.isDirectory()) return false;
        if (new File(root, "quick.dc2state").isFile()) return true;
        for (int port = 0; port < 2; port++) {
            File card = new File(root, "card" + port);
            String[] entries = card.list();
            if (entries != null && entries.length > 0) return true;
        }
        return false;
    }

    private static void copyStream(InputStream in, OutputStream out) throws IOException {
        byte[] chunk = new byte[65536];
        int got;
        while ((got = in.read(chunk)) > 0) {
            out.write(chunk, 0, got);
        }
    }

    private static void copyLocalDir(File source, File target) throws IOException {
        File[] children = source.listFiles();
        if (children == null) return;
        target.mkdirs();
        for (File child : children) {
            if (!validEntryName(child.getName())) continue;
            File dest = new File(target, child.getName());
            if (child.isDirectory()) {
                copyLocalDir(child, dest);
            } else if (child.isFile()) {
                try (InputStream in = new FileInputStream(child);
                        OutputStream out = new FileOutputStream(dest)) {
                    copyStream(in, out);
                }
                long modified = child.lastModified();
                if (modified > 0) dest.setLastModified(modified);
            }
        }
    }

    private static void deleteRecursively(File target) {
        if (target.isDirectory()) {
            File[] children = target.listFiles();
            if (children != null) {
                for (File child : children) deleteRecursively(child);
            }
        }
        target.delete();
    }

    private static void deleteContents(File folder) {
        File[] children = folder.listFiles();
        if (children == null) return;
        for (File child : children) deleteRecursively(child);
    }

    public void migrateLegacySaves(String targetSavesPath) {
        try {
            File legacy = new File(getFilesDir(), "saves");
            File target = new File(targetSavesPath);
            if (legacy.getCanonicalPath().equals(target.getCanonicalPath())) return;
            if (!hasLocalSaveFiles(target) && hasLocalSaveFiles(legacy)) {
                copyLocalDir(legacy, target);
            }
        } catch (IOException ignored) {
        }
    }

    private Map<String, ChildDoc> listChildren(Uri treeUri, String parentDocId) throws IOException {
        Map<String, ChildDoc> found = new HashMap<>();
        Uri childrenUri = DocumentsContract.buildChildDocumentsUriUsingTree(treeUri, parentDocId);
        String[] columns = {
            DocumentsContract.Document.COLUMN_DOCUMENT_ID,
            DocumentsContract.Document.COLUMN_DISPLAY_NAME,
            DocumentsContract.Document.COLUMN_MIME_TYPE,
            DocumentsContract.Document.COLUMN_LAST_MODIFIED,
            DocumentsContract.Document.COLUMN_SIZE,
        };
        try (Cursor cursor = getContentResolver().query(childrenUri, columns, null, null, null)) {
            if (cursor == null) throw new IOException("cannot query " + childrenUri);
            while (cursor.moveToNext()) {
                String docId = cursor.getString(0);
                String name = cursor.getString(1);
                String mime = cursor.getString(2);
                long modified = cursor.isNull(3) ? 0 : cursor.getLong(3);
                long size = cursor.isNull(4) ? -1 : cursor.getLong(4);
                if (docId != null && validEntryName(name)) {
                    found.put(name, new ChildDoc(treeUri, docId, name, mime, modified, size));
                }
            }
        } catch (RuntimeException failed) {
            throw new IOException(failed);
        }
        return found;
    }

    private String ensureSubdir(Uri treeUri, String parentDocId, Map<String, ChildDoc> children, String name) throws IOException {
        ChildDoc existing = children.get(name);
        if (existing != null && existing.isDir()) return existing.docId;
        Uri parentUri = DocumentsContract.buildDocumentUriUsingTree(treeUri, parentDocId);
        try {
            Uri created = DocumentsContract.createDocument(
                    getContentResolver(), parentUri, DocumentsContract.Document.MIME_TYPE_DIR, name);
            if (created == null) throw new IOException("cannot create folder " + name);
            return DocumentsContract.getDocumentId(created);
        } catch (RuntimeException failed) {
            throw new IOException(failed);
        }
    }

    private boolean hasSafSaveFiles(Uri treeUri, String savesDocId) throws IOException {
        Map<String, ChildDoc> children = listChildren(treeUri, savesDocId);
        for (ChildDoc child : children.values()) {
            if (!child.isDir()) return true;
            if (("card0".equals(child.name) || "card1".equals(child.name))
                    && !listChildren(treeUri, child.docId).isEmpty()) {
                return true;
            }
        }
        return false;
    }

    private void pullDir(Uri treeUri, String docId, File localDir, boolean reuseUnchanged) throws IOException {
        localDir.mkdirs();
        Map<String, ChildDoc> remote = listChildren(treeUri, docId);
        Set<String> seen = new HashSet<>();
        for (ChildDoc child : remote.values()) {
            seen.add(child.name);
            File target = new File(localDir, child.name);
            if (child.isDir()) {
                if (target.exists() && !target.isDirectory()) target.delete();
                pullDir(treeUri, child.docId, target, reuseUnchanged);
            } else {
                if (target.isDirectory()) deleteRecursively(target);
                boolean unchanged = reuseUnchanged
                        && target.isFile()
                        && child.size >= 0
                        && target.length() == child.size
                        && child.modified > 0
                        && Math.abs(target.lastModified() - child.modified) < 2000;
                if (!unchanged) {
                    File temp = new File(localDir, child.name + ".tmp");
                    try (InputStream in = getContentResolver().openInputStream(child.uri);
                            OutputStream out = new FileOutputStream(temp)) {
                        if (in == null) throw new IOException("cannot read " + child.uri);
                        copyStream(in, out);
                    }
                    if (target.exists() && !temp.renameTo(target)) {
                        target.delete();
                        if (!temp.renameTo(target)) throw new IOException("cannot replace " + target);
                    } else if (!target.exists() && !temp.renameTo(target)) {
                        throw new IOException("cannot move " + temp);
                    }
                    if (child.modified > 0) target.setLastModified(child.modified);
                }
                syncedStamps.put(target.getAbsolutePath(), new Stamp(target.lastModified(), target.length()));
            }
        }
        File[] existing = localDir.listFiles();
        if (existing != null) {
            for (File extra : existing) {
                if (!seen.contains(extra.getName())) deleteRecursively(extra);
            }
        }
    }

    private boolean writeSafUri(Uri uri, File source) {
        ContentResolver cr = getContentResolver();
        try (ParcelFileDescriptor pfd = cr.openFileDescriptor(uri, "rwt")) {
            if (pfd != null) {
                try (InputStream in = new FileInputStream(source);
                        FileOutputStream out = new FileOutputStream(pfd.getFileDescriptor())) {
                    copyStream(in, out);
                    out.getFD().sync();
                    return true;
                }
            }
        } catch (IOException | RuntimeException ignored) {
        }
        try (InputStream in = new FileInputStream(source);
                OutputStream out = cr.openOutputStream(uri, "wt")) {
            if (out != null) {
                copyStream(in, out);
                out.flush();
                return true;
            }
        } catch (IOException | RuntimeException ignored) {
        }
        return false;
    }

    private void pushDir(Uri treeUri, String parentDocId, File localDir) throws IOException {
        ContentResolver cr = getContentResolver();
        Uri parentUri = DocumentsContract.buildDocumentUriUsingTree(treeUri, parentDocId);
        Map<String, ChildDoc> remote = listChildren(treeUri, parentDocId);
        File[] localChildren = localDir.listFiles();
        if (localChildren != null) {
            for (File local : localChildren) {
                String name = local.getName();
                if (!validEntryName(name)) continue;
                ChildDoc existing = remote.remove(name);
                if (local.isDirectory()) {
                    if (existing != null && !existing.isDir()) {
                        try {
                            DocumentsContract.deleteDocument(cr, existing.uri);
                        } catch (RuntimeException ignored) {
                        }
                        existing = null;
                    }
                    String subDocId;
                    if (existing != null) {
                        subDocId = existing.docId;
                    } else {
                        Uri created = DocumentsContract.createDocument(
                                cr, parentUri, DocumentsContract.Document.MIME_TYPE_DIR, name);
                        if (created == null) throw new IOException("cannot create folder " + name);
                        subDocId = DocumentsContract.getDocumentId(created);
                    }
                    pushDir(treeUri, subDocId, local);
                } else if (local.isFile()) {
                    long length = local.length();
                    long modified = local.lastModified();
                    String key = local.getAbsolutePath();
                    Stamp previous = syncedStamps.get(key);
                    if (existing != null
                            && !existing.isDir()
                            && previous != null
                            && previous.modified == modified
                            && previous.size == length
                            && (existing.size < 0 || existing.size == length)) {
                        continue;
                    }
                    if (existing != null && existing.isDir()) {
                        try {
                            DocumentsContract.deleteDocument(cr, existing.uri);
                        } catch (RuntimeException ignored) {
                        }
                        existing = null;
                    }
                    boolean wrote = false;
                    if (existing != null) {
                        wrote = writeSafUri(existing.uri, local);
                        if (!wrote) {
                            try {
                                DocumentsContract.deleteDocument(cr, existing.uri);
                            } catch (RuntimeException ignored) {
                            }
                        }
                    }
                    if (!wrote) {
                        Uri created = DocumentsContract.createDocument(
                                cr, parentUri, "application/octet-stream", name);
                        if (created == null || !writeSafUri(created, local)) {
                            throw new IOException("cannot write " + name);
                        }
                    }
                    syncedStamps.put(key, new Stamp(modified, length));
                }
            }
        }
        for (ChildDoc removed : remote.values()) {
            try {
                DocumentsContract.deleteDocument(cr, removed.uri);
            } catch (IOException | RuntimeException ignored) {
            }
            String prefix = new File(localDir, removed.name).getAbsolutePath();
            syncedStamps.entrySet().removeIf(e -> e.getKey().equals(prefix) || e.getKey().startsWith(prefix + "/"));
        }
    }

    public String prepareSaves(String treeUriString, String fallbackSavesPath) {
        synchronized (saveLock) {
            try {
                Uri treeUri = Uri.parse(treeUriString);
                String rootDocId = DocumentsContract.getTreeDocumentId(treeUri);
                Map<String, ChildDoc> rootChildren = listChildren(treeUri, rootDocId);
                String rootName = displayName(treeUriString);
                String savesDocId;
                if (rootChildren.containsKey("card0")
                        || rootChildren.containsKey("card1")
                        || rootChildren.containsKey("quick.dc2state")
                        || "saves".equalsIgnoreCase(rootName)) {
                    savesDocId = rootDocId;
                } else if ("DC2".equalsIgnoreCase(rootName)
                        || (rootChildren.containsKey("saves") && rootChildren.get("saves").isDir())) {
                    savesDocId = ensureSubdir(treeUri, rootDocId, rootChildren, "saves");
                } else {
                    String dc2DocId = ensureSubdir(treeUri, rootDocId, rootChildren, "DC2");
                    savesDocId = ensureSubdir(treeUri, dc2DocId, listChildren(treeUri, dc2DocId), "saves");
                }

                File mirror = new File(getFilesDir(), "picked_saves");
                File marker = new File(getFilesDir(), "picked_saves.uri");
                String previousUri = marker.isFile()
                        ? new String(Files.readAllBytes(marker.toPath()), StandardCharsets.UTF_8).trim()
                        : "";
                boolean sameTree = treeUriString.equals(previousUri);
                syncedStamps.clear();

                if (!hasSafSaveFiles(treeUri, savesDocId)) {
                    deleteContents(mirror);
                    mirror.mkdirs();
                    File localDefault = new File(fallbackSavesPath);
                    if (hasLocalSaveFiles(localDefault)) {
                        copyLocalDir(localDefault, mirror);
                        pushDir(treeUri, savesDocId, mirror);
                    }
                } else {
                    if (!sameTree) deleteContents(mirror);
                    mirror.mkdirs();
                    pullDir(treeUri, savesDocId, mirror, sameTree);
                }

                Files.write(marker.toPath(), treeUriString.getBytes(StandardCharsets.UTF_8));
                activeTreeUri = treeUri;
                activeSavesDocId = savesDocId;
                activeMirrorDir = mirror;
                return mirror.getAbsolutePath();
            } catch (IOException | RuntimeException failed) {
                Log.w(TAG, "cannot prepare saves in " + treeUriString, failed);
                activeTreeUri = null;
                activeSavesDocId = null;
                activeMirrorDir = null;
                return "";
            }
        }
    }

    private void pushActiveSaves() {
        synchronized (saveLock) {
            if (activeTreeUri == null || activeSavesDocId == null || activeMirrorDir == null) return;
            try {
                pushDir(activeTreeUri, activeSavesDocId, activeMirrorDir);
            } catch (IOException | RuntimeException failed) {
                Log.w(TAG, "cannot sync saves", failed);
            }
        }
    }

    private void runSyncLoop() {
        try {
            while (syncDirty.compareAndSet(true, false)) {
                pushActiveSaves();
            }
        } finally {
            syncRunning.set(false);
            if (syncDirty.get() && syncRunning.compareAndSet(false, true)) {
                syncExecutor.execute(this::runSyncLoop);
            }
        }
    }

    public void syncSaves() {
        syncDirty.set(true);
        if (syncRunning.compareAndSet(false, true)) {
            syncExecutor.execute(this::runSyncLoop);
        }
    }

    private void flushSavesOnPause() {
        if (activeTreeUri == null) return;
        syncDirty.set(false);
        Future<?> future = syncExecutor.submit(this::pushActiveSaves);
        try {
            future.get(3, TimeUnit.SECONDS);
        } catch (Exception ignored) {
        }
    }
}
