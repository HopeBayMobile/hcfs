<!--
Copyright (c) 2021 HopeBayTech.

This file is part of Tera.
See https://github.com/HopeBayMobile for further info.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
-->
Cross-compile android executable binary in Ubuntu
=================================================

NOTE
----
* MODIFY `DOCHECK` AND FILL IT WITH CORRESPONDING TERA USER INFO.
* `MAKEFILE` IS WRITTEN SPECIFICALLY TO TEST NEXUS-5X. MODIFICATION TO
 `MAKEFILE` MIGHT BE NEEDED IN ORDER TO WORK ON OTHER DEVICE AS WELL.

Steps
-----
1. Download [Android NDK](https://developer.android.com/ndk/index.html) from [here](http://dl.google.com/android/repository/android-ndk-r12-linux-x86_64.zip). This will download `android-ndk-r12`.
2. Test has been implemented in Makefile.
    * `$ make run`
        * This will build `US000_03.c`, push and run it on device, and return
          the `diff` result of dataOnCloud and dataOnLocal.
    * `make clean`

