#/bin/bash

LOCAL_PATH=`pwd`

NDK_BUILD="/Users/fangyuxun/Android/android-ndk-r10e/ndk-build"

echo "=== Start to build HCFS ==="
src_path=$LOCAL_PATH"/build/HCFS/jni/"
cp $LOCAL_PATH/src/HCFS/*.c $src_path
cp $LOCAL_PATH/src/HCFS/*.h $src_path
cd $src_path
$NDK_BUILD

echo "=== Start to build HCFS CLI ==="
src_path=$LOCAL_PATH"/build/HCFS_CLI/jni/"
cp $LOCAL_PATH/src/CLI_utils/*.c $src_path
cp $LOCAL_PATH/src/CLI_utils/*.h $src_path
cd $src_path
$NDK_BUILD

echo "=== Start to build API Server ==="
src_path=$LOCAL_PATH"/build/API_SERV/jni/"
cp $LOCAL_PATH/src/API/*.c $src_path
cp $LOCAL_PATH/src/API/*.h $src_path
cd $src_path
$NDK_BUILD
