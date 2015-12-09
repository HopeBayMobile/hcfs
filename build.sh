#/bin/bash

echo "=== Start to build HCFS ==="
cp src/HCFS/*.c build/HCFS/jni/
cp src/HCFS/*.h build/HCFS/jni/
~/Android/android-ndk-r10e/ndk-build

echo "=== Start to build HCFS CLI ==="
cp src/CLI_utils/*.c build/HCFS_CLI/jni/
cp src/CLI_utils/*.h build/HCFS_CLI/jni/
~/Android/android-ndk-r10e/ndk-build

echo "=== Start to build API Server ==="
cp src/API/*.c build/API_SERV/jni/
cp src/API/*.h build/API_SERV/jni/
~/Android/android-ndk-r10e/ndk-build
