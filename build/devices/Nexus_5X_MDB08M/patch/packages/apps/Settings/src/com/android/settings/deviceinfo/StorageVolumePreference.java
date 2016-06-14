/*
 * Copyright (C) 2015 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.android.settings.deviceinfo;

import android.content.Context;
import android.content.res.ColorStateList;
import android.graphics.Color;
import android.graphics.drawable.Drawable;
import android.os.storage.StorageManager;
import android.os.storage.VolumeInfo;
import android.preference.Preference;
import android.text.format.Formatter;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.ImageView;
import android.widget.ProgressBar;
import android.util.Log;

import com.android.settings.R;
import com.android.settings.deviceinfo.StorageSettings.UnmountTask;

import java.io.File;
import org.json.JSONException;
import org.json.JSONObject;

/**
 * Preference line representing a single {@link VolumeInfo}, possibly including
 * quick actions like unmounting.
 */
public class StorageVolumePreference extends Preference {
    private final StorageManager mStorageManager;
    private final VolumeInfo mVolume;
    private StorageTera mStorageTera;

    private int mColor;
    private int mUsedPercent = -1;
    static final String TAG = "StorageVolumePreference";
    public StorageVolumePreference(Context context, VolumeInfo volume, int color) {
        super(context);

        mStorageTera =  new StorageTera(context);

        mStorageManager = context.getSystemService(StorageManager.class);
        mVolume = volume;
        mColor = color;

        setLayoutResource(R.layout.storage_volume);

        setKey(volume.getId());
        setTitle(mStorageManager.getBestVolumeDescription(volume));

        Drawable icon;
        if (VolumeInfo.ID_PRIVATE_INTERNAL.equals(volume.getId())) {
            icon = context.getDrawable(R.drawable.ic_settings_storage);
        } else if (volume.getId().equals("hcfs")) {
            icon = context.getDrawable(R.drawable.ic_settings_cloud_storage);
        } else {
            icon = context.getDrawable(R.drawable.ic_sim_sd);
        }

        if (volume.isMountedReadable()) {
            if (volume.getType() == VolumeInfo.TYPE_HCFS) {
                try {
                    String hcfsStat = mStorageTera.getHCFSStat();
                    JSONObject jObj = new JSONObject(hcfsStat);
                    long cloud_used = -1;
                    long quota = -1;
                    if (jObj.getBoolean("result")) {
                        JSONObject data = jObj.getJSONObject("data");
                        cloud_used = data.getLong("cloud_used");
                        quota = data.getLong("quota");
                    }
                    final long totalBytes = quota;
                    final long usedBytes = cloud_used;
                    final long freeBytes = totalBytes - usedBytes;

                    final String used = Formatter.formatFileSize(context, usedBytes);
                    final String total = Formatter.formatFileSize(context, totalBytes);
                    setSummary(context.getString(R.string.storage_volume_summary, used, total));
                    mUsedPercent = (int) ((usedBytes * 100) / totalBytes);
                } catch (JSONException e) {
                    Log.e(TAG, "Can't get HCFS stat", e);
                }
            } else {
                // TODO: move statfs() to background thread
                final File path = volume.getPath();
                final long freeBytes = path.getFreeSpace();
                final long totalBytes = path.getTotalSpace();
                final long usedBytes = totalBytes - freeBytes;

                final String used = Formatter.formatFileSize(context, usedBytes);
                final String total = Formatter.formatFileSize(context, totalBytes);
                setSummary(context.getString(R.string.storage_volume_summary, used, total));
                mUsedPercent = (int) ((usedBytes * 100) / totalBytes);
                if (freeBytes < mStorageManager.getStorageLowBytes(path)) {
                    mColor = StorageSettings.COLOR_WARNING;
                    icon = context.getDrawable(R.drawable.ic_warning_24dp);
                }
            }
        } else {
            setSummary(volume.getStateDescription());
            mUsedPercent = -1;
        }

        icon.mutate();
        icon.setTint(mColor);
        setIcon(icon);

        if (volume.getType() == VolumeInfo.TYPE_PUBLIC
                && volume.isMountedReadable()) {
            setWidgetLayoutResource(R.layout.preference_storage_action);
        }
    }

    @Override
    protected void onBindView(View view) {
        final ImageView unmount = (ImageView) view.findViewById(R.id.unmount);
        if (unmount != null) {
            unmount.setImageTintList(ColorStateList.valueOf(Color.parseColor("#8a000000")));
            unmount.setOnClickListener(mUnmountListener);
        }

        final ProgressBar progress = (ProgressBar) view.findViewById(android.R.id.progress);
        if (mVolume.getType() == VolumeInfo.TYPE_PRIVATE && mUsedPercent != -1) {
            progress.setVisibility(View.VISIBLE);
            progress.setProgress(mUsedPercent);
            progress.setProgressTintList(ColorStateList.valueOf(mColor));
        } else if (mVolume.getType() == VolumeInfo.TYPE_HCFS && mUsedPercent != -1) {
            progress.setVisibility(View.VISIBLE);
            progress.setProgress(mUsedPercent);
            progress.setProgressTintList(ColorStateList.valueOf(mColor));
        } else {
            progress.setVisibility(View.GONE);
        }

        super.onBindView(view);
    }

    private final View.OnClickListener mUnmountListener = new OnClickListener() {
        @Override
        public void onClick(View v) {
            new UnmountTask(getContext(), mVolume).execute();
        }
    };
}
