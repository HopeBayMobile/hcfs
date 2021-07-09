::
:: Copyright (c) 2021 HopeBayTech.
::
:: This file is part of Tera.
:: See https://github.com/HopeBayMobile for further info.
::
:: Licensed under the Apache License, Version 2.0 (the "License");
:: you may not use this file except in compliance with the License.
:: You may obtain a copy of the License at
::
::     http://www.apache.org/licenses/LICENSE-2.0
::
:: Unless required by applicable law or agreed to in writing, software
:: distributed under the License is distributed on an "AS IS" BASIS,
:: WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
:: See the License for the specific language governing permissions and
:: limitations under the License.
::
@if "%CD%" == "C:\Windows" (echo ===
	echo Error: On Windows, use [ net use * \\nas\ubuntu\CloudDataSolution ] to mount here as network drive and execute again.
	echo ===
	pause
	exit)
@prompt $$ 
adb wait-for-device
adb root
adb wait-for-device
adb disable-verity | findstr /I "reboot"
if %errorlevel% == 0 (
	adb reboot
)

adb wait-for-device
adb root
adb wait-for-device
adb remount
adb push hopebay /system/bin
adb shell busybox --install /system/xbin
pause