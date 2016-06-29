/*
 * Copyright (C) 2012 The CyanogenMod Project
 *
 * * Licensed under the GNU GPLv2 license
 *
 * The text of the license can be found in the LICENSE file
 * or at https://www.gnu.org/licenses/gpl-2.0.txt
 */

package com.hopebaytech.updater.receiver;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.net.ConnectivityManager;
import android.preference.PreferenceManager;
import android.util.Log;

import com.hopebaytech.updater.misc.Constants;
import com.hopebaytech.updater.service.UpdateCheckService;
import com.hopebaytech.updater.utils.Utils;

//guo: this one is up by BOOT_COMPLETE in AndroidManifest.xml
public class UpdateCheckReceiver extends BroadcastReceiver {
    private static final String TAG = "UpdateCheckReceiver";

    @Override
    public void onReceive(Context context, Intent intent) {
        // Load the required settings from preferences
        SharedPreferences prefs = PreferenceManager.getDefaultSharedPreferences(context);
        //int updateFrequency = prefs.getInt(Constants.UPDATE_CHECK_PREF, Constants.UPDATE_FREQ_WEEKLY);//guo mark
        //guo: we hardcode that everyday checks the update
        int updateFrequency = Constants.UPDATE_FREQ_DAILY; //guo modify

        // Check if we are set to manual updates and don't do anything
        if (updateFrequency == Constants.UPDATE_FREQ_NONE) {
            return;
        }

        // Not set to manual updates, parse the received action
        final String action = intent.getAction();
        if (ConnectivityManager.CONNECTIVITY_ACTION.equals(action)) {//guo: AndroidManifest.xml: android.net.conn.CONNECTIVITY_CHANGE
            // Connectivity has changed
            boolean hasConnection = !intent.getBooleanExtra(ConnectivityManager.EXTRA_NO_CONNECTIVITY, false);
            Log.i(TAG, "Got connectivity change, has connection: " + hasConnection);
            if (!hasConnection) {
                return;
            }
        } else if (Intent.ACTION_BOOT_COMPLETED.equals(action)) {//guo: AndroidManifest.xml
            // We just booted. Store the boot check state
            prefs.edit().putBoolean(Constants.BOOT_CHECK_COMPLETED, false).apply();
        }

        // Handle the actual update check based on the defined frequency
        if (updateFrequency == Constants.UPDATE_FREQ_AT_BOOT) {
            boolean bootCheckCompleted = prefs.getBoolean(Constants.BOOT_CHECK_COMPLETED, false);
            if (!bootCheckCompleted) {//guo: if setup for boot to check, start the UpdateCheckService.java
                Log.i(TAG, "Start an on-boot check");
                Intent i = new Intent(context, UpdateCheckService.class);
                i.setAction(UpdateCheckService.ACTION_CHECK);//guo: who get this action??? CAN'T FIND IT???
                context.startService(i);
            } else {
                // Nothing to do
                Log.i(TAG, "On-boot update check was already completed.");
                return;
            }
        } else if (updateFrequency > 0) {
            Log.i(TAG, "Scheduling future, repeating update checks.");
            Utils.scheduleUpdateService(context, updateFrequency * 1000);//guo: this will start UpdateCheckService
        }
    }
}
