Repository for Gateway 2.0 HCFS
===============================

Latest source code under 'src/HCFS'.
Latest unittest code under 'tests/unit_test' (run the script run_unittests for all unittests).
Latest source code for command-line utilities under 'src/CLI_utils'.

Developing Flow
-----------
1. Fork branch from android-dev and developing in it.
2. Unit test your code
3. Check coding style of your code (do it last as coding style breaks during modify)

    `./tests/code_checking/hb_clint.py $(git diff android-dev  --name-only | grep '.*\.c$\|.*\.h$\|.*\.cc$')`
4. Create Merge Request on gitlab
5. Ask team member to peer review your Merge Request, collect at least 2 upvotes.
6. Reassign MR to team member whom can accept it.

Config file
-----------

Please put the following as a text file under /etc/hcfs.conf
```
METAPATH = (Directory of meta file storage, must exist)
BLOCKPATH = (Diectory of block file storage, must exist)
CACHE_SOFT_LIMIT = (Soft limit for triggering cache replacement, in bytes)
CACHE_HARD_LIMIT = (Hard limit for blocking further IO, in bytes)
CACHE_DELTA = (Amount of cache to be replaced after hitting hard limit and before allowing further IO, in bytes)
CACHE_RESERVED = (Size of cache space reserved for high-priority pin files/dirs)
MAX_BLOCK_SIZE = (Data block size, in bytes)
CURRENT_BACKEND = (Name of backend type)  (Currently supporting swift or S3)
SWIFT_ACCOUNT = (swift account)
SWIFT_USER = (swift user name)
SWIFT_PASS = (swift password)
SWIFT_URL = (swift proxy IP + port)
SWIFT_CONTAINER = (swift container)
SWIFT_PROTOCOL = (protocol for swift connection (https for now))
S3_ACCESS = (S3 access key)
S3_SECRET = (S3 secret key)
S3_URL = (S3 url)
S3_BUCKET = (S3 bucket name)
S3_PROTOCOL = (protocol for S3 connection (https for now))
LOG_LEVEL = (To which log level the log messages should be dumped) (Ranged from 0 to 10 now, with 0 being the most critical level)
LOG_PATH = (A directory that create hcfs log files. This setting is optional and hcfs will create log file at current path if this term is not set.
           This will be ignored if the path is invalid, and create log file at current path if it is ignored.)
```

Example:
```
METAPATH= /home/jiahongwu/testHCFS/metastorage
BLOCKPATH = /home/jiahongwu/testHCFS/blockstorage
CACHE_SOFT_LIMIT = 53687091
CACHE_HARD_LIMIT = 107374182
CACHE_DELTA = 10485760
CACHE_RESERVED = 536870912
MAX_BLOCK_SIZE = 1048576
CURRENT_BACKEND = swift
SWIFT_ACCOUNT = hopebay
SWIFT_USER = hopebay
SWIFT_PASS = ZZZZZZZZ
SWIFT_URL = 192.168.0.100:8080
SWIFT_CONTAINER = hopebay_private_container
SWIFT_PROTOCOL = https
S3_ACCESS = XXXXX
S3_SECRET = YYYYY
S3_URL = s3.hicloud.net.tw
S3_BUCKET = testgateway
S3_PROTOCOL = https
LOG_LEVEL = 10
LOG_PATH = /home/kewei/
```

To change configuration:
-----------------
  1. Use "adb pull /etc/hcfs.conf hcfs.conf" to pull configuration file template.
  2. Edit configuration (might need to change cache size and log level too)
  3. Use "adb push hcfs.conf /data/hcfs.conf.tmp" to push configuration file to a temp location.
  4. Remove /data/hcfs.conf if the file exists.
  5. Use "hcfsconf enc /data/hcfs.conf.tmp /data/hcfs.conf" to encrypt the config file.
  6. Delete /data/hcfs.conf.tmp
  7. Reboot.

Required packages
-----------------
    build-essential
    libattr1-dev
    libfuse-dev
    libcurl4-openssl-dev
    liblz4-dev
    libssl-dev
    libsqlite3-dev
    libjansson-dev
    libcap-dev
