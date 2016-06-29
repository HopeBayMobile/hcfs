/*
 * Copyright (C) 2013 The CyanogenMod Project
 *
 * * Licensed under the GNU GPLv2 license
 *
 * The text of the license can be found in the LICENSE file
 * or at https://www.gnu.org/licenses/gpl-2.0.txt
 */

package com.hopebaytech.updater.utils;

import android.app.AlarmManager;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.net.ConnectivityManager;
import android.net.NetworkInfo;
import android.os.Environment;
import android.os.PowerManager;
import android.os.SystemProperties;
import android.os.UserHandle;
import android.os.storage.StorageManager;
import android.os.storage.StorageVolume;
import android.preference.PreferenceManager;
import android.text.TextUtils;
import android.util.Log;

import com.hopebaytech.updater.R;
import com.hopebaytech.updater.misc.Constants;
import com.hopebaytech.updater.service.UpdateCheckService;

import java.io.File;
import java.io.IOException;
import java.io.OutputStream;

public class Utils {

	//guo: add
	private static boolean isWifiOnly=true;

    private Utils() {
        // this class is not supposed to be instantiated
    }

	//guo add
	public static boolean getWifiOnly(){
		return isWifiOnly;
	}

	public static void setWifiOnly(boolean value){
		isWifiOnly = value;
	}
	//end guo add

    public static File makeUpdateFolder() {
		// /storage/emulated/0/hbtupdater/
		// in HCFS system, we do `mount -o bind /data/media/0/hbtupdater /storage/emulated/0/hbtupdater`
		// to redirect every thing to /data/media/0/hbtupdater
        return new File(Environment.getExternalStorageDirectory(),
                Constants.UPDATES_FOLDER);
    }

    //guo: cancel Notification in the notification bar
    public static void cancelNotification(Context context) {
        final NotificationManager nm =
                (NotificationManager) context.getSystemService(Context.NOTIFICATION_SERVICE);
        nm.cancel(R.string.not_new_updates_found_title);
        nm.cancel(R.string.not_download_success);
    }

    public static String getDeviceType() {
		//guo: getprop DON'T HAVE THIS
        return SystemProperties.get("ro.cm.device");
    }

    public static String getInstalledVersion() {
		//guo: getprop DON'T HAVE THIS
        return SystemProperties.get("ro.cm.version");
    }

    public static int getInstalledApiLevel() {
		//guo: getprop has this
        return SystemProperties.getInt("ro.build.version.sdk", 0);
    }

    public static long getInstalledBuildDate() {
		//guo: getprop has this
        return SystemProperties.getLong("ro.build.date.utc", 0);
    }

    public static String getIncremental() {
		//guo: system/build.prop has ro.build.version.incremental
        return SystemProperties.get("ro.build.version.incremental");
    }

    public static String getUserAgentString(Context context) {
        try {
            PackageManager pm = context.getPackageManager();
            PackageInfo pi = pm.getPackageInfo(context.getPackageName(), 0);
            return pi.packageName + "/" + pi.versionName;
        } catch (PackageManager.NameNotFoundException nnfe) {
            return null;
        }
    }

	//guo: check network connection status
    public static boolean isOnline(Context context) {
        ConnectivityManager cm = (ConnectivityManager) context.getSystemService(Context.CONNECTIVITY_SERVICE);
        NetworkInfo netInfo = cm.getActiveNetworkInfo();
        if (netInfo != null && netInfo.isConnected()) {
            return true;
        }
        return false;
    }

    public static void scheduleUpdateService(Context context, int updateFrequency) {
        // Load the required settings from preferences
        SharedPreferences prefs = PreferenceManager.getDefaultSharedPreferences(context);
        long lastCheck = prefs.getLong(Constants.LAST_UPDATE_CHECK_PREF, 0);

        // Get the intent ready
        Intent i = new Intent(context, UpdateCheckService.class);
        i.setAction(UpdateCheckService.ACTION_CHECK);
        PendingIntent pi = PendingIntent.getService(context, 0, i, PendingIntent.FLAG_UPDATE_CURRENT);

        // Clear any old alarms and schedule the new alarm
        AlarmManager am = (AlarmManager) context.getSystemService(Context.ALARM_SERVICE);
        am.cancel(pi);

        if (updateFrequency != Constants.UPDATE_FREQ_NONE) {
            am.setRepeating(AlarmManager.RTC_WAKEUP, lastCheck + updateFrequency, updateFrequency, pi);
        }
    }

	//guo: start to execute otapackage.zip install and reboot into recovery mode
    public static void triggerUpdate(Context context, String updateFileName) throws IOException {
        // Add the update folder/file name
        File primaryStorage = Environment.getExternalStorageDirectory();
        // If the path is emulated, translate it, if not return the original path
        String updatePath = Environment.maybeTranslateEmulatedPathToInternal(
                primaryStorage).getAbsolutePath();
        // Create the path for the update package
        String updatePackagePath = updatePath + "/" + Constants.UPDATES_FOLDER + "/" + updateFileName;

        /*
         * maybeTranslateEmulatedPathToInternal requires that we have a full path to a file (not just
         * a directory) and have read access to the file via both the emulated and actual paths.  As
         * this is currently done, we lack the ability to read the file via the actual path, so the
         * translation ends up failing.  Until this is all updated to download and store the file in
         * a sane way, manually perform the translation that is needed in order for uncrypt to be
         * able to find the file.
         */
        updatePackagePath = updatePackagePath.replace("storage/emulated", "data/media");

        // Reboot into recovery and trigger the update
        // guo: this line reboot into the recovery mode
        android.os.RecoverySystem.installPackage(context, new File(updatePackagePath));
    }

    public static int getUpdateType() {
        int updateType = Constants.UPDATE_TYPE_NIGHTLY;
        try {
            String cmReleaseType = SystemProperties.get( Constants.PROPERTY_CM_RELEASETYPE);

            // Treat anything that is not SNAPSHOT as NIGHTLY
            if (!cmReleaseType.isEmpty()) {
                if (TextUtils.equals(cmReleaseType, Constants.CM_RELEASETYPE_SNAPSHOT)) {
					updateType = Constants.UPDATE_TYPE_SNAPSHOT;
                }
            }
        } catch (RuntimeException ignored) {
        }

        return updateType;
    }

    public static boolean hasLeanback(Context context) {
        PackageManager packageManager = context.getPackageManager();
        return packageManager.hasSystemFeature(PackageManager.FEATURE_LEANBACK);
    }
}
