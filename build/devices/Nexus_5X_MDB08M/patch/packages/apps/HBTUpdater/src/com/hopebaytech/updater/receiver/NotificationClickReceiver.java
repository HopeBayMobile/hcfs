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

import com.hopebaytech.updater.UpdatesSettings;

//guo add
import android.util.Log;

public class NotificationClickReceiver extends BroadcastReceiver{

    private String TAG="NotificationClickReceiver";//guo

    @Override
    public void onReceive(Context context, Intent intent) {

		Log.d(TAG, "guo: onReceive()");

        // Bring the main app to the foreground
        // guo: when hit the downloadManager icon in the notification bar, this receiver will bring the updatesSettings to the foreground
        Intent i = new Intent(context, UpdatesSettings.class);
        i.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_SINGLE_TOP | 
                Intent.FLAG_ACTIVITY_CLEAR_TOP | Intent.FLAG_ACTIVITY_EXCLUDE_FROM_RECENTS);
        context.startActivity(i);
    }
}
