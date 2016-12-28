Please view this file on the android-dev branch, on stable branches it's out of date.

## Known Issues / Limitations
 1. Encryption is not enabled now. Some crash issue occurred in the previous build when encryption is turned on (during decryption), and was not yet re-tested in this build.
 2. If cache is full during recording video, user need to wait cache been flushed to storage after recording finished.
 3. When installing an app, pinned space must contain enough space for the app as the installation process will first download the content to the pinned space.
 4. Data upload to the device via USB might fail if the amount of data to upload on the device plus the data to be uploaded exceeds cache size, and the network speed is slow.
 5. (A temp fix for crash issue) Files in /data/app are pinned now. An "unpin" action will not unpin files in the app package folder under /data/app.
 6. If booster partition is damaged and cannot be repaired during restoration, the booster partition will be deleted with no notification.Tera app does not do any response to this situation.
 7. Settings might crash when sync all data.
 8. Database might be locked and let tera app crashed.
 9. If pin space < 100MB, Tera app does not deal with this situation, the booster will still be created.
 10. if data in booster space is > booster size, Tera app does not deal with this situation.
 11. When sync all data, the booster might not be unmounted successfully.
 12. After successfully sync all data, the booster will not re-mounted. And the icon will be gone from the Launcher after rebooting.
 13. If the system is shutdown or reboot when boosting/unboosting, the Tera app did not deal with this situation after booting complete, and the icon will be gone from Launcher also.
 14. the booster might not sync all data ok since we did not execute `sync` command before syncing the booster.
 15. [HCFS] In restoration stage1, if system reboot when restored smart cache(2.7GB) has been downloaded and moved into now active hcfs, stage1 will fail because of no more space.
       It should have kept restoring after rebooting in stage1.

v 2.2.4.2354
=====
## New Features
 - [Tera-App] feature/add_isboosting_api: Add isBoosting api  ([!179](gateway-2-0/android-management-app!179))

## Fixed
 - [HCFS] fix/#14171_wrong_usage_of_smart_cache_2.2.4: Fix/#14171 wrong usage of smart cache 2.2.4  ([!694](gateway-2-0/hcfs!694))
 - [Nexus-5X] fix/bug_14364: Fix/bug 14364 and add more TW chinese for HBTUpdater  ([!101](gateway-2-0/nexus-5x!101))
 - [Nexus-5X] fix/bug_14364: modify strings.xml for bug 14364  ([!103](gateway-2-0/nexus-5x!103))
 - [Nexus-5X] fix/bug_14397: bugfix#14397 ([!97](gateway-2-0/nexus-5x!97))
 - [Nexus-5X] fix/hbtupdater_i18n_new: add some hbtupdater i18n for TW Chinese  ([!98](gateway-2-0/nexus-5x!98))
 - [Tera-App] bugfix/#14464: Bugfix/#14464 Launch Tera app with intent made by Intent.makeRestartActivityTask(). If the Tera app has already launched, Tera app will be relaunched so thatonCreate() will be called instead of onNewIntent(). The switch fragment operations are processed in onCreate(), thus we need to call onCreate().  ([!183](gateway-2-0/android-management-app!183))
 - [Tera-Launcher] bugfix/#14261: The application info will be null while uninstall ([!25](gateway-2-0/tera-launcher!25))
 - [Tera-Launcher] bugfix/launcher_boost_handle: Bugfix/launcher boost handle  ([!26](gateway-2-0/tera-launcher!26))
 - [Tera-Service] bugfix/#14202: The intent will be null while restart serviceIf service crashed and then restarted, the intent will be null.It will cause null pointer exception  ([!7](TeraAndroid/android_packages_apps_TeraService!7))

## CI / Refactoring / Other
 - [Nexus-5X] revert-67e7b3fe: Revert "Merge branch 'update_terafonn_jar' into 'master'"This reverts commit 67e7b3feb11921bdb75cb5d8097f7666b21f7db8  ([!94](gateway-2-0/nexus-5x!94))
 - [Nexus-5X] revert-e3e8fdfe: Revert "Merge branch 'update_terafonn_jar-2.2.4' into 'release/2.2.4-bugfix'"This reverts merge request !91  ([!93](gateway-2-0/nexus-5x!93))
 - [Nexus-5X] update_terafonn_jar-2.2.4: Update terafonn jar for 2.2.4 branch  ([!91](gateway-2-0/nexus-5x!91))
 - [Nexus-5X] update_terafonn_jar-2.2.4: Update terafonnapi jar  ([!96](gateway-2-0/nexus-5x!96))
 - [Nexus-5X] update_terafonn_jar: Rename API Rename isBoosting to isBoostOrUnboostInProgress ([!89](gateway-2-0/nexus-5x!89))

### Project Branch / Tag in this build
 - [HCFS] release2.2.4_bugfix
 - [Tera-App] 2.2.4.0004
 - [Nexus-5X] 2.2.4.0004
 - [Tera-Launcher] 2.2.4.0004
 - [Tera-Service] 2.2.4.0004

v 2.2.4.2269
=====
## New Features
 - [HCFS] feature/update-nexus-install-gapps: Feature/update nexus install gapps. Update image and use sideload again.On windows, ADB is much stable within VMware player rather than Virtualbox.  ([!658](gateway-2-0/hcfs!658))
 - [Tera-Launcher] feature/smart_cache: Feature/smart cache  ([!19](gateway-2-0/tera-launcher!19))
 - [Tera-Service] feature/create_thumbnail_by_intent: Support create thumbnail while receive intent The TeraService will create thumbnail while receivecom.teraservice.create.thumbnail intent  ([!6](TeraAndroid/android_packages_apps_TeraService!6))

## Fixed
 - [HCFS] bugfix/12929-url-string-overflow: Fix #12929 Swift URL is too long cause hcfs crash# CommentThis bug is resolved by using asprintf to allocate string memory on the fly.The cost is extra malloc on each curl OP, however due to the cost of curl is much more than a memory allocation, the performance should only have little effect.  ([!665](gateway-2-0/hcfs!665))
 - [HCFS] bugfix/fix_pin_file_downloaded_error: Fix Bug #12293 Pin app but app's file cannot be downloaded completelyFix block counting error in truncate_delete_block  ([!687](gateway-2-0/hcfs!687))
 - [HCFS] bugfix/reboot_hang_in_restoration_stage1: bugfix/reboot_hang_when_cachefull_in_restoration_stage1  ([!676](gateway-2-0/hcfs!676))
 - [HCFS] fix/#14255-delete-file-with-800-alias: Fix/#14255 crash on deleting file with 800 aliasIn function [_linked_list_enqueue](https://gitlab.hopebaytech.com/gateway-2-0/hcfs/merge_requests/689/diffs#485e277c31afa32cec3098ed4c059915b0fe4207_224_225):The condition of when to move 1 notify from linked list back to ring buffer is wrong. On the contrary, the shift should happen when ring buffer overflow, e.g. `notify.len >= FUSE_NOTIFY_RINGBUF_MAXLEN`.Those data believed original from linked list are actually from ring buffer's old data, thus cause error when hcfs try to free nested member that already been freed.I use simpler condition here. Notify get shift only if there is notify in linked list.```diff-		if (notify.len < FUSE_NOTIFY_RINGBUF_MAXLEN) {+		if (notify.linked_list_head) {```  ([!689](gateway-2-0/hcfs!689))
 - [HCFS] fix/dereferencing_after_deallocated: metaops: fix dereferencing fd after deallocated  ([!674](gateway-2-0/hcfs!674))
 - [HCFS] fix/ndk-warnings: eliminate NDK/clang compilation warnings  ([!620](gateway-2-0/hcfs!620))
 - [HCFS] fix/null_dereference: restoration_utils: avoid dereference of null pointer  ([!675](gateway-2-0/hcfs!675))
 - [HCFS] fix/out_of_bounds_access: do_restoration: fix ouf-of-bounds access  ([!662](gateway-2-0/hcfs!662))
 - [HCFS] fix/pthread_exit_portability: Fix pthread exit / pthread_joiun portabilityThe pthread_exit() function terminates the calling thread and returns avalue via retval that (if the thread is joinable) is available to another thread in the same process that calls pthread_join(3).  ([!683](gateway-2-0/hcfs!683))
 - [HCFS] fix/pyhcfs-build: fix pyhcfs build  ([!690](gateway-2-0/hcfs!690))
 - [HCFS] fix/recover-super-block-ut-error: Fix recover super block UT errorcustom_fake need to be defined earlier, since some runtime like covr will use basic functions like `pread`  ([!680](gateway-2-0/hcfs!680))
 - [HCFS] fix/redundant_checks: fuseop: eliminate redundant checks We should simplify probe error handling though.  ([!666](gateway-2-0/hcfs!666))
 - [HCFS] fix/scanf_field_overrun: hcfscurl: avoid buffer overflow resulting from scanfsscanf() without field width limits can crash with huge input data.  ([!679](gateway-2-0/hcfs!679))
 - [HCFS] fix/unexpected_inclusion: avoid unexpected header inclusionTo eliminate the complexity of static analyzer, we should avoid unexpected inclusion.  ([!682](gateway-2-0/hcfs!682))
 - [HCFS] fix/ut_meta_processing_overflow: UT: meta_processing: fix overflow in implicit constant conversion  ([!685](gateway-2-0/hcfs!685))
 - [HCFS] hotfix/int32_overflow: hotfix/int32_overflow  ([!673](gateway-2-0/hcfs!673))
 - [HCFS] origin/bugfix/block_count_error: Origin/bugfix/block count error  ([!686](gateway-2-0/hcfs!686))
 - [HCFS] Fix/#14171 wrong usage of smart cache 2.2.4
 - [HCFS] hotfix/fix_api_hang_in_shutdown: fix hang issue in api interface when shutting down ([!684](gateway-2-0/hcfs!684))
 - [Tera-App] bugfix/#13065: Bugfix/#13065 Don't sign out user if user complete change account process when user leave change account page  ([!160](gateway-2-0/android-management-app!160))
 - [Tera-App] bugfix/#13507: bugfix/#13507 Prevent folder from showing pin/unpin icon when user move to next level folder then back to parent folder at once.  ([!149](gateway-2-0/android-management-app!149))
 - [Tera-App] bugfix/#13866: Bugfix/#13866 Start a job service to pin /storage/emulated/0/Android folder until pin success  ([!162](gateway-2-0/android-management-app!162))
 - [Tera-App] bugfix/#13929: Bugfix/#13929 Add open app flag to start Tera app when user click on the restoration done notification.  ([!152](gateway-2-0/android-management-app!152))
 - [Tera-App] bugfix/#14081: Bugfix/#14081 Do not send insufficient pin space notification to user if user is doing restoration.  ([!151](gateway-2-0/android-management-app!151))
 - [Tera-App] bugfix/#14093: Bugfix/#14093 Not allow user to open app/file if data is not in local with "cloud isconnected" status.  ([!159](gateway-2-0/android-management-app!159))
 - [Tera-App] bugfix/#14096: Bugfix/#14096 If user click home key or switch app key to temporarily leave Tera app, keep the last visible page when user go back to Tera app again.  ([!150](gateway-2-0/android-management-app!150))
 - [Tera-App] bugfix/#14177: Bugfix/#14177 Fixed OOM due to not sampling when decoding image  ([!156](gateway-2-0/android-management-app!156))
 - [Tera-App] bugfix/#14183: Bugfix/#14183 Use isAdded() to check fragment is attached to activity or not before calling getString()  ([!161](gateway-2-0/android-management-app!161))
 - [Tera-App] bugfix/#14186: Bugfix/#14186 Handle error code returned from mgmt server  ([!157](gateway-2-0/android-management-app!157))
 - [Tera-App] bugfix/#14231: Bugfix/#14231 Change the text color of cancel.  ([!169](gateway-2-0/android-management-app!169))
 - [Tera-App] bugfix/#14273: Bugfix/#14273 Add error message if auth code is invalid or expired  ([!167](gateway-2-0/android-management-app!167))
 - [Tera-App] bugfix/#14286: Bugfix/#14286 If the showing content process is not completed, not allow user to change between All Apps/Pinned Apps or All Files/Pinned Files. It can prevent user from pining/unpining apps/files when the showing content process is still running.  ([!166](gateway-2-0/android-management-app!166))
 - [Tera-App] bugfix/#14334: Bugfix/#14334 Add try catche to handle the I/O error of reading thumbnail.  ([!170](gateway-2-0/android-management-app!170))
 - [Tera-App] bugfix/#14382: Bugfix/#14382 Don't not log out Tera app if failed to authenticate with Mgmt server  ([!174](gateway-2-0/android-management-app!174))
 - [Tera-App] bugfix/#14394: Bugfix/#14394 Disable all non-system apps with enabled status BOOSTED boost status  ([!172](gateway-2-0/android-management-app!172))
 - [Tera-App] bugfix/app_dialog_null_pointer: Bugfix/app_dialog_null_pointerFixed getString() null pointer exception due to fragment is not attached to activity  ([!165](gateway-2-0/android-management-app!165))
 - [Tera-App] bugfix/dismiss_progress_dialog_when_resotre_failed: Bugfix/dismiss_progress_dialog_when_resotre_failedDismiss progress dialog when register Tera in restoration page.  ([!168](gateway-2-0/android-management-app!168))
 - [Tera-Launcher] bugfix/#14222: Bugfix/#14222  ([!20](gateway-2-0/tera-launcher!20))
 - [Tera-Launcher] bugfix/#14261: The application info will be null while uninstall  ([!24](gateway-2-0/tera-launcher!24))
 - [Tera-Service] bugfix/#14202: The intent will be null while restart serviceIf service crashed and then restarted, the intent will be null.It will cause null pointer exception  ([!7](TeraAndroid/android_packages_apps_TeraService!7))

## CI / Refactoring / Other
 - [HCFS] ci/update_submodule: Change container and update submodule by remote  ([!668](gateway-2-0/hcfs!668))
 - [HCFS] ci/update_submodule: Update submodule and correct variable name  ([!667](gateway-2-0/hcfs!667))
 - [HCFS] refactor/encapsulate_logger: Refactoring: encapsulate logger internal structure Encapsulation can be used to hide data members and members function,that is important to not only object-oriented programming but also avoid mistakes on changing implementation-specific fields without being altered.This patch uses the technique of forware declaration to move the real logger structure into C source rather than header files, to ensure minimal exposure outside logger implementation.  ([!681](gateway-2-0/hcfs!681))

### Project Branch / Tag in this build 
 - [HCFS] release2.2.4_bugfix
 - [Tera-App] 2.2.4.0003 
 - [Tera-Launcher] 2.2.4.0002
 - [Tera-Service] 2.2.4.0002
 - [Nexus-5X] release-2.2.4-bugfix

v 2.2.4.2171
=====
## New Features
 - [HCFS] feature/1491-inform-media-files: Add middleware function add_notify_event_objbuild json object with json_pack and Call add_notify_event_obj to reduceoverhead on building and parsing json string.Other Changes* Update event table* Refactoring lookup_pathname in fuseop/unittests/fake_misc.c  ([!644](gateway-2-0/hcfs!644))
 - [HCFS] feature/recover_sb_dirty_queue: Feature/recover sb dirty queue  ([!639](gateway-2-0/hcfs!639))
 - [HCFS] feature/smart_cache_api_doc: Fix typos in API doc and restorecon cmd  ([!654](gateway-2-0/hcfs!654))
 - [HCFS] feature/smart_cache: Feature/smart cache  ([!646](gateway-2-0/hcfs!646))
 - [HCFS] feature/smartcache_restore: Feature/smartcache restore  ([!648](gateway-2-0/hcfs!648))
 - [Nexus-5X] feature/smart_cache: add selinux for restoration for block_dev  ([!78](gateway-2-0/nexus-5x!78))
 - [Nexus-5X] feature/smart_cache: smart cache mount and selinux Add getAppBoostStatus method  ([!76](gateway-2-0/nexus-5x!76))
 - [Nexus-5X] feature/use_thumbnails_in_gallery: Feature/use thumbnails in gallery  ([!72](gateway-2-0/nexus-5x!72))
 - [Tera-App] feature/smart_cache: Feature/smart cache  ([!146](gateway-2-0/android-management-app!146))
 - [Tera-Launcher] feature/alert_dialog_for_launch_app: Pop out alert dialog while launching app if wifi-only be set and usingcellular network  ([!16](gateway-2-0/tera-launcher!16))
 - [Tera-Launcher] feature/smart_cache: Feature/smart cache  ([!19](gateway-2-0/tera-launcher!19))
 - [Tera-Launcher] feature/toggle_pin_unpin_feature: Support toggle pin/unpin feature on Launcher  ([!18](gateway-2-0/tera-launcher!18))

## Fixed
 - [HCFS] bugfix/13664_delete_file_has_alot_alias: Bugfix/13664 delete file has alot alias  ([!647](gateway-2-0/hcfs!647))
 - [HCFS] bugfix/bug_dirty_cache_size_addtwice: fix add twice dirty size  ([!621](gateway-2-0/hcfs!621))
 - [HCFS] fix/array_index_used_before_limits_check: do_restoration: fix: array index used before limits checkWe shall always perform the following check before actual use:```Cwhile ((startpos < fbuflen) && (fbuf[startpos] != ' ')) {...}```Logical operator '&&' guarantees evaluation of their operands fromleft to right.  ([!663](gateway-2-0/hcfs!663))
 - [HCFS] fix/comparison_with_unsigned: unsigned variable is never less than zero  ([!664](gateway-2-0/hcfs!664))
 - [HCFS] fix/doubly_manipulated_buffer: do_restoration: prevent buffer from being doubly filledBuffer 'despath' is being written before its old content has been used.Likely a mistake during merge.  ([!660](gateway-2-0/hcfs!660))
 - [HCFS] fix/fs_manager_fault_handling: FS_manager: signify num_FS for further error handlingmemer num_FS in struct FS_MANAGER_HEAD_TYPE was of type uint64_t, whichis not ideal for error handling, and there are various non-functional checks correspondingly.  ([!661](gateway-2-0/hcfs!661))
 - [HCFS] fix/malformed_package_list: restoration_utils: stop at malformed package listPackage count in given path was not verified before we are building and sorting array, that caused unexpected troubles with malformed package list.  ([!657](gateway-2-0/hcfs!657))
 - [HCFS] fix/pyhcfs_old_version_FSStat_compatible: Fix to be compatiable FSStat V1  ([!643](gateway-2-0/hcfs!643))
 - [HCFS] fix/realloc_mistake: do_restoration: fix realloc mistake'removed_list' nulled but not freed upon failure. If realloc fails, then the original memory allocation exists, but isleaked as it is no longer referred to by d_data. So you need to keep ahandle on the original allocation until you have verified that the newallocation is valid.  ([!653](gateway-2-0/hcfs!653))
 - [HCFS] fix/restoration_resource_leak: do_restoration: fix resource leak  ([!652](gateway-2-0/hcfs!652))
 - [HCFS] hotfix/cherrypick_fixes: Fixed resource leak in mount_manager  ([!649](gateway-2-0/hcfs!649))
 - [HCFS] hotfix/default_quota_fix: Fixed default quota issue  ([!655](gateway-2-0/hcfs!655))
 - [Nexus-5X] bugfix/#13848: Show sync data completed notification only start sync by Settings  ([!79](gateway-2-0/nexus-5x!79))
 - [Nexus-5X] fix/issue_13839: add more i18n for Settings  ([!83](gateway-2-0/nexus-5x!83))
 - [Nexus-5X] fix/issue_13839: modify the string for sync-all-data button in the factory reset  ([!82](gateway-2-0/nexus-5x!82))
 - [Nexus-5X] hotfix/mount_hcfs_smartcache: mount smartcache  ([!77](gateway-2-0/nexus-5x!77))
 - [Tera-App] bugfix/#13753: Change sync status when user login Tera app and show access cloud settings if log~in with mobile network first time  ([!144](gateway-2-0/android-management-app!144))
 - [Tera-App] bugfix/#13866: Add a job service to pin android folder until pin success  ([!145](gateway-2-0/android-management-app!145))
 - [Tera-App] bugfix/#13875: Use JobScheduler to poll the piggyback from mgmt server even when app is removed by user from recent apps  ([!143](gateway-2-0/android-management-app!143))

## CI / Refactoring / Other
 - [HCFS] ci/refactoring: CI/refactoring 
    * Improve compatibility on installing docker host
    * Fix script's potential issue with shellcheck
    * Better error handling of shell script
    * Print source code near error lines ([!642](gateway-2-0/hcfs!642))
 - [Nexus-5X] modify_shutdown_condition: Modify shutdown condition while low powerStart shutdown thread while power level is lower than 3%  ([!81](gateway-2-0/nexus-5x!81))


v 2.2.3.2056
=====
## New Features
 - [Tera-Launcher] feature/alert_dialog_for_launch_app: Pop out alert dialog while launching app if wifi-only be set and usingcellular network  ([!16](gateway-2-0/tera-launcher!16))

## Fixed
 - [HCFS] bugfix/fix_pin_fail_caused_by_NOENT: skip pinning in case that entry is not found.  ([!637](gateway-2-0/hcfs!637))
 - [Tera-App] bugfix/#13836: bugfix/#13836 Handle josn non-key situation and not to execute getDirLocationStatus() if path is belong to system app  ([!142](gateway-2-0/android-management-app!142))
 - [Nexus-5X] bugfix/#13851: Fix alert dialog show conditionFactory reset will not show alert dialog while set to erase cloud data  ([!71](gateway-2-0/nexus-5x!71))

## CI / Refactoring / Other
 - [HCFS] ci/update_launch_by_branch_or_tag: update launcher with branch or tag See merge request !614  ([!633](gateway-2-0/hcfs!633))


v 2.2.3.2033
=====
## New Features
 - None

## Fixed
 - [HCFS] hotfix-2.2.3/fix_syncinode_errorhandling: Enhance error handling in sync inode to prevent IO error in meta sync occupying upload handle  ([!626](gateway-2-0/hcfs!626))
 - [HCFS] hotfix/download_issue_for_2.2.3: Hotfix/download issue for 2.2.3  ([!611](gateway-2-0/hcfs!611))
 - [HCFS] hotfix/hang_when_restoration_completed_2.2.3: Hotfix/hang when restoration completed 2.2.3  ([!624](gateway-2-0/hcfs!624))
 - [Tera-App] bigfix/#13648: bugfix/#13648 Check whether dialog fragment is attached to activity before getString()  ([!134](gateway-2-0/android-management-app!134))
 - [Tera-App] bugfix/#12866: bugfix/#12866 Don't execute factory reset if device category is unregistered ([!136](gateway-2-0/android-management-app!136))
 - [Tera-App] bugfix/#13063: bugfix/#13063 Set default log level to Log.INFO and incease log level to Log.DEBUGN if debug.tera.enable property is set true  ([!135](gateway-2-0/android-management-app!135))
 - [Tera-App] bugfix/change_account_failed_not_dismiss_progress_dialog: bugfix/change_account_failed_not_dismiss_progress_dialog Dismiss progress dialog when change account failed. In addition, integrate mgmt server error code into MgmtCluster.java  ([!127](gateway-2-0/android-management-app!127))
 - [Tera-App] fix/#13738: bug fix #13738  ([!138](gateway-2-0/android-management-app!138))

## CI / Refactoring / Other
 - None

v 2.2.3.1931
=====
## New Features
 - [Tera-App] enhancement/tera_api_service_performance: Enhancement/tera api service performance ([!126](gateway-2-0/android-management-app!126))
 - [Nexus-5x] feature/disable_storage_detail_usage: Disable storage detail usage ([!60](gateway-2-0/nexus-5x!60))
 - [Tera-Launcher] feature/alert_dialog_for_launch_app: Pop out alert dialog while launching app if wifi-only be set and using  cellular network ([!16](gateway-2-0/tera-launcher!16))

## Fixed
 - [HCFS] hotfix/download_issue_for_2.2.3: Hotfix/download issue for 2.2.3 (!611)
 - [Tera-App] fix/#13608: bug fix #13608 ([!128](gateway-2-0/android-management-app!128))
 - [Nexus-5x] fix/bug#13147: OTA時開啟wifi only,測試機開wifi連到另一台手機開的4G熱點,此時下載OTA package會停留在0%畫面 ([!66](gateway-2-0/nexus-5x!62/nexus-5x!66))
 - [Nexus-5x] fix/bug#13611: User在開啟Auto download後關閉"HopeBayTech Updater" 的存取權限,此時按"CHECK FOR UPDATE" 什麼訊息都沒出現 ([!66](gateway-2-0/nexus-5x!66))
 - [Nexus-5x] fix/bug#13620: 開啟"Update over Wi-Fi Only" 時在4G網路下如有更新不會跳出訊息提示使用者,手動檢查更新也會要求使用者開啟wifi ([!66](gateway-2-0/nexus-5x!66))
 - [Nexus-5x] fix/selinux_for_loop_device_mount_unmount: add selinux rule for hcfs mount/umount loop device ([!63](gateway-2-0/nexus-5x!63))
 - [Nexus-5x] fix/bug#13626: 當沒有新的版本可以更新時,user點選"CHECK FOR UPDATE"時不應該檢查cache available space ([!61](gateway-2-0/nexus-5x!61))
 - [Nexus-5x] bugfix/settings_crash: Fix Settings crash while enter USB&Storage fragment and press back key rapidly ([!58](gateway-2-0/nexus-5x!58))
 - [Tera-Launcher] bugfix/launcher_performance: Using callback for check app availability ([!17](gateway-2-0/tera-launcher!17))

## CI / Refactoring / Other
 - [Nexus-5x] update/lib_terafonnapi: Update terafonnapi.jar ([!59](gateway-2-0/nexus-5x!59))


v 2.2.3.1845
=====
## New Features
 - [HCFS] integrate/stage1_integration: Integrate/stage1 integration (!598)
 - [Tera-App] Feature/restoration ([!93](gateway-2-0/android-management-app!93))
 - [Nexus-5x] when ota failed, system will show a notification to notify the user ([!55](gateway-2-0/nexus-5x!55))
 - [Nexus-5x] ota app will check cache size before doing ota operation ([!55](gateway-2-0/nexus-5x!55))
 - [Nexus-5x] ota app will pop up System already updated alertDialog when the system is updated to the latest version ([!55](gateway-2-0/nexus-5x!55))

## Fixed
 - [HCFS] Fix/stress test rename in sdcard (!600)
 - [HCFS] bugfix/rename_in_external_volume: Fix rename cannot work in external volume. (!551)
 - [HCFS] fix/dead-branch: never call when ENCRYPT_ENABLE is not set (!576)
 - [Tera-App] bugfix/fix_null_pointer_exception: Fixed the null pointer exception ([!122](gateway-2-0/android-management-app!122))

## CI / Refactoring / Other
 - [Nexus-5x] refactor/HBTUpdater: remove unused import ([!55](gateway-2-0/nexus-5x!55))

v 2.2.2.1820
=====
## New Features
 - [Nexus-5x] feature/restoration_selinux_policy: add selinux rule for hcfs to stat com.google.android.gms ([!56](gateway-2-0/nexus-5x!56))

## Fixed
 - [Tera-App] bugfix/#13372: Accept the file name with white space and Chinese ([!121](gateway-2-0/android-management-app!121))
 - [Tera-App] enhancement/tera_api_service: Enhance the performance of Tera api service ([!120](gateway-2-0/android-management-app!120))
 - [Nexus-5x] bugfix/shutdown_battery_level: Start shutdown thread if using USB charging while battery low ([!57](gateway-2-0/nexus-5x!57))
 - [Tera-Launcher] 'bugfix/launcher_performance': Launcher performance tuning    1. Divide app tracking list from whole workspace to each pages  2. Fix folder icon sometimes not be updated issue. ([!15](gateway-2-0/tera-launcher!15))

## CI / Refactoring / Other
 - [HCFS] refactor/fuse_notify_UT: Refactoring fuse_notify UT    define _ut_wrap, used as a general wrapper of masked functions.     `#define <func>(...) _ut_wrap(<func>, <error value>, __VA_ARGS__)` can do:    1. _ut_wrap calls original function as normal  2. Set `<func>_error_on` can specify which time to return the error value  3. set `<func>_errno` can specify what errno should be on error. (!592)
 - [HCFS] fix/UT-cloud_get_put-scan-build: Use scan-build to fix UT error: cloud get put    * fix dead increment  * avoid double free  * fix memory leak (!573)

v 2.2.2.1795
=====
## New Features
 - [Nexus-5X] When permission is granted, the OTA package can trigger download from the notification

## Fixed
 - [HCFS] fix/refine_hcfsapid_log: Refine log (!588)
 - [HCFS] hotfix/add_version_log_back: add version number back to logger (!584)
 - [HCFS] hotfix/extra_delete_notify_error: Don't error if notify_delete non-existed entries (!583)
 - [HCFS] fix/reassigned-values: fix reassigned values, that is useless: Reported by cppcheck. (!577)
 - [Tera-App] bugfix/#13262: repin-system-app-highpririty-pin: Fix/repin system app highpririty pin ([!119](gateway-2-0/android-management-app!119))
 - [Tera-App] bugfix/#12840: Process the remaining user requests after user move to other page ([!107](gateway-2-0/android-management-app!107))
 - [Tera-App] bugfix/#13244: 1. Only decrypt response content when http resonse code is 200.  2. Code refacotoring ([!117](gateway-2-0/android-management-app!117))
 - [Tera-App] bugfix/#13300: Declare lost intent in AndroidManifest.xml ([!116](gateway-2-0/android-management-app!116))
 - [Tera-App] bugfix/#12926: Sign out when change account failed in order to let user select account in login page ([!114](gateway-2-0/android-management-app!114))
 - [Tera-App] bugfix/#13052: Show the installed app when user install the frist app from google play ([!111](gateway-2-0/android-management-app!111))
 - [Tera-App] bugfix/#12640: Jump to APP/FILE page when user clicks on insufficient space notification ([!108](gateway-2-0/android-management-app!108))
 - [Tera-App] bugfix/#12325: Show Tera ongoing notification on system boot-up ([!101](gateway-2-0/android-management-app!101))
 - [Tera-App] bugfix/#13329: 在Tera App中, 說明中心頁面, 使用意見回饋功能, 寄出的信沒有附上log
 - [Nexus-5x] refactor/HBTUpdater: remove some redundant variables and strings ([!52](gateway-2-0/nexus-5x!52))
 - [Nexus-5x] bugfix: System Update AlertDialog will not show up when permission is not granted after permission grant dialog is dismissed after hit autodownload preference  ([!52](gateway-2-0/nexus-5x!52))
 - [Nexus-5x] bugfix/shutdown_battery_level: Auto starts gracefully shutdown while battery level is lower than 3 ([!54](gateway-2-0/nexus-5x!54))
 - [Nexus-5x] fix/bug-13265: fix bug#13265: ota folder could not bind successfully([!53](gateway-2-0/nexus-5x!53))
 - [Tera-Launcher] bugfix/#13189: bugfix/#13189 Fix add shortcut crash: The shortcut item can't get package name using the same method as normal app  does ([!14](gateway-2-0/tera-launcher!14))
 - [Tera-Launcher] bugfix/#13136: Bugfix/#13136 Some google app can't show app info and pin/unpin drop target if the shortcut was auto created by google play ([!13](gateway-2-0/tera-launcher!13))

## CI / Refactoring / Other
 - [HCFS] ci/switch_branch_for_nexus5x: Add argument for switching branch in nexus5x (!585)
 - [HCFS] doc/update_pyhcfs_readme Update README.md (!582)
 - [HCFS] patch-1: Update README.md (!580)
 - [HCFS] test-meta-parser-2.2.2.1145: Test meta parser 2.2.2.1145    Merge to master for meta parser CI functional test (!575)

v 2.2.2.1743
=====
## New Features
 - [HCFS] feature/collect_logs Add collect system log API (!534)
 - [HCFS] feature/default_value_for_notify_server Add default notify server (!541)
 - [HCFS] feature/HCFSAPID_LOG_ROTATE Update hcfsapid logger (!547)
 - [HCFS] test/optimize_pkg_hash pkg_cache: replace with faster hash function     The deterministic version of djb2 hash is introduced to replace the original hash function in order to speed up. While compiled with gcc -O1, 10x speedup is measured. (!522)
 - [HCFS] hotfix/missing_cloud_stat Try to download backed-up cloud stat if missing (!554)
 - [HCFS] Feature/list volume (!546)
 - [Tera-App] Feature/decrypt json string ([!99](gateway-2-0/android-management-app!99))
 - [Tera-App] Feature/help page ([!91](gateway-2-0/android-management-app!91))
 - [Nexus-5x] auto grant permissions ([!38](gateway-2-0/nexus-5x!38))
 - [Nexus-5x] attach logs ([!38](gateway-2-0/nexus-5x!38))

## Fixed
 - [HCFS] Fix/handle todelete issues (!568)
 - [HCFS] hotfix/api_error_handling Avoid sending multiple return codes when error occurs (!565)
 - [HCFS] hotfix/fuse_notify_error_at_shutdown Fix: fuse notify log error at shutdown    Should not print error on shutdown (!571)
 - [HCFS] hotfix/api_error_handling Added http header initiation in test backend (!566)
 - [HCFS] Hotfix/fix curl auth token    Fix at 68da156b , also fix syntax warning of src/HCFS/hcfscurl.c (!570)
 - [HCFS] Add missing CURLOPT_NOSIGNAL=1    * Add missing CURLOPT_NOSIGNAL=1  * Use same CURLOPT setting as other usage in swift related function.  * Use CURLOPT_NOBODY to perform HEAD request  * Use noop_write_file_function for empty body, since default func is fwrite. (!558)
 - [HCFS] Bugfix/deleted file is not forgot    When unlink file, let hcfs calls fuse_lowlevel_notify_delete on all 3 mount point to force system release lookup count (opened by file count). (!516)
 - [HCFS] Bugfix/modify error handling on pyhcfs (!545)
 - [HCFS] Block SIGUSR1 when processing fs operations during unmount process (!540)
 - [HCFS] Error handling for unavailable download resource (!537)
 - [Tera-App] bugfix/interval_related_issue    Arrange all interval to Interval.java ([!113](gateway-2-0/android-management-app!113))
 - [Tera-App] bugfix/#13065 Retry three times for download user icon if failed ([!112](gateway-2-0/android-management-app!112))
 - [Tera-App] bugfix/#12580  Dismiss progress circle when user close dialog ([!110](gateway-2-0/android-management-app!110))
 - [Tera-App] Bugfix/#11966 ([!109](gateway-2-0/android-management-app!109))
 - [Tera-App] bug fix #12796 ([!97](gateway-2-0/android-management-app!97))
 - [Tera-App] Bugfix/#12532 ([!96](gateway-2-0/android-management-app!96))
 - [Tera-App] fix/issue_12787 Fix page will not transit to specific page ([!90](gateway-2-0/android-management-app!90))
 - [Tera-App] fix/help_page_alignment Fix help page alignment ([!106](gateway-2-0/android-management-app!106))
 - [Tera-App] fix/refine_app_availability Refine app availability ([!105](gateway-2-0/android-management-app!105))
 - [Tera-App] bugfix/#13015 Move Snack.make(...) to onActivityCreated() ([!102](gateway-2-0/android-management-app!102))
 - [Tera-App] bugfix/#12799 Only show calculating text once ([!103](gateway-2-0/android-management-app!103))
 - [Tera-App] bugfix/enhance_terafonn_api_service Enhance TeraFonnApiService ([!94](gateway-2-0/android-management-app!94))
 - [Tera-App] bugfix/#12743 Read preference from database instead of xml ([!95](gateway-2-0/android-management-app!95))
 - [Tera-App] bugfix/#12586 Keep getActivity() instance and replace getActivit() with the kept instance ([!92](gateway-2-0/android-management-app!92))
 - [Tera-App] bugfix/#12245 Handle upper case file extension ([!104](gateway-2-0/android-management-app!104))
 - [Nexus-5x] Fix/issue 12763 ([!40](gateway-2-0/nexus-5x!40))
 - [Nexus-5x] bug fix #12796 ([!41](gateway-2-0/nexus-5x!41))
 - [Nexus-5x] bug fix #12854 ([!43](gateway-2-0/nexus-5x!43))
 - [Nexus-5x] Fix/bug 13096    從狀態列點擊OTA更新後到更新頁面點選下載會出現 HopeBayTech Updater 停止運作訊息 ([!49](gateway-2-0/nexus-5x!49))
 - [Nexus-5x] Fix bug 13112 ([!48](gateway-2-0/nexus-5x!48))
 - [Nexus-5x] Fix bug 12676, 11948 ([!47](gateway-2-0/nexus-5x!47))
 - [Nexus-5x] Fix/settings display ([!46](gateway-2-0/nexus-5x!46))
 - [Nexus-5x] bug fix #13009 ([!45](gateway-2-0/nexus-5x!45))
 - [Nexus-5x] bug fix #13127 ([!50](gateway-2-0/nexus-5x!50))
 - [Nexus-5x] HBTUpdater refactor: replace tab by 4 spaces ([!51](gateway-2-0/nexus-5x!51))
 - [Nexus-5x] Create notification to notify the factory reset is ready to start ()[!44](gateway-2-0/nexus-5x!44))
 - [Nexus-5x] selinux for packages.xml ([!39](gateway-2-0/nexus-5x!39))
 - [Tera-Launcher] Start and bind service again while DeadObjectException occurs ([!12](gateway-2-0/tera-launcher!12))

## CI / Refactoring / Other
 - [HCFS] Ci/hcfs buildbox    1. 整理靜態分析工具的部屬程式  2. 升級分析工具  3. 修復之前壞掉的report  4. 分析範圍從 src/HCFS 擴展到 src/ (!550)
 - [HCFS] Change ota build process to fit path release\2.2-OTA-*    Change ota build process to fit path release\2.2-OTA-* (!561)
 - [HCFS] use {Leak,Address}Sanitizer to resolve memory leaks / heap-use-after-free / global-buffer-overflow    To utilize LeakSanitizer and AddressSanitizer, we shall allow the full compilation with clang in advance.    There are various memory violations detected in hcfs_clouddelete. (!572)
 - [HCFS] fix UT errno check    UT failed due to not set errno  on error.    in pyhcfs UT we use `pread_cnt_error_on_call_count` to control fake pread function return -1 but not set errno, however recent change returns -errno in some case, which leads to UT fail on return value checking. (!569)
 - [HCFS] tests: hcfscurl: eliminate unpredictable mutex locks    From Open Group Base Specifications [1]:    > The pthread_cond_broadcast() or pthread_cond_signal() functions  > may be called by a thread whether or not it currently owns the  > mutex that threads calling pthread_cond_wait() or  > pthread_cond_timedwait() have associated with the condition  > variable during their waits.    We do not have to lock the specific mutex every time when calling  pthread_cond_broadcast(). Instead, it is straightforward to unblock  threads blocked on the condition variable of swift token control  without performing mutex locks inside test thread functions.    mutrace detects no mutex contended according to filtering parameters.    [1] http://pubs.opengroup.org/onlinepubs/009695399/functions/ \             pthread_cond_broadcast.html (!567)
 - [HCFS] Use LeakSanitizer to eliminate memory leakage (!563)
 - [HCFS] push all hcfs library to device to debug (!559)
 - [HCFS] use undefined sanitizer to resolve memory violations (!556)
 - [HCFS] Fixed warnings pointed by scan-build (!552)
 - [HCFS] Fix errors scanned by ccc-analyzer    Fix errors scanned by ccc-analyzer and clang compiler (!525)
 - [HCFS] Fix memory and resource leaks    Use clang's Address Sanitizer to figure out these leaks. (!531)
 - [HCFS] fix signed integer overflow for expression 'UINT32_MAX - 10'    Take an example from 'INT02-C. Understand integer conversion rules',  CERT C Coding Standard:      signed char sc = SCHAR_MAX;      unsigned char uc = UCHAR_MAX;      signed long long sll = sc + uc;    The actual addition operation, due to integer promotion, takes place  between the two 32-bit int values. This operation is not influenced by  the resulting value being stored in a signed long long integer.    To ensure the expected subtraction two unsigned integers, we have to  cast in advance.    Reported-by-Cppcheck (!532)
 - [Tera-App] Ci/adjust release file permission ([!100](gateway-2-0/android-management-app!100))

## Redmine Issue Fixed
 - Bug #10940: User need to drop down the notification bar to see pin file fail message. It is inconvenience for user.
 - Bug #11065: Storage size did not release immediately after file was deleted on PC via MTP mode
 - Bug #11836: 當 在切換 List View 到 Grid View 時, 會出現 Pin/Unpin fail
 - Bug #11888: Pin/Unpin 時, 有機會跳回原本狀態
 - Bug #11948: OTA package did not delete after OTA finished
 - Bug #11966: Activation code fail reason is not clear for server problem.
 - Bug #12245: 圖片及影片將副檔名改成大寫後在Tera會無法預覽及開啟
 - Bug #12353: 在Switch account畫面如果停留超過15分鐘才開始轉換帳號, 轉換會失敗卡在"處理中,請稍後"
 - Bug #12412: 初次登入 Tera 後, 打開系統設定使用回復原廠設定功能, 並無清除雲端資料選項
 - Bug #12532: Transfer data, 在sync data時如果Tera app開啟wifi only但手機在4G狀態,會在transferring畫面一直轉圈圈
 - Bug #12538: Transfer data時,如果網路中斷,會在transferring畫面持續轉圈圈,應出現錯誤訊息中斷transfer過程
 - Bug #12541: Factory reset時,如果網路中斷,會在sync data畫面持續轉圈圈,應出現錯誤訊息中斷sync過程
 - Bug #12580: Grid View app information 點擊 Pin/Unpin 後, 跳出 app information 轉圈圈 又再跑出一次
 - Bug #12586: 在 Tera App 中長按檔案顯示詳細資料時太快關閉檔案明細小框框會使 Tear App crash
 - Bug #12676: after restarting android framework , some mount points are deleted
 - Bug #12743: 開啟Tera Notification settings後, 開啟/關閉網路不會跳出連線/斷線通知
 - Bug #12752: 當app不在local時且關閉網路,app在launcher 沒有顯示灰階, 直到點選程式集才會改變顏色
 - Bug #12760: Meta-parser API (get_vol_usage) 輸入錯誤內容的檔案，仍然可以parse出正確結果
 - Bug #12763: Factory reset device only, mgmt server status did not change to "fully backup, waiting for transfer"
 - Bug #12787: Tera app 設定wifi only 但Factory rest 時sync data只有4G網路, 選擇sync settings應該直接到Tera app的settings 頁面
 - Bug #12796: 訊息欄會常駐預期外的Tera app狀態顯示,點選後會進到App info畫面, user可能會誤觸停用Tera app
 - Bug #12799: 在APP/FILE page, Storage狀態每三秒會閃一次"Calculating"
 - Bug #12834: 無網路的情況下,當刪除檔案過多時，有機會將手機空間吃光
 - Bug #12854: 當登入多個google帳號, Factory reset頁面因為內容太長無法在第一頁看到"Erase Tera Cloud" 選項
 - Bug #13009: 燒完image後(沒有Open Gapps), OTA folder不在mount point
 - Bug #13015: 有時開啟Tera app會閃退,再開啟一次就會正常
 - Bug #13018: 沒有登入Tera時, 做factory rest 不應該確認網路狀態與sync data設定.
 - Bug #13065: 有時登入Tera後確認User profile會發現沒有顯示頭像圖片,從背景移掉Tera app再開啟即可顯示
 - Bug #13083: 每次開機都會做最佳化,導致開機需要等很久
 - Bug #13096: 從狀態列點擊OTA更新後到更新頁面點選下載會出現 HopeBayTech Updater 停止運作訊息
 - Bug #13112: OTA package下載完成提示安裝訊息,在中文會出現"%s" 字串
 - Bug #13127: 手機沒電關機之後重開機hcfs沒有正常啟動

v 2.2.2.1655
=====
## Bug Fixed
- [HCFS] hotfix/curl_NOSIGNAL (!558)
- [HCFS] fix/graceful_umount_hcfs (!540)

v 2.2.2.1465
=====
## Fixes 
 - [HCFS] Fixed #12713: hotfix/returnEIO_when_cache_full_and_nothing_replace (!527)
 - [Nexus-5x] Fix/issue 12529 [!36](gateway-2-0/nexus-5x!36)
 - [Nexus-5x] Save the storage usage in Preference to improve user experience [!37](gateway-2-0/nexus-5x!37)
 - [Tera-App] add startForeground [!89](gateway-2-0/android-management-app!89)
 - [Tera-App] Add query connection status interface [!84](gateway-2-0/android-management-app!84)
 - Bug #12746: 從背景移除Tera app, "com.hopebaytech.hcfsmgmt: server" 就掉了

## CI / Refactoring / test / other
 - [HCFS] tests: specify canonical LDFLAGS (!523)

v 2.2.2.1445
=====
## New Features
 - [Tera-App] Add a feedback button onto settings page ([!45] (gateway-2-0/android-management-app!45))
 - [Nexus-5x] Feature/factory reset alertdialog ([!33](gateway-2-0/nexus-5x!33))
 - [Nexus-5x] Support auto download OTA package    code is reviewed, it's ok! ([!31](gateway-2-0/nexus-5x!31))

## Fixes 
 - [HCFS] bugfix/remove_sb_size_from_system_size (!521)
 - [Tera-App] Process new json format and fixed change account related problems ([!81](gateway-2-0/android-management-app!81)), (#12550)
 - [Tera-App] Replace cs@tera.mobi with cs@hbmobile.com ([!80](gateway-2-0/android-management-app!80)), (#12595)
 - [Tera-App] Feature/share preference ([!78](gateway-2-0/android-management-app!78))
 - [Tera-App] Change the method of calculating tera storage usage ([!83](gateway-2-0/android-management-app!83))
 - [Tera-App] Add missing status code ([!79](gateway-2-0/android-management-app!79))
 - [Tera-App] bug fix #12430 ([!75](gateway-2-0/android-management-app!75))
 - [Tera-App] 當 pin 一個檔案失敗後, 再 pin 同一個檔案會顯示 "Unpinning successful" ([!75](gateway-2-0/android-management-app!75))
 - [Nexus-5x] Fix exception while launch storage & USB settings ([!35](gateway-2-0/nexus-5x!35))
 - [Nexus-5x] Fix/download dialog not display ([!32](gateway-2-0/nexus-5x!32))
 - [Nexus-5x] Fix/#12457 ([!30](gateway-2-0/nexus-5x!30))
 - [Nexus-5x] fix bug#12362:/storage/emulated/0/hbtupdater/ would be seen by the user and bug#11942:download complete alertdialog did not re-show if the user leave the ota download page when downloading ([!29](gateway-2-0/nexus-5x!29))
 - [Tera-Launcher] Add predefined color set ([!11](gateway-2-0/tera-launcher!11))
 - [Bug #12430] 當 pin 一個檔案失敗後, 再 pin 同一個檔案會顯示 "Unpinning successful"
 - [Bug #12544] 沒有登入Tera情況下,開啟Storage&USB會不斷出現Settings exception訊息.
 - [Bug #12550] Change account會顯示失敗,但是確認mgmt server帳號已被轉換到新的google account
 - [Bug #12595] Feedback email 應變更為cs@hbmobile.com
 - [Tera-App] Get Cloud storage total from quota and adjust the sequence ([!85](gateway-2-0/android-management-app!85))

## CI / Refactoring / Other
 - [HCFS] Ci/update flash script    Add some missed '-s \$TARGET_DEVICE' in FlashImages() (!519)

v 2.2.2.1412
=====
## New Features
 - [HCFS] Change the value of st_blksize to 4096 (!497)
 - [HCFS] Calculate cache size and meta size in 4K unit (!514)
 - [HCFS] Feature/pyhcfs lib - Added 2 new APIs: 
  1. get_vol_usage - Show usage of single volume from FSstat file; To get system total volume usage you need to get_vol_usage  of each FSstat file and add them up.
  2. list_file_blocks - list object name of all blocks of the file `NOTICE: Some file has sparse data blocks, which means some of data blocks may not listed in list. MyTera need to provide empty data chunk for those blocks when server file data!` (!482)

## Fixes 
 - [HCFS] Hotfix/add missing padding. metafile now actually have 64byte padding between header and content, preserved for later use. (!486)
 - [HCFS] Hotfix/make emulated case insensitive. Emulated volume (sdcard) is now case-insensitive, so that some apps (such as Asphalt 8) can work correctly. (Fixed bug #12284) (!483)
 - [HCFS] bugfix/delete meta while no backend. Modify timing of pushing inode into delete queue.
  1. Handle deletion of meta when backend is not set.
  2. Defer to enqueue to delete list in order to avoid to remove meta when cloud deletion is faster than local deletion. (!485)
 - [HCFS] Hotfix/backup pin size (!480)
 - [HCFS] bugfix/init_retry_sync_list (!481)
 - [HCFS] hotfix/tocloud_unittest (!479)
 - [HCFS] optimize longpow by table lookup. Typically, longpow is used for 1K base, and we can speed up by table  lookup as fastpath. In addition, slow path can be slightly improved. (!488)
 - [HCFS] exclude core dump file. After the failure of certain unit test iteration, core dump file might  be generated, and we should not track them in GIT repository. (!491)
 - [HCFS] fix bug #12281
 - [Tera-App] Error handling when getDirStatus api failed to get status [!68](gateway-2-0/android-management-app!68)
 - [Tera-App] Show system status bar after login Tera app [!69](gateway-2-0/android-management-app!69)
 - [Tera-App] Redefine storage usage scope [!67](gateway-2-0/android-management-app!67)
 - [Tera-App] Replace the pin/unpin failed message with revised version [!65](gateway-2-0/android-management-app!65)
 - [Nexus-5x] fix Bug #11930:ota app can reshow the download alertdialog after re-launch ota app [!27](gateway-2-0/nexus-5x!27)
 - [Nexus-5x] fix Bug #11948:OTA package did not delete after OTA finished [!28](gateway-2-0/nexus-5x!28)
 - [Nexus-5x] Get Tera storage usage from mgmt api service [!25](gateway-2-0/nexus-5x!25)
 - [Nexus-5x] remove the ota package file name from the notification [!26](gateway-2-0/nexus-5x!26)
 - [Tera-Launcher] Fix service leak issue    1. Synchronize bind service function  2. Handle exception in TeraApiService class [!10](gateway-2-0/tera-launcher!10)

## CI / Refactoring / Other
 - [HCFS] Update CI setup, fix ccm report error (!477)
 - [HCFS] Update README.md (!490)
 - [HCFS] Update README.md (!487)
 - [HCFS] build: fix typo (!492)
 - [HCFS] Fix #12461 Missing jansson dependency while building hcfs (!505)

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