How to Apply TeraFonn Patch to Android Source Tree
====

In Android root path, using command to patch.
```
cd /path/to/s58a/source/top/
git apply /path/to/Terafonn_X.X.XXXX.patch
```

How to build S58A with TeraFonn
====

build image with following command
```
ENABLE_HCFS=1 ./build.sh -s s58a_aap_gen1 -v
```

How to build S58A without TeraFonn
====

build image with following command
```
./build.sh -s s58a_aap_gen1 -v
```
