Please view this file on the android-dev branch, on stable branches it's out of date.

## Known Issues / Limitations / Features to be Implemented
 1. Encryption is not enabled now. Some crash issue occurred in the previous build when encryption is turned on (during decryption), and was not yet retested in this build.
 2. If cache is full during recording video, user need to wait cache been flush to storage after recording finished.
 3. When installing an app, pinned space must contain enough space for the app as the installation process will first download the content to the pinned space.
 4. Data upload to the device via USB might fail if the amount of data to upload on the device plus the data to be uploaded exceeds cache size, and the network speed is slow.
 5. (A temp fix for crash issue) Files in /data/app are pinned now. An "unpin" action will not unpin files in the app package folder under /data/app.

v 2.2.2.1262
=====

## New Features
- [HCFS] feature/cache_xattr_value_for_exteral (!469)
- [HCFS] Feature/reserved meta size (!466)
- [HCFS] Feature/sync all data (!453)
- [HCFS] Feature/connect_swift_by_token (!463)
- [HCFS] Feature/event notification (!452)
- [HCFS] Feature/xattr_size
  1. Value block size is reduced to 256 bytes.
  2. # of keys in a page is 4.
  3. # of hash bucket is reduced to 8. (!455)
- [HCFS] Feature/meta version
  - Add magic number and version number in metafiles
  - Replace all internal `struct stat` with `HCFS_STAT` (!468)
- [Tera-App] Feature/factory reset on the phone and clear all data on the cloud ([!55](gateway-2-0/android-management-app!55))
- [Tera-App] Feature/wipe a locked device ([!58](gateway-2-0/android-management-app!58))
- [Tera-App] Feature/send a message to the locked device ([!58](gateway-2-0/android-management-app!58))
- [Tera-App] Feature/events notified from HCFS ([!58](gateway-2-0/android-management-app!58))
- [Tera-App] Transfer device. ([!58] (gateway-2-0/android-management-app!58))
- [Tera-App] Improve system security by returning ArkFlex U token and url instead of the user password. ([!61] (gateway-2-0/android-management-app!61))
- [Tera-App] Get new ArkFlexU access token when the older one is expired. ([!61] (gateway-2-0/android-management-app!61))
- [Tera-Launcher] Update Launcher icon depends on app status. ([!6](gateway-2.0/tera-launcher!6))
- [Tera-Launcher] Pop toast while pin/unpin complete ([!7](gateway-2.0/tera-launcher!7))
- [Tera-Launcher] Show progress dialog while pin/unpin in progress ([!8](gateway-2.0/tera-launcher!8))
- [Nexus-5x] Feature/factory reset on the phone and clear all data on the cloud ([!9](gateway-2-0/nexus-5x!9))
- [Nexus-5x] Feature/download oldest package algorithm and setup downloaded functionality by wifi only ([!19](gateway-2-0/nexus-5x!19))
- [Nexus-5x] Feature/factory reset_local only with sync_all_data ([!20](gateway-2-0/nexus-5x!20))
- [Nexus-5x] remove some built-in apps (clock, calculator and camera) ([!15](gateway-2-0/nexus-5x!15))
- [Nexus-5x] ADB on by default ([!16](gateway-2-0/nexus-5x!16))

## Fixes
- [HCFS] Bugfix/listxattr external selinux(!476)
- [HCFS] Add lock to HCFSAPI thread pool and read swift user in config(!475)
- [HCFS] Need to count # of blocks for fallocate op (!472)
- [HCFS] Fixed an issue that FSmgr db will not be backed-up if there is no network connection initially (!467)
- [HCFS] Fix signed / unsigned number checking error (!462)
- [HCFS] Reducing number of sys calls by using PREAD/PWRITE and eliminating access checks (!451)
- [HCFS] Error handling when reclaiming inode (!459)
- [HCFS] Added statistics to track maximum inode number (!458)
- [HCFS] bug fix #12224 
- [HCFS] Fix compile error(!474)
- [Tera-App] When pin folder failed, the pinned files in this folder became unpinned    Hide pin icon of folder[!66](gateway-2-0-android-management-app!66)
- [Tera-App] Replace the pin/unpin failed message with revised version[!65](gateway-2-0-android-management-app!65)
- [Tera-App] Code refactoring/mgmt auth ([!54](gateway-2-0/android-management-app!54))
- [Tera-App] Fix Bug #11879 ([!56](gateway-2-0/android-management-app!56))
- [Tera-App] Cannot unpin system app when pinned failed ([!57](gateway-2-0/android-management-app!57))
- [Tera-App] bug fix #12365
- [Tera-App] bug fix #12316
- [Tera-App] bug fix #12155
- [Tera-App] feature request #11779
- [Nexus-5x] fix Bug #12329: phone can not be located position by network ([!14](gateway-2-0/nexus-5x!14))
- [Nexus-5x] bug fix #11604 ([!17](gateway-2-0/nexus-5x!17))
- [Nexus-5x] bug fix #12350
- [Nexus-5x] add META_SPACE_LIMIT to hcfs.conf ([!18](gateway-2-0/nexus-5x!18))
- [Nexus-5x] update sepolicy ([!10](gateway-2-0/nexus-5x!10))
- [Nexus-5x] Adjust auto shutdown threshold to power level 3 ([!13](gateway-2-0/nexus-5x!13))
- [Nexus-5x] move mgmt app to priv-app ([!12](gateway-2-0/nexus-5x!12))
- [Nexus-5x] disable add user/guest button ([!11](gateway-2-0/nexus-5x!11))

## CI / Refactoring
- [Tera-Launcher] Push tag to launcher repo with current version number (!471)
- [HCFS] Ci/update flash script (!430)


v 2.2.2.1128
=====

## New Features
- [HCFS] Feature/log rotation compression (!437)
- [HCFS] Added shutdown info to HCFS (!446)

## Fixes
- [HCFS] If battery level <= 3% at bootup, shutdown immediately (Hotfix/check battery at bootup !448)
- [HCFS] Rearrange HCFS shutdown sequence so that all terminate steps (other than API shutdown) will be conducted before replying a shutdown request from API. (Hotfix/rearrange shutdown sequence !450)
- [HCFS] bugfix/cannot_correctly_pin_all_files (!436)
- [HCFS] Add some error handling when pin (about pin scheduler) (!449)
- [HCFS] when file is syncing, do not expire it in meta cache (!447)
- [HCFS] Reduced the number of sequence number updates in write and truncate.  Now will only increase the seq number after being synced to cloud or if loaded to meta cache (!438)
- [Nexus-5x] Adjust auto shutdown threshold to power level 3 ([!13](gateway-2-0/nexus-5x!13))
- [Android-System UI] Disable add user/guest button

## Tests 
- [HCFS] Test backend monitor  US-870  US-000 (!442)

v 2.2.2.1074
=====

## Fixes
- [HCFS] hotfix/hang_when_writing (!439)

v 2.2.1.1060
=====

## Fixes
 - [Nexus-5x] fix/user_build_adb_failed ([!7](gateway-2-0/nexus-5x!7))
 - [Nexus-5x] Let OTA updater can successfully trigger upgrade from notification bar and can launch OTA updater from notification bar when downloading ([!8](gateway-2-0/nexus-5x!8))
    - #11987 OTA will fail if user trigger OTA from notification bar
    - #11924 在系統更新頁面時, 拉下狀態訊息列, 再點選狀態訊息, 此時會出現錯誤訊息 "HopeBayTech Updater" exception happened

## CI / Refactoring
 - [Nexus-5x] Fix CI error on build ota update package (!434)
 - [Nexus-5x] Fix CI error when two image jobs try to push tags with same name (!435)

v 2.2.1.1053
=====

## New Features
 - [HCFS] Add meta parser python library (AKA pyhcfs) for Tera Client (!419)
    - The pyhcfs is a python library for Tera Client(web interface) to read user file tree from raw meta files in backend, which does not affect user device.

## Fixes
 - [HCFS] #11394 Handle /data/data/\<pkg\>/lib -> /data/app/lib symlink (!431)
 - [HCFS] bugfix/enable_change_config (!432)
 - [Nexus-5x] generate files for normal boot to upgrade recovery.img ([!4](gateway-2-0/nexus-5x!4))
 - [Nexus-5x] modify ota server URL to `ota.tera.mobi:50000` ([!5](gateway-2-0/nexus-5x!5))
 - [Tera-App] #11821 當進入 Device storage/DCIM/Camera/ 後, 縮圖圖示會閃爍. ([!50](gateway-2-0/android-management-app!50))
 - [Tera-App] #11833 Pin fail 後, pin/unpin file 進度會一直轉圈圈 ([!51](gateway-2-0/android-management-app!51))
 - [Tera-App] #11864 Switch account後, Tera連線失敗 ([!52](gateway-2-0/android-management-app!52))
 - [Launcher] #11741 Launcher pin/unpin buttons disappear while executing Tera mgmt app via adb command([!5](gateway-2-0/tera-launcher!5))

## CI / Refactoring
 - [HCFS] Migrate 5x device patches into new repo "nexus-5x" to simplify system integration process(!420)(!429)

v 2.2.1.1025
=====

## New Features
 - [Android] remove user settings (!424)
 - [Tera-App] Add a feedback button onto settings page ([!45](gateway-2-0/android-management-app!45))
 - [Tera-App] Add required information for ui automation ([@63f5b04a](gateway-2-0/android-management-app@63f5b04a))
 - [Android] Feature/HBTUpdater (!426)
	1. Integrate OTA Updater APP (/system/priv-app/HBTUpdater.apk) into nexus-5X source code. User can upgrade system from OTA now.
	2. Download the oldest OTA update only (User may need to upgrade system multiple times)
	3. Temporarily disable wifi download only (update through Wi-Fi+Cellular )
	4. Temporarily disable auto download OTA update

## Fixes
 - [HCFS] Fix #11476 Device should not show "Transmission slow" for return code 401. (!427)
 - [HCFS] hotfix/break_upload_block_loop_on_error (!421)
 - [HCFS] Adding lower limit to truncate and write offset, and unittest (!425)
 - [Android] Move tera api library to android prebuilt (!418)
 - [Tera-App] #11598 Tera exception happened after pressing "Start switching" button to switch account ([!46](gateway-2-0/android-management-app!46))
 - [Tera-App] #11100 List view 點擊 Pin/Unpin App 時會出現 pin status 切回點擊前狀態 ([!47](gateway-2-0/android-management-app!47))
 - [Tera-App] #11636 There is no Tera version information and IMEI 2 should not list in About page. ([!48](gateway-2-0/android-management-app!48))
 - [Tera-App] #11632 APP/FILE 畫面下, 切換到Display by file時資料夾名稱下半部被裁切掉了  ([@7d50f700](gateway-2-0/android-management-app@7d50f700))
 - [Tera-App] Fix login permission request dialog issue ([!39](gateway-2-0/android-management-app!39))
 - [Tera-App] Fix inconsistent state when mgmt app (launcher) pin or unpin ([!40](gateway-2-0/android-management-app!40))
 - [Tera-App] Show message on login page when auto auth failed ([@e79552cf](gateway-2-0/android-management-app@e79552cf))
 - [Tera-App] Unpin/pin related files/apps when pin/unpin failed ([@2aa28b4d](gateway-2-0/android-management-app@2aa28b4d))

## CI / Refactoring
  - No changes

v 2.2.1.0962
=====
## New Features
- (!396) System supports new pin type - “high-priority-pin”. (also gateway-2-0/android-management-app!35)
    - Some reserved space will be used for high-priority-pin files/dirs if there are no cache space can be reused.
    - Integrate high priority pin to Tera app
- (!410) Replace default with Tera launcher (pin/unpin from launcher) (also !415)
- (!413) Do not sync to backend if only atime is changed, or if block status changed due to cache download or paging out
    - Change the code so that the meta file won't be synced to backend if
        1. Only atime is changed, or
        2. If block status changed due to cache download or paging out.
- (US-941) As a user, I want to do factory reset on the phone but still keep all data on the cloud(and could be restored later).
- (US-946) As a user, I want to do pin / unpin on launcher icons directly.
- (US-948) As a user, I want to see TeraFonn related cloud storage information and launch TeraFonn APP from the Setting app.
- (US-1257) As a user, I want to be notified to release pin space when remaining pin space is not enough
- (US-713) As a user, I want to change the google account associated with TeraFonn on the phone.
- (US-935) As a user, I want to activate Tera via invitation (activation) code from within the phone.
- (US-946) As a user, I want to do pin / unpin on launcher icons directly.

## Fixes
 - (!414) Change hcfs default config
 - (!411) Workaround for 5x : unzip images and flash one by one
    This is a workaround for booting error if install image with `fastboot update  *-img-*.zip`
 - (!412) Fix: Failed to mkdir /storage/emulated/0
 - (!406) Hotfix/cache blocked by pin
    1. Pin file will trigger cache replacement
    2. Read from cloud will trigger cache replacement 
    3. Enhance rebuild super block
    4. CACHE_USAGE_NUM_ENTRIES 65536 -> 128
 - (Teara App) Fix google silent login at system bootup
 - (Teara App) Fix google login issue
 - (Teara App) Fix open multi Tera app
 - (Teara App) Start sync cloud when user activates Tera
 - (Teara App) Remove ongoing notification when authentication failed
 - (Setting App) Fix crash when open “Storage & USB” from settings app

## CI / Refactoring
 - (!405) Release ota package files with CI. It's for later ota procedure.

v 2.2.1.0908
=====
## New Features
  - Add Tera feature to Settings app (!400)
    1. Support launch Tera management app by Settings app
    2. Support show cloud storage usage in Settings app
  - Pause cache management (!387). 
    - Now cache management will not actively trying to find cache blocks to page out if none is available.
  - Atomic upload android (!204)
    1. atomic upload(robust data integrity during file changing)
    2. able to resume file sync after disconnection

## Fixes
  - Fix status inconsistency in read/write (!394). 
    - Read / write operations opening a data block will now reload block status if needed.
  - Fix readdir issue (!374). 
    - Fixed the issue on deleting a directory of many files under CTS.

## CI / Refactoring
  - Fix image build failure (!399)
  - Fix unittest no init sem issue (!398)
  - Auto build nexus 5x (!389)(!391)(!393)(!397)
  - Reviewing API (!388)
  - Update version num format (!375)


v 2.2.1.0675 (release2.2.1_base)
=====
## New Features
  - Power Consumption/[Backend Monitoring Design](https://docs.google.com/presentation/d/1n851y5R_x3Y07UehvKN3ac1wfdeZF1hBvpX94KwmDzk/edit) (!357)
    - Reconnection waiting time increase exponentially if backend is not available (5XX code, connection timed out).
    - HCFS will switch to offline mode and pause sync if received 5xx return code from server.
  - Performance/Enhance pkg lookup. (!363)
  - Performance/Skip xattr permission check if domain is SECURITY (!383)

## CI / Refactoring
  - Refactoring/normalize data type 3 (!358)
  - Refactoring/normalize data type 4 (!371)
  - Refactoring/normalize_data_type_5 (!376)

v 2.0.6.0617
=====
## New Features
  - Feature/more xfer statistics (!351)
  - Feature/selinux allow pull hcfs log (!294)
  - Feature/unpin dirty api (!356)
  - Feature/unpin dirty size (!327)
  - APP: Default display layout is changed to Grid layout
  - APP: Remove "Display by data type"
  - APP: Tap App icon to pop up detail information dialog both in ListView and GridView
  - APP: Build database even TeraFonn is not activated
  - APP: Add dialog to show local data ratio when user long-press on file/dir item
  - APP: Add pin/unpin action icon to item popup dialog
  - APP: Add IMEI and system version information to about page
  - APP: Display colorful/gray item icon
  - APP: Apply the value of (pin + unpin but dirty) to local space usage notification
  - APP: Apply hcfs connection status api on dashboard and ongoing notification

## Fixes
  - Add no pkg case to pkg lookup (!324)
  - Bugfix/bugs (!303)
  - Enable set xattr without value (!345)
  - Feature/update sepolicy for cts (!309)
  - Fix getxattr bug (!310)
  - Fix superblock errors (!343)
  - Fix to hang issue (!314)
  - Fix/cts failed and path error (!302)
  - Hotfix/change cache delta value (!313)
  - Hotfix/pathlookup lock deadlock (!339)
  - Release lock if needed (!291)
  - Reorder lock acquiring sequence (!330)
  - #9861    P2  When user login Terra Fonn with invalid account, the APP will not show any notification.  
  - #10590   P2  It should have a message when back end server fail to connect.
  - #10927   P4  Non-intuitive behavior when pinning fails due to no enough cache space
  - #11071   P3  Circular progress bar persists after pin fails
  - #11097   P2  Grid View 長按App Pin/UnPin 時, 會出現 App Pin Status 與長按後跳出的Pin/Cancel pin Button 不符
  - #11100   P2  List view 點擊 Pin/Unpin App 時會出現 pin status 切回點擊前狀態
  - #11138   P2  After pinning Video by data type then recording video, the video was not pinned after 1 hour. 
  - #11151   P3  Pin/Unpin 時如果選擇的項目超出畫面, 則Pin/Unpin後項目icon不會更新 
  - #11155   P2  TeraFonn app 啟用前, 透過Google帳號 restore app, 會造成 TeraFonn app 沒將 app 訊息寫入uid.db  
  - #11161   P3  "Data upload complete" notification function review
  - #11166   P3  Phone Data Ratio not precisely calculated 
  - #11172   P3  在Pin/UnPin 資料夾名稱有相同字母開頭且有規則的延長接續下去, 會發生相同名稱開頭的資料夾被一起Pin / UnPin
  - #11178   P3  All app related folders under Android folder should be pin/unpin when pin/unpin app.  

## CI / Refactoring
  - Update acer code to v5.0419 (!344)
  - Feature/normalize data type 2 (!246)
  - New CI flow - single job that runs all existed jobs (!289)
  - Print version number in log file (!315)
  - Update/change build directory for s58a (!328)
  - Upgrade is not needed outside of docker container (!286)
  - Wraning if using ndk-r11, auto init third_party/jansson (!350)
  - APP: Change targetSdkVersion to 23 and add READ_EXTERNAL_STORAGE runtime permission

v 2.0.5.0394
=====
## New Features
  1. Sdcard is enabled now, with extend to sdcard feature disabled.
  2. Quota is now supported. 

## Fixes
  1. 修正一個truncate權限導致app處理時會dump error的問題
  2. SELinux 權限修正
  3. 增加linux capabilities的檢查
  4. Redmine issues fixed: 10041, 10930, 10933, 11037, 11043, 11046, 11052 

## Known Issues / Limitations / Features to be Implemented
  1. Encryption is not enabled now. Some crash issue occurred in the previous build when encryption is turned on (during decryption), and was not yet retested in this build.
  2. 錄影時如果cache已滿, 停止錄影後會需要耐心等候錄影的cache flush to storage.
  3. Delete an app then reinstall it again (without running some other app with external storage access) might cause wrong permission when reading from external storage for this app.
  4. For uses in demo kit environment, backend setting via APP does not work (no connection to management cluster). Please push backend settings to phone directly.
  5. When installing an app, pinned space must contain enough space for the app as the installation process will first download the content to the pinned space.
  6. Data upload to the device via USB might fail if the amount of data to upload on the device plus the data to be uploaded exceeds cache size, and the network speed is slow.
  7. (A temp fix for crash issue) Files in /data/app are pinned now. An "unpin" action will not unpin files in the app package folder under /data/app.
  8. If backend (ArkFlexU) is not operating normally, there is a small chance of meta corruption. This issue is still under investigation.

v 2.0.4.0344
=====
## New Features
  1. SELinux is now enforced. User build should work now.

## Fixes
  1. Fixed crashes due to system app data flushed from cache.

## Known Issues / Limitations / Features to be Implemented
  1. Android SELinux security set to permissive for now until security policies are fixed.
  2. Backend quota (ArkflexU) not yet supported.
  3. Encryption is not enabled now. Some crash issue occurred in the previous build when encryption is turned on (during decryption), and was not yet retested in this build.
  4. 錄影時如果cache已滿, 停止錄影後會需要耐心等候錄影的cache flush to storage.
  5. Delete an app then reinstall it again (without running some other app with external storage access) might cause wrong permission when reading from external storage for this app.
  6. For uses in demo kit environment, backend setting via APP does not work (no connection to management cluster). Please push backend settings to phone directly.
  7. When installing an app, pinned space must contain enough space for the app as the installation process will first download the content to the pinned space.
  8. Data upload to the device via USB might fail if the amount of data to upload on the device plus the data to be uploaded exceeds cache size, and the network speed is slow.
  9. (A temp fix for crash issue) Files in /data/app are pinned now. An "unpin" action will not unpin files in the app package folder under /data/app.

v 2.0.4.0311
=====
## New Features

## Fixes
  1. Fixed crashes due to system app data flushed from cache.

## Known Issues / Limitations / Features to be Implemented
  1. Android SELinux security set to permissive for now until security policies are fixed.
  2. Backend quota (ArkflexU) not yet supported.
  3. Encryption is not enabled now. Some crash issue occurred in the previous build when encryption is turned on (during decryption), and was not yet retested in this build.
  4. 錄影時如果cache已滿, 停止錄影後會需要耐心等候錄影的cache flush to storage.
  5. Delete an app then reinstall it again (without running some other app with external storage access) might cause wrong permission when reading from external storage for this app.
  6. For uses in demo kit environment, backend setting via APP does not work (no connection to management cluster). Please push backend settings to phone directly.
  7. Authentication using Google auth might fail.
  8. When installing an app, pinned space must contain enough space for the app as the installation process will first download the content to the pinned space.
  9. Data upload to the device via USB might fail if the amount of data to upload on the device plus the data to be uploaded exceeds cache size, and the network speed is slow.
  10. An Installed app might disappear from launcher after device reboot under the following conditions: (a) It is unpinned, and (b) Content of the app is flushed from the device due to insufficient cache space.

v 2.0.4.0284
=====
## New Features

## Fixes
  1. System app data on /data/data is pinned by default and cannot be changed.
  2. Image is now released from CI build, and version number is changed to fit CI build number

## Known Issues / Limitations / Features to be Implemented
  1. Android SELinux security set to permissive for now until security policies are fixed.
  2. Backend quota (ArkflexU) not yet supported.
  3. Encryption is not enabled now. Some crash issue occurred in the previous build when encryption is turned on (during decryption), and was not yet retested in this build.
  4. 錄影時如果cache已滿, 停止錄影後會需要耐心等候錄影的cache flush to storage.
  5. Delete an app then reinstall it again (without running some other app with external storage access) might cause wrong permission when reading from external storage for this app.
  6. For uses in demo kit environment, backend setting via APP does not work (no connection to management cluster). Please push backend settings to phone directly.
  7. System app data on external storage is currently not pinned by default. This will be fixed.
       Workaround for known issue 7: Current workaround is to pin /Android folder from TeraFonn app when the system is first installed. If data for a certain app need to be completely unpinned, please first "pin" this app then "unpin" the app.

v 2.0.4.0012
=====
## New Features

## Fixes
  1. Fixed libHCFS_api.so

## Known Issues / Limitations / Features to be Implemented
  1. Android SELinux security set to permissive for now until security policies are fixed.
  2. Backend quota (ArkflexU) not yet supported.
  3. Encryption is not enabled now. Some crash issue occurred in the previous build when encryption is turned on (during decryption), and was not yet retested in this build.
  4. 錄影時如果cache已滿, 停止錄影後會需要耐心等候錄影的cache flush to storage.
  5. Delete an app then reinstall it again (without running some other app with external storage access) might cause wrong permission when reading from external storage for this app.
  6. For uses in demo kit environment, backend setting via APP does not work (no connection to management cluster). Please push backend settings to phone directly.

v 2.0.4.0011
=====
## New Features
  1. HCFS binaries and libraries changed to 64-bit builds.
  2. Configuration is now encrypted.
  3. Pin action is now multi-threaded

## Fixes
  1. 2GB limitation lifted
  2. Google Play is working now
  3. Default pin behavior is now "unpin"
  4. Fixed HCFS unmount hang issue.

## Known Issues / Limitations / Features to be Implemented
  1. Android SELinux security set to permissive for now until security policies are fixed.
  2. Backend quota (ArkflexU) not yet supported.
  3. Encryption is not enabled now. Some crash issue occurred in the previous build when encryption is turned on (during decryption), and was not yet retested in this build.
  4. 錄影時如果cache已滿, 停止錄影後會需要耐心等候錄影的cache flush to storage.
  5. Delete an app then reinstall it again (without running some other app with external storage access) might cause wrong permission when reading from external storage for this app.
  6. For uses in demo kit environment, backend setting via APP does not work (no connection to management cluster). Please push backend settings to phone directly.

v 2.0.3.0010
=====
## New Features

## Fixes
  1. Changed unnecessary error / info logs to debug logs
  2. Reduced cache limit to reserve storage space for system usage

## Known Issues / Limitations / Features to be Implemented
  1. Android SELinux security set to permissive for now until security policies are fixed.
  2. Backend quota (ArkflexU) not yet supported.
  3. Encryption is not enabled now. Some crash issue occurred in the previous build when encryption is turned on (during decryption), and was not yet retested in this build.
  4. 錄影時如果cache已滿, 停止錄影後會需要耐心等候錄影的cache flush to storage.
  5. Google play not working
  6. Delete an app then reinstall it again (without running some other app with external storage access) might cause wrong permission when reading from external storage for this app.
  7. HCFS unmount is removed from phone shutdown process due to hang issue. Phone shutdown might cause data inconsistency if data processing is in progress.
  8. For uses in demo kit environment, backend setting via APP does not work (no connection to management cluster). Please push backend settings to phone directly.

v 2.0.3.0009
=====
## New Features

## Fixes
  1. Ported camera app from old framework to new one
  2. API server now supports multi-threading

## Known Issues / Limitations / Features to be Implemented
  1. Android SELinux security set to permissive for now until security policies are fixed.
  2. Backend quota (ArkflexU) not yet supported.
  3. Encryption is not enabled now. Some crash issue occurred in the previous build when encryption is turned on (during decryption), and was not yet retested in this build.
  4. 錄影時如果cache已滿, 停止錄影後會需要耐心等候錄影的cache flush to storage.
  5. Google play not working
  6. Delete an app then reinstall it again (without running some other app with external storage access) might cause wrong permission when reading from external storage for this app.
  7. HCFS unmount is removed from phone shutdown process due to hang issue. Phone shutdown might cause data inconsistency if data processing is in progress.
  8. For uses in demo kit environment, backend setting via APP does not work (no connection to management cluster). Please push backend settings to phone directly.

v 2.0.3.0008
=====
## New Features
  Changes
  1. New framework from acer
  2. Fuse lib upgraded to 2.9.4
   

## Fixes
  1. APP enhancement

## Known Issues / Limitations / Features to be Implemented
  1. Android SELinux security set to permissive for now until security policies are fixed.
  2. Backend quota (ArkflexU) not yet supported.
  3. Encryption is not enabled now. Some crash issue occurred in the previous build when encryption is turned on (during decryption), and was not yet retested in this build.
  4. 錄影時如果cache已滿, 停止錄影後會需要耐心等候錄影的cache flush to storage.
  5. Google play not working
  6. Delete an app then reinstall it again (without running some other app with external storage access) might cause wrong permission when reading from external storage for this app.
  7. HCFS unmount is removed from phone shutdown process due to hang issue. Phone shutdown might cause data inconsistency if data processing is in progress.
  8. For uses in demo kit environment, backend setting via APP does not work (no connection to management cluster). Please push backend settings to phone directly.

v 2.0.3.0007
=====
## New Features

## Fixes
  1. Fixed management app crash issue
  2. Fixed curl connection issue with HTTPS

## Known Issues / Limitations / Features to be Implemented
  1. Android SELinux security set to permissive for now until security policies are fixed.
  2. Backend quota (ArkflexU) not yet supported.
  3. Encryption is not enabled now. Some crash issue occurred in the previous build when encryption is turned on (during decryption), and was not yet retested in this build.
  4. 錄影時如果cache已滿, 停止錄影後會需要耐心等候錄影的cache flush to storage.
  5. Google play not working
  6. Delete an app then reinstall it again (without running some other app with external storage access) might cause wrong permission when reading from external storage for this app.
  7. HCFS unmount is removed from phone shutdown process due to hang issue. Phone shutdown might cause data inconsistency if data processing is in progress.
  8. For uses in demo kit environment, backend setting via APP does not work (no connection to management cluster). Please push backend settings to phone directly.

v 2.0.3.0006
=====
## New Features
  1. Supports Acer Jade 2 (s58a), not for Acer Z630
  2. Android 6.0

## Fixes
  1. Fixed parent lookup error in external storage if inode is deleted but not closed.
  2. Fixed setxattr read issue when HCFS is mounted
  3. Fixed app execution performance issue by caching package to uid lookup

## Known Issues / Limitations / Features to be Implemented
  1. Android SELinux security set to permissive for now until security policies are fixed.
  2. Backend quota (ArkflexU) not yet supported.
  3. Encryption is not enabled now. Some crash issue occurred in the previous build when encryption is turned on (during decryption), and was not yet retested in this build.
  4. 錄影時如果cache已滿, 停止錄影後會需要耐心等候錄影的cache flush to storage.
  5. Management App 尚未完成整合. Please edit configuration directly to setup backend.
  6. Google play not working
  7. Delete an app then reinstall it again (without running some other app with external storage access) might cause wrong permission when reading from external storage for this app.
  8. HCFS unmount is removed from phone shutdown process due to hang issue. Phone shutdown might cause data inconsistency if data processing is in progress.

v 2.0.2.0005
=====
## New Features
  -

## Fixes
  1. API server code on git now includes fix to writing configuration issue.
  2. Before this version, system may crash when receiving SIGPIPE signal due to broken SSL connections or API call connections. This was resolved by logging SIGPIPE only and not raising it.
  3. Enabled extended attributes (was turned off in Android builds).
  4. Fixes to how to build shared libs

## Known Issues / Limitations / Features to be Implemented
  1. Android SELinux security disabled for now until security policies are fixed.
  2. Backend quota (ArkflexU) not yet supported.
  3. Encryption is not enabled now. Some crash issue occurred in the previous build when encryption is turned on (during decryption), and was not yet retested in this build.
  4. 錄影時如果cache已滿, 停止錄影後會需要耐心等候錄影的cache flush to storage.
  5. 首頁上會多出兩個"新增共用空間" 圖示. 目前無功用.
  6. 設定中"WIFI連線時上傳資料" 應該是 "只有WIFI連線時才上傳". Turn off時會是如果有網路連線就上傳.
  7. Installed apps might disappear from launcher after system reboot. This will be fixed in Android 6.0 integration.
  8. 錄影到快16分鐘時系統會自動停止錄影, 此時有可能錄影app會crash且檔案不能撥放 (但是系統還是可以運作).
  9. 手機進入休眠模式後重啟可能會無法正常運作(因為HCFS未被開啟). 請重新開機.

v 2.0.2.0004
=====
## New Features
  N/A

## Fixes
  1. 初始設定TeraFonn現在會依照所使用的帳號關聯到不同的ArkFlexU 帳號
  2. TeraFonn 開機後會檢查是否已創建過volumes, 如果有就不會嘗試新創volumes
  3. 本地儲存空間設定大小已調大. EU ~ 4GB, TW ~ 11GB 

## Known Issues / Limitations / Features to be Implemented
  1. Android SELinux security disabled for now until security policies are fixed.
  2. Backend quota (ArkflexU) not yet supported.
  3. Encryption is not enabled now. Some crash issue occurred in the previous build when encryption is turned on (during decryption), and was not yet retested in this build.
  4. 錄影時如果cache已滿, 停止錄影後會需要耐心等候錄影的cache flush to storage.
  5. 首頁上會多出兩個"系統" 圖示. 目前無功用.
  6. 設定中"WIFI連線時上傳資料" 應該是 "只有WIFI連線時才上傳". Turn off時會是如果有網路連線就上傳.
  7. 資料傳輸量不會每日重設.
  8. 系統使用上有不穩定的情形. 正在除錯中.
  Note: The API server included in the build is not built from the source tagged on git, but from a previous binary built by Yuxun (some problem occurred when using the binary built from the tagged source code). This will be tracked further.

v 2.0.2.0003
=====
## New Features
  1. User apps are now stored in HCFS
  2. Supports app pin / unpin
  3. FIFO / socket are supported.
  4. "只有WIFI連線才能上傳" implemented 

## Fixes
  1. 本地空間使用顯示已正常
  2. 修正一些UI上點選時看起來會有動作但是實際上為不可點選

## Known Issues / Limitations / Features to be Implemented
  1. Android SELinux security disabled for now until security policies are fixed.
  2. Backend quota (ArkflexU) not yet supported.
  3. For the 2.0.2.0003 image, the debug log of the device (under /mtklog) writes constantly to the storage space. This could fill up the pinned space quickly (if the directory is pinned), or compete for cache space / upload bandwidth (if unpinned). If /mtklog is pinned, the size won't grow if all pinned space is occupied.
  4. Encryption is not enabled now. Some crash issue occurred in the previous build when encryption is turned on (during decryption), and was not yet retested in this build.
  5. 錄影時如果cache已滿, 停止錄影後會需要耐心等候錄影的cache flush to storage.
  6. 首頁上會多出兩個"系統" 圖示. 目前無功用.
  7. 設定中"WIFI連線時上傳資料" 應該是 "只有WIFI連線時才上傳". Turn off時會是如果有網路連線就上傳.
  8. 資料傳輸量不會每日重設.
  Note: The API server included in the build is not built from the source tagged on git, but from a previous binary built by Yuxun (some problem occurred when using the binary built from the tagged source code). This will be tracked further.

v 2.0.1.0002
=====
## Fixes
  1. Fixed hang issues due to timeout when calling curl lib.
  2. Fixed some variable conversion issues.
  3. Reduced thread numbers.
  4. Fixed an issue re undeleted temp files if curl operations (upload, download, delete remote object, etc) failed.
  5. Fixed cache size in default configuration file for testing Acer machines. In the old setting, the size of the cache hard limit is larger than the available disk space for cache purpose.
  6. Enhanced network error handling.
  7. Some libraries are now included as shared libraries.
  8. Fixed upload policy
  Please find the complete comparison using https://gitlab.hopebaytech.com/gateway-2-0/hcfs/compare/terafonn2.0.1.0001...terafonn2.0.1.0002

## New Features
  -

## Known Issues / Limitations / Features to be Implemented
  1. Support App data storage space on cloud filesystem (internal storage space).
  2. App may not be able to access app-specific part of the external storage if it does not request for external storage permission (uid modeling of app-specific space on external storage not yet done).
  3. Android SELinux security disabled for now until security policies are fixed.
  4. Backend quota (ArkflexU) not yet supported.
  5. FIFO type filesystem object not yet supported in HCFS.
  6. Switch for sync to backend (turn sync on/off) not yet implemented.
  7. For the 2.0.1.0001 image, the debug log of the device (under /mtklog) writes constantly to the storage space. This could fill up the pinned space quickly (if the directory is pinned), or compete for cache space / upload bandwidth (if unpinned). If /mtklog is pinned, the size won't grow if all pinned space is occupied.
  8. Encryption is not enabled now. Some crash issue occurred in the previous build when encryption is turned on (during decryption), and was not yet retested in this build.
  9. 錄影時如果cache已滿, 停止錄影後會需要耐心等候錄影的cache flush to storage.

v 2.0.1.0001
=====
## New Features
  1. Cache-based cloud filesystem on Android phone (external storage space only now)
  2. Cloud storage setup via management app
  3. File / media pin / unpin via management app (no app pin / unpin yet)
  4. System monitoring via management app

## Known Issues / Limitations / Features to be Implemented
  1. Support App data storage space on cloud filesystem (internal storage space).
  2. App may not be able to access app-specific part of the external storage if it does not request for external storage permission (uid modeling of app-specific space on external storage not yet done).
  3. App / TeraFonn might hang when browsing image content on management app. Reboot phone will fix the situation.
  4. Android SELinux security disabled for now until security policies are fixed.
  5. Backend quota (ArkflexU) not yet supported.
  6. FIFO type filesystem object not yet supported in HCFS.
  7. Switch for sync to backend (turn sync on/off) not yet implemented.
  8. Error handling for network disconnection not included in this build