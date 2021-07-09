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
###  **How to use**
----------

#### **Starting HCFS**
1. Build HCFS by using "make" under 'src/HCFS'.

2. Use './hcfs' if no parameter is needed. If needs to add options
(e.g. -oallow_other,big_writes to allow CIFS sharing and write in
chunks larger than 4KB), use './hcfs /tmp -oallow_other,xxxx'. Note
that 'tmp' can be any directory and this won't be the actual mountpoint.

3. Compile the utility program 'HCFSvol.c' under 'src/CLI_utils' using
'cc HCFSvol.c -o HCFSvol'.

ps. Before launch the hcfs, please make sure the permission setting of /etc/fuse.conf is 644

#### **Creating and mounting a HCFS FS**

1. Use './HCFSvol create FSname' for creating the FS 'FSname'.
(For Android, need to specify whether the mount is for internal or
external storage, using './HCFSvol create FSname internal' or
'./HCFSvol create FSname external'.

2. Use './HCFSvol mount FSname MntPoint' for mounting the FS 'FSname'
to the mountpoint 'MntPoint'.

#### **Other more useful commands for HCFSvol**

1. 'delete FSname': Delete FS 'FSname'. Note that the filesystem
has to be empty and unmounted.

2. 'list': Lists existing filesystems.

3. 'unmount FSname': Unmount FS 'FSname'. Note that you can also use
'fusermount -u MntPoint' to unmount the FS mounted on MntPoint.

4. 'unmountall': Unmount all filesystems.

5. 'terminate': Shutdown hcfs and unmount all filesystems.


###  **Template**
----------

#### **Dependency**

```preinstall
CURL
OPENSSL
FUSE



Python Package

Debian Package

Driver

```

----------

#### **GetStarted**

```Develop
Develop Environment 
```

----------

#### **Deployment**

```Deployment
Source Code Deployment
    
Debian Package Deployment

```

----------
