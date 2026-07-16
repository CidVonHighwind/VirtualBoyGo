package com.nintendont.virtualboygo;

import android.app.NativeActivity;
import android.content.ContentResolver;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.SharedPreferences;
import android.content.UriPermission;
import android.database.Cursor;
import android.net.Uri;
import android.os.BatteryManager;
import android.os.Bundle;
import android.os.ParcelFileDescriptor;
import android.provider.DocumentsContract;
import android.util.Log;

import java.util.ArrayList;
import java.util.List;

// Extends NativeActivity (native side still loads as before) only to get the
// onCreate/onActivityResult lifecycle callbacks the SAF folder picker
// (ACTION_OPEN_DOCUMENT_TREE) needs - see core/RomScanner.h for why SAF. The
// methods below are called from native code via JNI (core/AndroidRomAccess.h);
// they're plain polled methods, so no JNI_OnLoad/RegisterNatives is needed.
public class MainActivity extends NativeActivity {
    private static final String TAG = "VirtualBoyGo";
    private static final String PREFS_NAME = "virtualboygo";
    private static final String PREF_ROMS_TREE_URI = "roms_tree_uri";
    private static final int REQUEST_PICK_ROMS_FOLDER = 1001;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        if (getRomsTreeUriString() == null) {
            requestPickRomsFolder();
        }
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode != REQUEST_PICK_ROMS_FOLDER || resultCode != RESULT_OK || data == null) {
            return;
        }
        Uri treeUri = data.getData();
        if (treeUri == null) {
            return;
        }

        // Persistable so it survives restarts (the point of SAF: pick once).
        // Read+write, since save states/SRAM go back into this folder.
        getContentResolver().takePersistableUriPermission(treeUri,
                Intent.FLAG_GRANT_READ_URI_PERMISSION | Intent.FLAG_GRANT_WRITE_URI_PERMISSION);

        SharedPreferences prefs = getSharedPreferences(PREFS_NAME, MODE_PRIVATE);
        prefs.edit().putString(PREF_ROMS_TREE_URI, treeUri.toString()).apply();
        Log.d(TAG, "ROMs folder set: " + treeUri);
    }

    // Launches the folder picker. Only called from onCreate, before any VR
    // session exists: launching it while already immersed makes Horizon OS
    // drop VR focus to the system shell and never hand it back.
    private void requestPickRomsFolder() {
        Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT_TREE);
        intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION | Intent.FLAG_GRANT_WRITE_URI_PERMISSION
                | Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION);
        startActivityForResult(intent, REQUEST_PICK_ROMS_FOLDER);
    }

    // ---- Called from native code (core/AndroidRomAccess.h) via JNI ----

    // 0-100 device battery level, for the in-menu battery indicator.
    public int getBatteryLevel() {
        Intent batteryIntent = registerReceiver(null, new IntentFilter(Intent.ACTION_BATTERY_CHANGED));
        if (batteryIntent == null) {
            return -1;
        }
        int level = batteryIntent.getIntExtra(BatteryManager.EXTRA_LEVEL, -1);
        int scale = batteryIntent.getIntExtra(BatteryManager.EXTRA_SCALE, -1);
        if (level < 0 || scale <= 0) {
            return -1;
        }
        return (int) (level / (float) scale * 100);
    }

    // Lets the user pick a different folder after first launch. Only clears
    // the saved folder - relaunching mid-session loses VR focus (see
    // requestPickRomsFolder), so the user restarts manually and onCreate's
    // picker fires again (getRomsTreeUriString is now null).
    public void requestChangeRomsFolder() {
        String saved = getRomsTreeUriString();
        if (saved != null) {
            try {
                getContentResolver().releasePersistableUriPermission(Uri.parse(saved),
                        Intent.FLAG_GRANT_READ_URI_PERMISSION | Intent.FLAG_GRANT_WRITE_URI_PERMISSION);
            } catch (Exception e) {
                Log.e(TAG, "releasePersistableUriPermission failed", e);
            }
        }
        getSharedPreferences(PREFS_NAME, MODE_PRIVATE).edit().remove(PREF_ROMS_TREE_URI).apply();
        Log.d(TAG, "ROMs folder cleared - restart the app to pick a new one");
    }

    // null if no folder is picked or it's no longer accessible - checked
    // against the OS's persisted-permission list, not just the cached pref.
    public String getRomsTreeUriString() {
        SharedPreferences prefs = getSharedPreferences(PREFS_NAME, MODE_PRIVATE);
        String saved = prefs.getString(PREF_ROMS_TREE_URI, null);
        if (saved == null) {
            return null;
        }
        Uri treeUri = Uri.parse(saved);
        for (UriPermission perm : getContentResolver().getPersistedUriPermissions()) {
            if (perm.getUri().equals(treeUri) && perm.isReadPermission()) {
                return saved;
            }
        }
        return null;
    }

    // Flat [name0, uri0, name1, uri1, ...] for every ".vb" file directly in
    // the chosen folder (non-recursive) - flat to keep the JNI side simple.
    public String[] listRomFiles() {
        String treeUriString = getRomsTreeUriString();
        if (treeUriString == null) {
            return new String[0];
        }
        Uri treeUri = Uri.parse(treeUriString);
        Uri childrenUri = DocumentsContract.buildChildDocumentsUriUsingTree(treeUri, DocumentsContract.getTreeDocumentId(treeUri));

        List<String> results = new ArrayList<>();
        ContentResolver resolver = getContentResolver();
        try (Cursor cursor = resolver.query(childrenUri, new String[]{
                DocumentsContract.Document.COLUMN_DOCUMENT_ID,
                DocumentsContract.Document.COLUMN_DISPLAY_NAME
        }, null, null, null)) {
            if (cursor != null) {
                while (cursor.moveToNext()) {
                    String documentId = cursor.getString(0);
                    String displayName = cursor.getString(1);
                    if (displayName == null || !displayName.toLowerCase().endsWith(".vb")) {
                        continue;
                    }
                    String name = displayName.substring(0, displayName.length() - 3);
                    Uri docUri = DocumentsContract.buildDocumentUriUsingTree(treeUri, documentId);
                    results.add(name);
                    results.add(docUri.toString());
                }
            }
        } catch (Exception e) {
            Log.e(TAG, "listRomFiles failed", e);
        }
        return results.toArray(new String[0]);
    }

    // Returns a detached raw fd (caller/native owns it and must close() it)
    // for read-only access to the given document URI string, or -1 on
    // failure.
    public int openRomFileDescriptor(String documentUriString) {
        try {
            Uri uri = Uri.parse(documentUriString);
            ParcelFileDescriptor pfd = getContentResolver().openFileDescriptor(uri, "r");
            if (pfd == null) {
                return -1;
            }
            return pfd.detachFd();
        } catch (Exception e) {
            Log.e(TAG, "openRomFileDescriptor failed: " + documentUriString, e);
            return -1;
        }
    }

    // ---- Save-state/SRAM I/O inside the ROMs folder (.srm in the root,
    // .state/.stateimg in a "States" subfolder). Unlike ROMs, these are
    // looked up (or created) by name each time, since SAF hands back no
    // stable URI for a file that doesn't exist yet. ----

    // documentId of parentDocumentId's child named displayName, or null.
    private String findChildDocumentId(Uri treeUri, String parentDocumentId, String displayName) {
        Uri childrenUri = DocumentsContract.buildChildDocumentsUriUsingTree(treeUri, parentDocumentId);
        try (Cursor cursor = getContentResolver().query(childrenUri, new String[]{
                DocumentsContract.Document.COLUMN_DOCUMENT_ID,
                DocumentsContract.Document.COLUMN_DISPLAY_NAME
        }, null, null, null)) {
            if (cursor != null) {
                while (cursor.moveToNext()) {
                    if (displayName.equals(cursor.getString(1))) {
                        return cursor.getString(0);
                    }
                }
            }
        } catch (Exception e) {
            Log.e(TAG, "findChildDocumentId failed: " + displayName, e);
        }
        return null;
    }

    // documentId of the ROMs folder's "States" subfolder, creating it if it
    // doesn't exist yet. null on failure.
    private String getOrCreateStatesDirDocumentId(Uri treeUri) {
        String rootDocId = DocumentsContract.getTreeDocumentId(treeUri);
        String existing = findChildDocumentId(treeUri, rootDocId, "States");
        if (existing != null) {
            return existing;
        }
        try {
            Uri parentUri = DocumentsContract.buildDocumentUriUsingTree(treeUri, rootDocId);
            Uri createdUri = DocumentsContract.createDocument(getContentResolver(), parentUri,
                    DocumentsContract.Document.MIME_TYPE_DIR, "States");
            return createdUri != null ? DocumentsContract.getDocumentId(createdUri) : null;
        } catch (Exception e) {
            Log.e(TAG, "getOrCreateStatesDirDocumentId failed", e);
            return null;
        }
    }

    // documentId of fileName's parent within the ROMs tree - the "States"
    // subfolder if inStatesDir, else the tree root. createIfMissing only
    // matters for the States subfolder (the root always exists); null if
    // it doesn't exist and wasn't created.
    private String getParentDocumentId(Uri treeUri, boolean inStatesDir, boolean createIfMissing) {
        if (!inStatesDir) {
            return DocumentsContract.getTreeDocumentId(treeUri);
        }
        if (createIfMissing) {
            return getOrCreateStatesDirDocumentId(treeUri);
        }
        return findChildDocumentId(treeUri, DocumentsContract.getTreeDocumentId(treeUri), "States");
    }

    // Opens (creating fileName - and its parent "States" subfolder, if
    // inStatesDir - if they don't exist yet) a writable fd, truncating any
    // existing content. Returns a detached raw fd (caller/native owns it
    // and must close() it), or -1 on failure or if no folder is picked.
    public int openRomsFileForWrite(String fileName, boolean inStatesDir) {
        String treeUriString = getRomsTreeUriString();
        if (treeUriString == null) {
            return -1;
        }
        Uri treeUri = Uri.parse(treeUriString);
        try {
            String parentDocId = getParentDocumentId(treeUri, inStatesDir, true);
            if (parentDocId == null) {
                return -1;
            }

            String fileDocId = findChildDocumentId(treeUri, parentDocId, fileName);
            Uri fileUri;
            if (fileDocId != null) {
                fileUri = DocumentsContract.buildDocumentUriUsingTree(treeUri, fileDocId);
            } else {
                Uri parentUri = DocumentsContract.buildDocumentUriUsingTree(treeUri, parentDocId);
                fileUri = DocumentsContract.createDocument(getContentResolver(), parentUri, "application/octet-stream", fileName);
                if (fileUri == null) {
                    return -1;
                }
            }

            ParcelFileDescriptor pfd = getContentResolver().openFileDescriptor(fileUri, "wt");
            if (pfd == null) {
                return -1;
            }
            return pfd.detachFd();
        } catch (Exception e) {
            Log.e(TAG, "openRomsFileForWrite failed: " + fileName, e);
            return -1;
        }
    }

    // Read counterpart to openRomsFileForWrite - -1 if fileName doesn't
    // exist or no folder is picked.
    public int openRomsFileForRead(String fileName, boolean inStatesDir) {
        String treeUriString = getRomsTreeUriString();
        if (treeUriString == null) {
            return -1;
        }
        Uri treeUri = Uri.parse(treeUriString);
        try {
            String parentDocId = getParentDocumentId(treeUri, inStatesDir, false);
            if (parentDocId == null) {
                return -1;
            }
            String fileDocId = findChildDocumentId(treeUri, parentDocId, fileName);
            if (fileDocId == null) {
                return -1;
            }
            Uri fileUri = DocumentsContract.buildDocumentUriUsingTree(treeUri, fileDocId);
            ParcelFileDescriptor pfd = getContentResolver().openFileDescriptor(fileUri, "r");
            if (pfd == null) {
                return -1;
            }
            return pfd.detachFd();
        } catch (Exception e) {
            Log.e(TAG, "openRomsFileForRead failed: " + fileName, e);
            return -1;
        }
    }

    // true if fileName exists inside the picked ROMs folder (root or
    // "States" subfolder, per inStatesDir).
    public boolean romsFileExists(String fileName, boolean inStatesDir) {
        String treeUriString = getRomsTreeUriString();
        if (treeUriString == null) {
            return false;
        }
        Uri treeUri = Uri.parse(treeUriString);
        String parentDocId = getParentDocumentId(treeUri, inStatesDir, false);
        if (parentDocId == null) {
            return false;
        }
        return findChildDocumentId(treeUri, parentDocId, fileName) != null;
    }
}
