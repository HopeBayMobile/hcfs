Please view this file on the android-dev branch, on stable branches it's out of date.

Known Issues / Limitations / Features to be Implemented
=====
  1. Encryption is not enabled now. Some crash issue occurred in the previous build when encryption is turned on (during decryption), and was not yet retested in this build.
  2. If cache is full during recording vedio, user need to wait cache been flush to storage after recording finished.
  3. Delete an app then reinstall it again (without running some other app with external storage access) might cause wrong permission when reading from external storage for this app.
  4. For uses in demo kit environment, backend setting via APP does not work (no connection to management cluster). Please push backend settings to phone directly.
  5. When installing an app, pinned space must contain enough space for the app as the installation process will first download the content to the pinned space.
  6. Data upload to the device via USB might fail if the amount of data to upload on the device plus the data to be uploaded exceeds cache size, and the network speed is slow.
  7. (A temp fix for crash issue) Files in /data/app are pinned now. An "unpin" action will not unpin files in the app package folder under /data/app.
  8. If backend (ArkFlexU) is not operating normally, there is a small chance of meta corruption. This issue is still under investigation.

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