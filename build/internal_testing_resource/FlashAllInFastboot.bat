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
fastboot oem unlock-go
:: FOR %%f IN (aosp_bullhead-img-*.zip) DO fastboot -w update %%f
FOR %%f IN (*.img) DO ( fastboot flash %%~nf %%f || goto :error )
fastboot reboot || goto :error
@ECHO OFF
echo Press any key to exit...
pause >nul
exit

:error
@ECHO OFF
echo Failed with error #%errorlevel%.
echo Press any key to exit...
pause >nul
exit /b %errorlevel%
