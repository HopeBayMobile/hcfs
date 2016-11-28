@ECHO OFF
:: Copyright 2012 The Android Open Source Project
::
:: Licensed under the Apache License, Version 2.0 (the "License");
:: you may not use this file except in compliance with the License.
:: You may obtain a copy of the License at
::
::      http://www.apache.org/licenses/LICENSE-2.0
::
:: Unless required by applicable law or agreed to in writing, software
:: distributed under the License is distributed on an "AS IS" BASIS,
:: WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
:: See the License for the specific language governing permissions and
:: limitations under the License.

SET "PATH=%PATH%%CD%\utils;%SYSTEMROOT%\System32;"
SET PATH

@ECHO ON
adb kill-server
adb wait-for-device
adb root
adb wait-for-device
:: adb shell "set -- `ps | grep hcfs`; kill -9 $2"
adb reboot bootloader
call FlashAllInFastboot.bat
adb kill-server