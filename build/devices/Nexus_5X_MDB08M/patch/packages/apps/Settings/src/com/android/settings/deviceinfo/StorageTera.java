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

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.util.Log;
import android.os.IBinder;
import android.os.RemoteException;

import com.hopebaytech.hcfsmgmt.terafonnapiservice.AppStatus;
import com.hopebaytech.hcfsmgmt.terafonnapiservice.AppStatus;
import com.hopebaytech.hcfsmgmt.terafonnapiservice.IFetchAppDataListener;
import com.hopebaytech.hcfsmgmt.terafonnapiservice.ITeraFonnApiService;
import com.hopebaytech.hcfsmgmt.terafonnapiservice.ITrackAppStatusListener;

public class StorageTera {

    private static Context mContext;
    private final String CLASSNAME = getClass().getSimpleName();
    private final String TAG = "StorageTera";
    private static ITeraFonnApiService mTeraService = null;
    private boolean mIsBound = false;
    private final ServiceConnection mConn = new ServiceConnection() {
        @Override
        public void onServiceConnected(ComponentName name, IBinder service) {
            Log.d(TAG, "on Service connected");
            mTeraService = ITeraFonnApiService.Stub.asInterface(service);
        }

        @Override
        public void onServiceDisconnected(ComponentName name) {
            Log.d(TAG, "on Service disconnected");
            mTeraService = null;
        }

    };


    public StorageTera(Context context) {
        if (mTeraService == null) {
            mContext=context;
            Intent intent = new Intent();
            final String packageName = "com.hopebaytech.hcfsmgmt";
            final String className = "com.hopebaytech.hcfsmgmt.terafonnapiservice.TeraFonnApiService";
            intent.setComponent(new ComponentName(packageName, className));
            ComponentName serviceComponentName = mContext.startService(intent);
            if (serviceComponentName != null) {
                mIsBound = mContext.bindService(intent, mConn, Context.BIND_AUTO_CREATE);
            } else {
                Log.e(TAG, "Failed to startService");
            }
        }
    }

    public String getHCFSStat() {
        String result="{\"result\":\"false\"}";
        try {
            result = mTeraService.getHCFSStat();
        } catch (RemoteException e) {
            Log.e(TAG, Log.getStackTraceString(e));
        }
        return result;
    }

    public boolean hcfsEnabled() {
        boolean enabled = true;
        if (!serviceReady()) {
            return false;
        }
        try {
            enabled = mTeraService.hcfsEnabled();
        } catch (RemoteException e) {
            Log.e(TAG, Log.getStackTraceString(e));
        }
        return enabled;
    }

    public boolean serviceReady() {
        if (mTeraService == null) {
            return false;
        } else {
            return true;
        }
    }

    public void unbind() {
        if (mIsBound) {
            Log.d(TAG, "HCFS API Service unbind");
            mContext.unbindService(mConn);
            mIsBound = false;
        }
    }
}
