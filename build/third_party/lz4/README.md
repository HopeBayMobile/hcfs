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
LZ4 - Library Files
================================

This directory contains many files, but you don't necessarily need them all.

If you want to integrate LZ4 compression/decompression into your program, you basically need to include "**lz4.c**" and "**lz4.h**" only.

If you want more compression, at the cost of compression speed (but preserving decompression speed), you will also have to include "**lz4hc.c**" and "**lz4hc.h**". Note that lz4hc needs lz4 to work properly.

Next level, if you want to produce files or data streams compatible with lz4 utility, you will have to use and include "**lz4frame.c**" and **lz4frame.h**". This library encapsulate lz4-compressed blocks into the official interoperable frame format. In order to work properly, lz4frame needs lz4 and lz4hc, and also "**xxhash.c**" and "**xxhash.h**", which provide the error detection algorithm.

A more complex "lz4frame_static.h" is also provided, although its usage is not recommended. It contains definitions which are not guaranteed to remain stable within future versions. Use only if you don't plan to update your lz4 version.

The other files are not source code. There are :

 - LICENSE : contains the BSD license text
 - Makefile : script to compile or install lz4 library (static or dynamic)
 - liblz4.pc.in : for pkg-config (make install)

