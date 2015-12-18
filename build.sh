#/bin/bash

LOCAL_PATH=`pwd`
source $LOCAL_PATH/.ndk_path
if ! type -P ndk-build && [[ -z "$NDK_BUILD" ]]; then
	echo "Cannot find path of ndk-build."
	echo "Please set ndk-build path as following:"
	echo "echo NDK_BUILD=[NDK_PATH/ndk-build] > $LOCAL_PATH/.ndk_path"
	exit 1
fi

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
