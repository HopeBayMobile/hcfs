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
## Error codes

* **-1** : System call error, error message will print to stderr and leave a copy in `ret['error_msg']`.
* **-2** : Unsupported meta version. File's meta version is not supported by pyhcfs.

list_volume
==============================
* If success, it will return a list of tuples (**inode**, **volume type**, **volume name**), or a negative value is returned on error.
* The volume type may be one of the following three kinds of value:

    | Volume type           | Number in API | Description                       |         
    | --------------------- | ------------- | --------------------------------- |         
    | ANDROID_INTERNAL      | 1             | This volume is an internal volume |         
    | ANDROID_EXTERNAL      | 2             | This volume is an external volume |         
    | ANDROID_MULTIEXTERNAL | 3             | This volume is an external volume, which is multi-mounted to support [Android 6.0's Runtime permissions](https://source.android.com/devices/storage/#runtime_permissions) |

* Example of list_volume (Success)

```
(b"test_data/v1/android/fsmgr")

[(3, 1, b'hcfs_app'), (2, 1, b'hcfs_data'), (130, 3, b'hcfs_external')]
```

* Example of list_volume (Failure)

```
list_volume(b"")

Error: list_volume: No such file or directory
-1
```

parse_meta
==============================
* `ret['result'] = 0` on success, or a negative `ret['result']` is returned on error.

* Example of parse_meta (Success)

```
parse_meta(b"test_data/v1/android/meta_isdir")

{ 'child_number': 1000,
  'file_type': 0,
  'result': 0,
  'stat': { '__pad1': 0,
            'atime': 1470911895,
            'atime_nsec': 0,
            'blksize': 1048576,
            'blocks': 0,
            'ctime': 1470911938,
            'ctime_nsec': 0,
            'dev': 0,
            'gid': 0,
            'ino': 6088,
            'magic': [104, 99, 102, 115],
            'metaver': 1,
            'mode': 16877,
            'mtime': 1470911938,
            'mtime_nsec': 0,
            'nlink': 1002,
            'rdev': 0,
            'size': 0,
            'uid': 0}}
```

* Example of parse_meta (Failure)

```
parse_meta(b"test_data/v0/android/meta_isreg")

Error: parse_meta: Unsupported meta version
{ 'child_number': 0,
  'error_msg': 'parse_meta: Unsupported meta version',
  'file_type': 0,
  'result': -2,
  'stat': { '__pad1': 0,
            'atime': 0,
            'atime_nsec': 0,
            'blksize': 0,
            'blocks': 0,
            'ctime': 0,
            'ctime_nsec': 0,
            'dev': 0,
            'gid': 0,
            'ino': 0,
            'magic': [0, 0, 0, 0],
            'metaver': 0,
            'mode': 0,
            'mtime': 0,
            'mtime_nsec': 0,
            'nlink': 0,
            'rdev': 0,
            'size': 0,
            'uid': 0}}
```

list_dir_inorder
==============================
* `ret['result'] = 0` on success, or a negative `ret['result']` is returned on error.
* Example of list_dir_inorder (Success)

```
list_dir_inorder(b"test_data/v1/android/meta_isdir", ret["offset"], limit=100)

{ 'child_list': [ {'d_name': b'.', 'd_type': 0, 'inode': 6088},
                  {'d_name': b'..', 'd_type': 0, 'inode': 128},
                  {'d_name': b'child-0003', 'd_type': 0, 'inode': 6098},
                  {'d_name': b'child-0006', 'd_type': 0, 'inode': 6097},
                  {'d_name': b'child-0009', 'd_type': 0, 'inode': 6090},
                  {'d_name': b'child-0012', 'd_type': 0, 'inode': 6095},
                  {'d_name': b'child-0015', 'd_type': 0, 'inode': 6185},
                  {'d_name': b'child-0018', 'd_type': 0, 'inode': 6102},
                  {'d_name': b'child-0021', 'd_type': 0, 'inode': 6113},
                  {'d_name': b'child-0024', 'd_type': 0, 'inode': 6173},
                  {'d_name': b'child-0027', 'd_type': 0, 'inode': 6142},
                  {'d_name': b'child-0030', 'd_type': 0, 'inode': 6176},
                  {'d_name': b'child-0033', 'd_type': 0, 'inode': 6156},
                  {'d_name': b'child-0036', 'd_type': 0, 'inode': 6174},
                  {'d_name': b'child-0039', 'd_type': 0, 'inode': 6111},
                  {'d_name': b'child-0042', 'd_type': 0, 'inode': 6121},
                  {'d_name': b'child-0045', 'd_type': 0, 'inode': 6103},
                  {'d_name': b'child-0048', 'd_type': 0, 'inode': 6105},
                  {'d_name': b'child-0051', 'd_type': 0, 'inode': 6233},
                  {'d_name': b'child-0054', 'd_type': 0, 'inode': 6268},
                  {'d_name': b'child-0057', 'd_type': 0, 'inode': 6133},
                  {'d_name': b'child-0060', 'd_type': 0, 'inode': 6140},
                  {'d_name': b'child-0063', 'd_type': 0, 'inode': 6147},
                  {'d_name': b'child-0066', 'd_type': 0, 'inode': 6214},
                  {'d_name': b'child-0069', 'd_type': 0, 'inode': 6285},
                  {'d_name': b'child-0072', 'd_type': 0, 'inode': 6157},
                  {'d_name': b'child-0075', 'd_type': 0, 'inode': 6281},
                  {'d_name': b'child-0078', 'd_type': 0, 'inode': 6141},
                  {'d_name': b'child-0081', 'd_type': 0, 'inode': 6146},
                  {'d_name': b'child-0084', 'd_type': 0, 'inode': 6217},
                  {'d_name': b'child-0087', 'd_type': 0, 'inode': 6192},
                  {'d_name': b'child-0090', 'd_type': 0, 'inode': 6655},
                  {'d_name': b'child-0093', 'd_type': 0, 'inode': 6238},
                  {'d_name': b'child-0096', 'd_type': 0, 'inode': 6657},
                  {'d_name': b'child-0099', 'd_type': 0, 'inode': 6198},
                  {'d_name': b'child-0102', 'd_type': 0, 'inode': 6684},
                  {'d_name': b'child-0105', 'd_type': 0, 'inode': 6215},
                  {'d_name': b'child-0108', 'd_type': 0, 'inode': 6723},
                  {'d_name': b'child-0111', 'd_type': 0, 'inode': 6227},
                  {'d_name': b'child-0114', 'd_type': 0, 'inode': 6126},
                  {'d_name': b'child-0117', 'd_type': 0, 'inode': 6732},
                  {'d_name': b'child-0120', 'd_type': 0, 'inode': 6284},
                  {'d_name': b'child-0123', 'd_type': 0, 'inode': 6259},
                  {'d_name': b'child-0126', 'd_type': 0, 'inode': 6132},
                  {'d_name': b'child-0129', 'd_type': 0, 'inode': 6730},
                  {'d_name': b'child-0132', 'd_type': 0, 'inode': 6247},
                  {'d_name': b'child-0135', 'd_type': 0, 'inode': 6693},
                  {'d_name': b'child-0138', 'd_type': 0, 'inode': 6264},
                  {'d_name': b'child-0141', 'd_type': 0, 'inode': 6700},
                  {'d_name': b'child-0144', 'd_type': 0, 'inode': 6279},
                  {'d_name': b'child-0147', 'd_type': 0, 'inode': 6704},
                  {'d_name': b'child-0150', 'd_type': 0, 'inode': 6694},
                  {'d_name': b'child-0153', 'd_type': 0, 'inode': 6164},
                  {'d_name': b'child-0156', 'd_type': 0, 'inode': 6741},
                  {'d_name': b'child-0159', 'd_type': 0, 'inode': 6431},
                  {'d_name': b'child-0162', 'd_type': 0, 'inode': 6721},
                  {'d_name': b'child-0165', 'd_type': 0, 'inode': 6136},
                  {'d_name': b'child-0168', 'd_type': 0, 'inode': 6293},
                  {'d_name': b'child-0171', 'd_type': 0, 'inode': 6276},
                  {'d_name': b'child-0174', 'd_type': 0, 'inode': 6391},
                  {'d_name': b'child-0177', 'd_type': 0, 'inode': 6543},
                  {'d_name': b'child-0180', 'd_type': 0, 'inode': 6447},
                  {'d_name': b'child-0183', 'd_type': 0, 'inode': 6395},
                  {'d_name': b'child-0186', 'd_type': 0, 'inode': 6584},
                  {'d_name': b'child-0189', 'd_type': 0, 'inode': 6594},
                  {'d_name': b'child-0192', 'd_type': 0, 'inode': 6486},
                  {'d_name': b'child-0195', 'd_type': 0, 'inode': 7173},
                  {'d_name': b'child-0198', 'd_type': 0, 'inode': 6430},
                  {'d_name': b'child-0201', 'd_type': 0, 'inode': 7256},
                  {'d_name': b'child-0204', 'd_type': 0, 'inode': 6413},
                  {'d_name': b'child-0207', 'd_type': 0, 'inode': 6414},
                  {'d_name': b'child-0210', 'd_type': 0, 'inode': 6593},
                  {'d_name': b'child-0213', 'd_type': 0, 'inode': 6598},
                  {'d_name': b'child-0216', 'd_type': 0, 'inode': 6609},
                  {'d_name': b'child-0219', 'd_type': 0, 'inode': 7223},
                  {'d_name': b'child-0222', 'd_type': 0, 'inode': 6186},
                  {'d_name': b'child-0225', 'd_type': 0, 'inode': 6299},
                  {'d_name': b'child-0228', 'd_type': 0, 'inode': 6506},
                  {'d_name': b'child-0231', 'd_type': 0, 'inode': 6305},
                  {'d_name': b'child-0234', 'd_type': 0, 'inode': 6283},
                  {'d_name': b'child-0237', 'd_type': 0, 'inode': 6500},
                  {'d_name': b'child-0240', 'd_type': 0, 'inode': 7555},
                  {'d_name': b'child-0243', 'd_type': 0, 'inode': 7517},
                  {'d_name': b'child-0246', 'd_type': 0, 'inode': 6739},
                  {'d_name': b'child-0249', 'd_type': 0, 'inode': 6396},
                  {'d_name': b'child-0252', 'd_type': 0, 'inode': 6286},
                  {'d_name': b'child-0255', 'd_type': 0, 'inode': 6475},
                  {'d_name': b'child-0258', 'd_type': 0, 'inode': 6743},
                  {'d_name': b'child-0261', 'd_type': 0, 'inode': 6234},
                  {'d_name': b'child-0264', 'd_type': 0, 'inode': 6580},
                  {'d_name': b'child-0267', 'd_type': 0, 'inode': 7229},
                  {'d_name': b'child-0270', 'd_type': 0, 'inode': 7362},
                  {'d_name': b'child-0273', 'd_type': 0, 'inode': 7263},
                  {'d_name': b'child-0276', 'd_type': 0, 'inode': 7514},
                  {'d_name': b'child-0279', 'd_type': 0, 'inode': 6211},
                  {'d_name': b'child-0282', 'd_type': 0, 'inode': 6624},
                  {'d_name': b'child-0285', 'd_type': 0, 'inode': 6629},
                  {'d_name': b'child-0288', 'd_type': 0, 'inode': 7337},
                  {'d_name': b'child-0291', 'd_type': 0, 'inode': 6464},
                  {'d_name': b'child-0294', 'd_type': 0, 'inode': 7421}],
  'num_child_walked': 100,
  'offset': (83560, 9),
  'result': 0}
```

get_vol_usage
==============================
* `ret['result'] = 0` on success, or a negative `ret['result']` is returned on error.
* Example of get_vol_usage (Success)

```
get_vol_usage(b"test_data/v1/android/FSstat")

{'result': 0, 'usage': 1373381904}
```

list_file_blocks
==============================
* `ret['result'] = 0` on success, or a negative `ret['result']` is returned on error.
* Example of list_file_blocks (Success)

```
list_file_blocks(b"test_data/v1/android/meta_isreg")

{ 'block_list': [ 'data_6086_10240_1',
                  'data_6086_10241_1',
                  'data_6086_10242_1',
                  'data_6086_10243_1'],
  'result': 0,
  'ret_num': 4}
```

## **Pitfall of list_file_blocks**
1. In this example, the block from 0 to 10239 are missing. While accessing those missing blocks, reader should assume they are zero-filled blocks to avoid reading data file which are not existed.
2. If the missing block is the last one in list, it is assumed to be a zero-filled block with size `file_size % 1048576`. File size can be read from `parse_meta` function.