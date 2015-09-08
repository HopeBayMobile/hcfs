Repository for Gateway 2.0 HCFS
===============================

Latest source code under 'src/HCFS'.
Latest unittest code under 'tests/unit_test' (run the script
run_unittests for all unittests).
Latest source code for command-line utilities under 'src/CLI_utils'.

Config file
-----------

Please put the following as a text file under /etc/hcfs.conf

METAPATH = (Directory of meta file storage, must exist)
BLOCKPATH = (Diectory of block file storage, must exist)
CACHE_SOFT_LIMIT = (Soft limit for triggering cache replacement, in bytes)
CACHE_HARD_LIMIT = (Hard limit for blocking further IO, in bytes)
CACHE_DELTA = (Amount of cache to be replaced after hitting hard limit and before allowing further IO, in bytes)
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

Example:

METAPATH= /home/jiahongwu/testHCFS/metastorage
BLOCKPATH = /home/jiahongwu/testHCFS/blockstorage
CACHE_SOFT_LIMIT = 53687091
CACHE_HARD_LIMIT = 107374182
CACHE_DELTA = 10485760
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

Required packages
-----------------

ATTR
FUSE
CURL
OPENSSL
LZ4

Required by Dev
---------------

libattr1-dev
libfuse-dev
libcurl4-openssl-dev
libssl-dev
liblz4-dev
