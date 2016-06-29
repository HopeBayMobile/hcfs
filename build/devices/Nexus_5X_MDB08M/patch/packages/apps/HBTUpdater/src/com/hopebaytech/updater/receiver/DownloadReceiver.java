/*
 * Copyright (C) 2014 The CyanogenMod Project
 *
 * * Licensed under the GNU GPLv2 license
 *
 * The text of the license can be found in the LICENSE file
 * or at https://www.gnu.org/licenses/gpl-2.0.txt
 */

package com.hopebaytech.updater.receiver;

import android.app.DownloadManager;
import android.app.StatusBarManager;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.preference.PreferenceManager;
import android.util.Log;
import android.widget.Toast;

import com.hopebaytech.updater.R;
import com.hopebaytech.updater.misc.Constants;
import com.hopebaytech.updater.misc.UpdateInfo;
import com.hopebaytech.updater.service.DownloadCompleteIntentService;
import com.hopebaytech.updater.service.DownloadService;
import com.hopebaytech.updater.utils.Utils;

import java.io.IOException;

public class DownloadReceiver extends BroadcastReceiver{
    private static final String TAG = "DownloadReceiver";

    public static final String ACTION_START_DOWNLOAD = "com.hopebaytech.hbtupdater.action.START_DOWNLOAD";
    public static final String EXTRA_UPDATE_INFO = "update_info";

    public static final String ACTION_DOWNLOAD_STARTED = "com.hopebaytech.hbtupdater.action.DOWNLOAD_STARTED";

    static final String ACTION_INSTALL_UPDATE = "com.hopebaytech.hbtupdater.action.INSTALL_UPDATE";
    static final String EXTRA_FILENAME = "filename";

    @Override
    public void onReceive(Context context, Intent intent) {
        String action = intent.getAction();

        if (ACTION_START_DOWNLOAD.equals(action)) {//guo: from UpdateCheckService.java or AndroidManifest.xml
			//guo: UpdatesSettings.java (startDownload()) 
			//Log.e(TAG, "started by ACTION_START_DOWNLOAD");
            UpdateInfo ui = (UpdateInfo) intent.getParcelableExtra(EXTRA_UPDATE_INFO);
            handleStartDownload(context, ui);
        } else if (DownloadManager.ACTION_DOWNLOAD_COMPLETE.equals(action)) {//guo:AndroidManifest.xml, get from the DownloadManager
            long id = intent.getLongExtra(DownloadManager.EXTRA_DOWNLOAD_ID, -1);
            handleDownloadComplete(context, id);
        } else if (ACTION_INSTALL_UPDATE.equals(action)) {//guo: DownloadNotifier.java's createInstallPendingIntent() call this
            StatusBarManager sb = (StatusBarManager) context.getSystemService(Context.STATUS_BAR_SERVICE);
            sb.collapsePanels();
            String fileName = intent.getStringExtra(EXTRA_FILENAME);
            try {
                Utils.triggerUpdate(context, fileName);//guo: reboot into recovery mode
            } catch (IOException e) {
                Log.e(TAG, "Unable to reboot into recovery mode", e);
                Toast.makeText(context, R.string.apply_unable_to_reboot_toast,
                            Toast.LENGTH_SHORT).show();
                Utils.cancelNotification(context);
            }
        }
    }

    private void handleStartDownload(Context context, UpdateInfo ui) {
		//Log.e(TAG, "starts DownloadService");
        DownloadService.start(context, ui);
    }

    private void handleDownloadComplete(Context context, long id) {
        SharedPreferences prefs = PreferenceManager.getDefaultSharedPreferences(context);
        long enqueued = prefs.getLong(Constants.DOWNLOAD_ID, -1);

        if (enqueued < 0 || id < 0 || id != enqueued) {
            return;
        }

        String downloadedMD5 = prefs.getString(Constants.DOWNLOAD_MD5, "");
        String incrementalFor = prefs.getString(Constants.DOWNLOAD_INCREMENTAL_FOR, null);

        Log.d(TAG, "downloadedMD5=" + downloadedMD5 + ", incrementalFor=" + incrementalFor);

        // Send off to DownloadCompleteIntentService
        Intent intent = new Intent(context, DownloadCompleteIntentService.class);
        intent.putExtra(Constants.DOWNLOAD_ID, id);
        intent.putExtra(Constants.DOWNLOAD_MD5, downloadedMD5);
        intent.putExtra(Constants.DOWNLOAD_INCREMENTAL_FOR, incrementalFor);
        context.startService(intent);

        // Clear the shared prefs
        prefs.edit()
                .remove(Constants.DOWNLOAD_MD5)
                .remove(Constants.DOWNLOAD_ID)
                .remove(Constants.DOWNLOAD_INCREMENTAL_FOR)
                .apply();
    }
}
