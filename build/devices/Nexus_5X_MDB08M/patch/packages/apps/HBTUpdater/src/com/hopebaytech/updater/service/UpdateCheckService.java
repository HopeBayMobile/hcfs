/*
 * Copyright (C) 2012 The CyanogenMod Project
 *
 * * Licensed under the GNU GPLv2 license
 *
 * The text of the license can be found in the LICENSE file
 * or at https://www.gnu.org/licenses/gpl-2.0.txt
 */

package com.hopebaytech.updater.service;

import android.app.IntentService;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.res.Resources;
import android.os.Parcelable;
import android.os.SystemProperties;
import android.preference.PreferenceManager;
import android.support.v4.app.NotificationCompat;
import android.text.TextUtils;
import android.util.Log;

import com.android.volley.DefaultRetryPolicy;
import com.android.volley.Response;
import com.android.volley.VolleyError;
import com.android.volley.VolleyLog;

import com.hopebaytech.updater.R;
import com.hopebaytech.updater.UpdateApplication;
import com.hopebaytech.updater.requests.UpdatesJsonObjectRequest;
import com.hopebaytech.updater.UpdatesSettings;
import com.hopebaytech.updater.misc.Constants;
import com.hopebaytech.updater.misc.State;
import com.hopebaytech.updater.misc.UpdateInfo;
import com.hopebaytech.updater.receiver.DownloadReceiver;
import com.hopebaytech.updater.utils.Utils;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.net.URI;
import java.util.Collections;
import java.util.Comparator;
import java.util.Date;
import java.util.LinkedList;

public class UpdateCheckService extends IntentService
	implements Response.ErrorListener, Response.Listener<JSONObject> {

    private static final String TAG = "UpdateCheckService";

    // Set this to true if the update service should check for smaller, test updates
    // This is for internal testing only
    private static final boolean TESTING_DOWNLOAD = false;

    // request actions
    public static final String ACTION_CHECK = "com.hopebaytech.hbtupdater.action.CHECK";
    public static final String ACTION_CANCEL_CHECK = "com.hopebaytech.hbtupdater.action.CANCEL_CHECK";

    // broadcast actions
    public static final String ACTION_CHECK_FINISHED = "com.hopebaytech.hbtupdater.action.UPDATE_CHECK_FINISHED";
    // extra for ACTION_CHECK_FINISHED: total amount of found updates
    public static final String EXTRA_UPDATE_COUNT = "update_count";
    // extra for ACTION_CHECK_FINISHED: amount of updates that are newer than what is installed
    public static final String EXTRA_REAL_UPDATE_COUNT = "real_update_count";
    // extra for ACTION_CHECK_FINISHED: amount of updates that were found for the first time
    public static final String EXTRA_NEW_UPDATE_COUNT = "new_update_count";
    // Aaron: extra for ACTION_CHECK_FINISHED: the update type (INCREMENTAL or FULL)
    public static final String EXTRA_UPDATE_TYPE = "update_type";

    // max. number of updates listed in the expanded notification
    private static final int EXPANDED_NOTIF_UPDATE_COUNT = 4;

    // DefaultRetryPolicy values for Volley
    private static final int UPDATE_REQUEST_TIMEOUT = 5000; // 5 seconds
    private static final int UPDATE_REQUEST_MAX_RETRIES = 3;

    public UpdateCheckService() {
        super("UpdateCheckService");
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        if (TextUtils.equals(intent.getAction(), ACTION_CANCEL_CHECK)) {
            ((UpdateApplication) getApplicationContext()).getQueue().cancelAll(TAG);
            return START_NOT_STICKY;
        }

        return super.onStartCommand(intent, flags, startId);
    }

	//guo: this is called by:
	//	(1) Utils.java's scheduleUpdateService()
	//	(2) UpdatesSettings.java
	//	(3) UpdateCheckReceiver.java
    @Override
    protected void onHandleIntent(Intent intent) {
        if (!Utils.isOnline(this)) {
            // Only check for updates if the device is actually connected to a network
            Log.i(TAG, "Could not check for updates. Not connected to the network.");
            return;
        }
		//Log.i(TAG, "onHandleIntent");
        getAvailableUpdates();//guo: this one connects to the OTA server
    }

    private void recordAvailableUpdates(LinkedList<UpdateInfo> availableUpdates,
            Intent finishedIntent) {

        if (availableUpdates == null) {
            sendBroadcast(finishedIntent); // Aaron: UpdatesSttings.java's BroadcastReceiver get this intent.
            return;
        }

        // Store the last update check time and ensure boot check completed is true
        Date d = new Date();
        PreferenceManager.getDefaultSharedPreferences(UpdateCheckService.this).edit()
                .putLong(Constants.LAST_UPDATE_CHECK_PREF, d.getTime())
                .putBoolean(Constants.BOOT_CHECK_COMPLETED, true)
                .apply();

        int realUpdateCount = finishedIntent.getIntExtra(EXTRA_REAL_UPDATE_COUNT, 0);
        UpdateApplication app = (UpdateApplication) getApplicationContext();

        // Write to log
        Log.i(TAG, "The update check successfully completed at " + d + " and found "
                + availableUpdates.size() + " updates ("
                + realUpdateCount + " newer than installed)");

        if (realUpdateCount != 0 && !app.isMainActivityActive()) {
            // There are updates available
            // The notification should launch the main app
            Intent i = new Intent(this, UpdatesSettings.class);
            i.putExtra(UpdatesSettings.EXTRA_UPDATE_LIST_UPDATED, true);
            PendingIntent contentIntent = PendingIntent.getActivity(this, 0, i,
                    PendingIntent.FLAG_ONE_SHOT);

            Resources res = getResources();
            String text = res.getQuantityString(R.plurals.not_new_updates_found_body,
                    realUpdateCount, realUpdateCount);

            // Get the notification ready
            NotificationCompat.Builder builder = new NotificationCompat.Builder(this)
                    .setSmallIcon(R.drawable.ic_system_update)
                    .setWhen(System.currentTimeMillis())
                    .setTicker(res.getString(R.string.not_new_updates_found_ticker))
                    .setContentTitle(res.getString(R.string.not_new_updates_found_title))
                    .setContentText(text)
                    .setContentIntent(contentIntent)
                    .setLocalOnly(true)
                    .setAutoCancel(true);

            LinkedList<UpdateInfo> realUpdates = new LinkedList<UpdateInfo>();
            for (UpdateInfo ui : availableUpdates) {
                if (ui.isNewerThanInstalled()) {
                    realUpdates.add(ui);
                }
            }

            Collections.sort(realUpdates, new Comparator<UpdateInfo>() {
                @Override
                public int compare(UpdateInfo lhs, UpdateInfo rhs) {
                    /* sort by date descending */
                    long lhsDate = lhs.getDate();
                    long rhsDate = rhs.getDate();
                    if (lhsDate == rhsDate) {
                        return 0;
                    }
                    return lhsDate < rhsDate ? 1 : -1;
                }
            });

			/* Aaron: comment
            NotificationCompat.InboxStyle inbox = new NotificationCompat.InboxStyle(builder)
                    .setBigContentTitle(text);
            int added = 0, count = realUpdates.size();

            for (UpdateInfo ui : realUpdates) {
                if (added < EXPANDED_NOTIF_UPDATE_COUNT) {
                    inbox.addLine(ui.getName());
                    added++;
                }
            }
            if (added != count) {
                inbox.setSummaryText(res.getQuantityString(R.plurals.not_additional_count,
                        count - added, count - added));
            }
            builder.setStyle(inbox);
            builder.setNumber(availableUpdates.size());
			*/

            //  if (count == 1) { //Aaron: comment

			//guo: way-1: UpdatesSetting(checkForUpdates()) ---> UpdateCheckService ---> DownloadReceiver ---> DownloadService
			//guo: way-2: UpdatesSetting(startDownload())                           ---> DownloadReceiver ---> DownloadService
			//guo: way-3: UpdateCheckReceiver               ---> UpdateCheckService ---> DownloadReceiver ---> DownloadService
			i = new Intent(this, DownloadReceiver.class); //guo: start DownloadReceiver.java
			i.setAction(DownloadReceiver.ACTION_START_DOWNLOAD);
			i.putExtra(DownloadReceiver.EXTRA_UPDATE_INFO, (Parcelable) realUpdates.getFirst());
			PendingIntent downloadIntent = PendingIntent.getBroadcast(this, 0, i,
					PendingIntent.FLAG_ONE_SHOT | PendingIntent.FLAG_UPDATE_CURRENT);

			builder.addAction(R.drawable.ic_tab_download,
					res.getString(R.string.not_action_download), downloadIntent);
            // } //Aaron: comment

            // Trigger the notification
            NotificationManager nm = (NotificationManager) getSystemService(NOTIFICATION_SERVICE);
            nm.notify(R.string.not_new_updates_found_title, builder.build());
        }

        sendBroadcast(finishedIntent);
    }

    //guo: the OTA server URL is here!!!
    private URI getServerURI() {
		//guo: this could be used for dynamically set the ota server ip
		//guo: `setprop hb.updater.url http://192.168.0.7:8000/upload/list`
        String propertyUpdateUri = SystemProperties.get("hb.updater.url");
        if (!TextUtils.isEmpty(propertyUpdateUri)) {
            return URI.create(propertyUpdateUri);
        }

		//default url
        String configUpdateUri = getString(R.string.conf_update_server_url_def); 
		//Log.d(TAG, "connect to url:" + configUpdateUri);
        return URI.create(configUpdateUri);
    }

	//connects to the OTA server
    private void getAvailableUpdates() {
		//Log.d(TAG, "getAvailableUpdates()");
        // Get the type of update we should check for
        int updateType = Utils.getUpdateType();

        // Get the actual ROM Update Server URL
        URI updateServerUri = getServerURI();
        UpdatesJsonObjectRequest request;
        try {
            request = new UpdatesJsonObjectRequest(updateServerUri.toASCIIString(),
                    Utils.getUserAgentString(this), buildUpdateRequest(updateType), this, this);
            // Improve request error tolerance
            request.setRetryPolicy(new DefaultRetryPolicy(UPDATE_REQUEST_TIMEOUT,
                        UPDATE_REQUEST_MAX_RETRIES, DefaultRetryPolicy.DEFAULT_BACKOFF_MULT));
            // Set the tag for the request, reuse logging tag
            request.setTag(TAG);
        } catch (JSONException e) {
            Log.e(TAG, "Could not build request", e);
            return;
        }

        ((UpdateApplication) getApplicationContext()).getQueue().add(request);
    }

	//guo: we don't use this, since our OTA server has only GET function
    private JSONObject buildUpdateRequest(int updateType) throws JSONException {
		//Log.d(TAG, "guo:buildUpdaterequest()");//guo
		if(true) return null;//guo
        JSONArray channels = new JSONArray();

        switch(updateType) {
            case Constants.UPDATE_TYPE_SNAPSHOT:
                channels.put("snapshot");
                break;
            case Constants.UPDATE_TYPE_NIGHTLY:
            default:
                channels.put("nightly");
                break;
        }
        JSONObject params = new JSONObject();
        params.put("device", TESTING_DOWNLOAD ? "cmtestdevice" : Utils.getDeviceType());
        params.put("channels", channels);
        params.put("source_incremental", Utils.getIncremental());

        JSONObject request = new JSONObject();
        request.put("method", "get_all_builds");
        request.put("params", params);

        return request;
    }

    private LinkedList<UpdateInfo> parseJSON(String jsonString/*, int updateType*/) {
        LinkedList<UpdateInfo> updates = new LinkedList<UpdateInfo>();
        try {
            JSONObject result = new JSONObject(jsonString);
            JSONArray updateList = result.getJSONArray("result");
            int length = updateList.length();

            Log.d(TAG, "Got update JSON data with " + length + " entries");

            for (int i = 0; i < length; i++) {
                if (updateList.isNull(i)) {
                    continue;
                }
                JSONObject item = updateList.getJSONObject(i);
                UpdateInfo info = parseUpdateJSONObject(item/*, updateType*/);
				//Log.d(TAG, "guo:" + info.toString());//guo
                if (info != null) {
                    updates.add(info);
                }
            }
        } catch (JSONException e) {
            Log.e(TAG, "Error in JSON result", e);
        }
        return updates;
    }

	//json is actually parsed here
    private UpdateInfo parseUpdateJSONObject(JSONObject obj/*, int updateType*/) throws JSONException {
        UpdateInfo ui = new UpdateInfo.Builder()
                .setFileName(obj.getString("filename"))
                .setDownloadUrl(obj.getString("url"))
                //.setChangelogUrl(obj.getString("changes"))
                //.setMD5Sum(obj.getString("md5sum"))
                //.setApiLevel(obj.getInt("api_level"))
                .setBuildDate(obj.getLong("timestamp"))
                //.setType(obj.getString("channel"))
                //.setIncremental(obj.getString("incremental"))
                .build();

        if (!ui.isNewerThanInstalled()) {
            Log.d(TAG, "Build " + ui.getFileName() + " is older than the installed build");
            return null;
        }

        return ui;
    }

	//Response.ErrorListener
    @Override
    public void onErrorResponse(VolleyError volleyError) {
        VolleyLog.e("Error: ", volleyError.getMessage());
        VolleyLog.e("Error type: " + volleyError.toString());
        Intent intent = new Intent(ACTION_CHECK_FINISHED);
        sendBroadcast(intent);
    }

	//Response.Listener<T>
    @Override
    public void onResponse(JSONObject jsonObject) {
        //int updateType = Utils.getUpdateType();

        LinkedList<UpdateInfo> lastUpdates = State.loadState(this);
        LinkedList<UpdateInfo> updates = parseJSON(jsonObject.toString()/*, updateType*/);

        int newUpdates = 0, realUpdates = 0;
        for (UpdateInfo ui : updates) {
            if (!lastUpdates.contains(ui)) {
                newUpdates++;
            }
            if (ui.isNewerThanInstalled()) {
                realUpdates++;
            }
        }

        // Aaron: determine the udpate type 
        int COUNT_FOR_FULL_UPDATE = 3;
        UpdateInfo.Type updateType;
        if (realUpdates > COUNT_FOR_FULL_UPDATE) {
            updateType = UpdateInfo.Type.FULL;
        } else {
            updateType = UpdateInfo.Type.INCREMENTAL;
        }

		//guo: this is the finished Intent
        Intent intent = new Intent(ACTION_CHECK_FINISHED); //guo: UpdatesSettings.java' internal receiver get this
        intent.putExtra(EXTRA_UPDATE_COUNT, updates.size());
        intent.putExtra(EXTRA_REAL_UPDATE_COUNT, realUpdates);
        intent.putExtra(EXTRA_NEW_UPDATE_COUNT, newUpdates);
        intent.putExtra(EXTRA_UPDATE_TYPE, updateType.ordinal()); // Aaron: pass the update type 

        recordAvailableUpdates(updates, intent);
        State.saveState(this, updates);
    }
}
