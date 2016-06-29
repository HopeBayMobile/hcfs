/*
 * Copyright (C) 2012 The CyanogenMod Project
 *
 * * Licensed under the GNU GPLv2 license
 *
 * The text of the license can be found in the LICENSE file
 * or at https://www.gnu.org/licenses/gpl-2.0.txt
 */

package com.hopebaytech.updater.misc;

import android.content.Context;
import android.os.Parcel;
import android.os.Parcelable;
import android.text.TextUtils;
import android.util.Log;

import com.hopebaytech.updater.utils.Utils;

import java.io.File;
import java.io.Serializable;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

public class UpdateInfo implements Parcelable, Serializable {

    private final String TAG = UpdateInfo.class.getSimpleName();
    private static final long serialVersionUID = 5499890003569313403L;
    private static final Pattern sIncrementalPattern =
            Pattern.compile("^incremental-(.*)-(.*).zip$");

    //public static final String CHANGELOG_EXTENSION = ".changelog.html";//guo: we don't need this

    public enum Type {
        UNKNOWN,
        STABLE,
        RC,
        SNAPSHOT,
        NIGHTLY,
        INCREMENTAL,
        FULL
    };
    private String mUiName;
    private String mFileName;
    private Type mType;
    private int mApiLevel;
    private long mBuildDate;
    private String mDownloadUrl;
    //private String mChangelogUrl;//guo we don't need this
    private String mMd5Sum;
    private String mIncremental;

    private Boolean mIsNewerThanInstalled;

    private UpdateInfo() {
        // Use the builder
    }

    private UpdateInfo(Parcel in) {
        readFromParcel(in);
    }

	/*
 	//guo: we don't need this
    public File getChangeLogFile(Context context) {
        return new File(context.getCacheDir(), mFileName + CHANGELOG_EXTENSION);
    }
	*/

    /**
     * Get API level
     */
	//guo: api_level
    public int getApiLevel() {
        return mApiLevel;
    }

    /**
     * Get name for UI display
     */
    public String getName() {
        return mUiName;
    }

    /**
     * Get file name
     */
	//guo: filename
    public String getFileName() {
        return mFileName;
    }

    /**
     * Set file name
     */
    public void setFileName(String fileName) {
        mFileName = fileName;
    }

    /**
     * Set update type
     */
    public void setType(Type updateType) {
        mType = updateType;
    }

    /**
     * Get build type
     */
	//guo: channel
    public Type getType() {
        return mType;
    }

   /**
     * Get MD5
     */
	//guo: md5sum
    public String getMD5Sum() {
        return mMd5Sum;
    }

    /**
     * Get build date
     */
    public long getDate() {
        return mBuildDate;
    }

    /**
     * Get download location
     */
	//guo: url
    public String getDownloadUrl() {
        return mDownloadUrl;
    }

    /**
     * Get changelog location
     */
	//guo: we don't need this
	/*
    public String getChangelogUrl() {
        return mChangelogUrl;
    }
	*/

    /**
     * Get incremental version
     */
	// guo: incremental
    public String getIncremental() {
        return mIncremental;
    }

    /**
     * Whether or not this is an incremental update
     */
    public boolean isIncremental() {
        Matcher matcher = sIncrementalPattern.matcher(getFileName());
        if (matcher.find() && matcher.groupCount() == 2) {
            return true;
        } else {
            return false;
        }
    }

    //guo: compare the apiLevel or the buildDate
    public boolean isNewerThanInstalled() {
        if (mIsNewerThanInstalled != null) {
            return mIsNewerThanInstalled;
        }

        int installedApiLevel = Utils.getInstalledApiLevel();
        // Log.d(TAG, "installedApiLevel=" + installedApiLevel + ", mApiLevel=" + mApiLevel);
        if (installedApiLevel != mApiLevel && mApiLevel > 0) {
            mIsNewerThanInstalled = mApiLevel > installedApiLevel;
        } else {
            // API levels match, so compare build dates.
            // Log.d(TAG, "mBuildDate=" + mBuildDate + ", Utils.getInstalledBuildDate()=" + Utils.getInstalledBuildDate());
            mIsNewerThanInstalled = mBuildDate > Utils.getInstalledBuildDate();
        }

        return mIsNewerThanInstalled;
    }

	//guo: ??? WE need this??? used only by DownloadNotifier.java
    public static String extractUiName(String fileName) {
        String deviceType = Utils.getDeviceType();
        String uiName = fileName.replaceAll("\\.zip$", "");
        return uiName.replaceAll("-" + deviceType + "-?", "");
    }

    @Override
    public String toString() {
		//guo modify
        return "UpdateInfo: " + mFileName +
			   "\nmUiName: " + mUiName +
			   "\nmType: " + mType.toString() +
			   "\nApiLevel: " + mApiLevel +
			   "\nBuoldDate: " + mBuildDate +
			   "\nDownloadUrl: " + mDownloadUrl +
			   "\nMd5Sum: " + mMd5Sum +
			   "\nIncremental: " + mIncremental;
    }

    @Override
    public boolean equals(Object o) {
        if (o == this) {
            return true;
        }

        if (!(o instanceof UpdateInfo)) {
            return false;
        }

        UpdateInfo ui = (UpdateInfo) o;
        return TextUtils.equals(mFileName, ui.mFileName)
                && mType.equals(ui.mType)
                && mBuildDate == ui.mBuildDate
                && TextUtils.equals(mDownloadUrl, ui.mDownloadUrl)
                && TextUtils.equals(mMd5Sum, ui.mMd5Sum)
                && TextUtils.equals(mIncremental, ui.mIncremental);
    }

    public static final Parcelable.Creator<UpdateInfo> CREATOR = new Parcelable.Creator<UpdateInfo>() {
        public UpdateInfo createFromParcel(Parcel in) {
            return new UpdateInfo(in);
        }

        public UpdateInfo[] newArray(int size) {
            return new UpdateInfo[size];
        }
    };

    @Override
    public int describeContents() {
        return 0;
    }

    @Override
    public void writeToParcel(Parcel out, int flags) {
        out.writeString(mUiName);
        out.writeString(mFileName);
        out.writeString(mType.toString());
        out.writeInt(mApiLevel);
        out.writeLong(mBuildDate);
        out.writeString(mDownloadUrl);
        out.writeString(mMd5Sum);
        out.writeString(mIncremental);
    }

    private void readFromParcel(Parcel in) {
        mUiName = in.readString();
        mFileName = in.readString();
        mType = Enum.valueOf(Type.class, in.readString());
        mApiLevel = in.readInt();
        mBuildDate = in.readLong();
        mDownloadUrl = in.readString();
        mMd5Sum = in.readString();
        mIncremental = in.readString();
    }
	
	//guo: utility class for shallow clone UpdateInfo instance data from the saved state file: /cache/hbtupdater.state
    public static class Builder {
        private String mUiName;
        private String mFileName;
        private Type mType = Type.UNKNOWN;
        private int mApiLevel;
        private long mBuildDate;
        private String mDownloadUrl;
        //private String mChangelogUrl; //guo: we don't need this
        private String mMd5Sum;
        private String mIncremental;


        public Builder setName(String uiName) {
            mUiName = uiName;
            return this;
        }

        public Builder setFileName(String fileName) {
            initializeName(fileName);
            return this;
        }

        public Builder setType(String typeString) {
            Type type;
            if (TextUtils.equals(typeString, "stable")) {
                type = UpdateInfo.Type.STABLE;
            } else if (TextUtils.equals(typeString, "RC")) {
                type = UpdateInfo.Type.RC;
            } else if (TextUtils.equals(typeString, "snapshot")) {
                type = UpdateInfo.Type.SNAPSHOT;
            } else if (TextUtils.equals(typeString, "nightly")) {
                type = UpdateInfo.Type.NIGHTLY;
            } else {
                type = UpdateInfo.Type.UNKNOWN;
            }
            mType = type;
            return this;
        }

        public Builder setType(Type type) {
            mType = type;
            return this;
        }

        public Builder setApiLevel(int apiLevel) {
            mApiLevel = apiLevel;
            return this;
        }

        public Builder setBuildDate(long buildDate) {
            mBuildDate = buildDate;
            return this;
        }

        public Builder setDownloadUrl(String downloadUrl) {
            mDownloadUrl = downloadUrl;
            return this;
        }

		//guo: we don't need this
		/*
        public Builder setChangelogUrl(String changelogUrl) {
            mChangelogUrl = changelogUrl;
            return this;
        }
		*/

        public Builder setMD5Sum(String md5Sum) {
            mMd5Sum = md5Sum;
            return this;
        }

        public Builder setIncremental(String incremental) {
            mIncremental = incremental;
            return this;
        }

		//guo:shallow clone
        public UpdateInfo build() {
            UpdateInfo info = new UpdateInfo();
            info.mUiName = mUiName;
            info.mFileName = mFileName;
            info.mType = mType;
            info.mApiLevel = mApiLevel;
            info.mBuildDate = mBuildDate;
            info.mDownloadUrl = mDownloadUrl;
            //info.mChangelogUrl = mChangelogUrl;//guo: we don't need this
            info.mMd5Sum = mMd5Sum;
            info.mIncremental = mIncremental;
            return info;
        }


        private void initializeName(String fileName) {
            mFileName = fileName;
            if (!TextUtils.isEmpty(fileName)) {
                mUiName = extractUiName(fileName);
            } else {
                mUiName = null;
            }
        }
    }
}

//guo: this json file is parsed in UpdateCheckService.java's parseUpdateJSONObject()
/* changelog.json example
 *https://github.com/sultanxda/CMUpdater-API/blob/stable/cm-13.0/onyx/api.json
{
 "id": null,
 "result": [
    {
    "url": "http://forum.xda-developers.com/devdb/project/dl/?id=18753&task=get",
    "timestamp": "1465101634",
    "api_level": "23",
    "md5sum": "dd7f93c4a7da32f339fd41c15e5ea01a",
    "filename": "cm-13.0-ZNH2K-20160605-STABLE-Sultan-onyx.zip",
    "channel": "NIGHTLY",
    "changes": "",
    "incremental": ""
    },
  ],
 "error": null
}
*/
