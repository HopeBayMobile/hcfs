set -ex

HCFSVOL=../CLI_utils/HCFSvol

COPY_TO=sample
MPATH=/data/hcfs/metastorage
MOUNT_APP=/data/app
MOUNT_DIR=/data/data
MOUND_EXT=/storage/emulated

fifo=$MOUNT_DIR/tmp.fifo
link=$MOUNT_DIR/tmp.link
dir=$MOUNT_DIR/tmp.dir
reg=$MOUND_EXT/0/tmp.reg

# check if using linux or not
if ! HCFSvol; then
	USERNAME=$(whoami)
	GROUP=$(id -gn)
	$HCFSVOL create app internal
	sudo mkdir -p $MOUNT_APP
	sudo chown -R $USERNAME:$GROUP $MOUNT_APP
	$HCFSVOL mount app $MOUNT_APP

	$HCFSVOL create data internal
	sudo mkdir -p $MOUNT_DIR
	sudo chown -R $USERNAME:$GROUP $MOUNT_DIR
	$HCFSVOL mount data $MOUNT_DIR

	$HCFSVOL create emulated internal
	sudo mkdir -p $MOUND_EXT
	sudo chown -R $USERNAME:$GROUP $MOUND_EXT
	$HCFSVOL mount emulated $MOUND_EXT
fi

rm -rf $COPY_TO
mkdir $COPY_TO

cp $MPATH/fsmgr $COPY_TO/

rm -f ${fifo}
mkfifo ${fifo}
inod=`stat ${fifo} -c %i`
cp $MPATH/sub_$(( $inod % 1000 ))/meta$inod $COPY_TO/meta_isfifo

rm -f ${link}
ln -s ${link}.tmp ${link}
inod=`stat ${link} -c %i`
cp $MPATH/sub_$(( $inod % 1000 ))/meta$inod $COPY_TO/meta_islnk

rm -f ${reg} ${reg}.large
mkdir -p $(dirname $reg)
touch $reg
touch ${reg}.large
dd if=/dev/zero of=${reg} bs=1M count=1
dd if=/dev/zero of=${reg}.large bs=1 count=1 seek=10G
cat ${reg} >> ${reg}.large
cat ${reg} >> ${reg}.large
cat ${reg} >> ${reg}.large
inod=`stat ${reg}.large -c %i`
cp $MPATH/sub_$(( $inod % 1000 ))/meta$inod $COPY_TO/meta_isreg
# Copy volumn where large file was stored
inod=`stat -c %i $MOUND_EXT`
cp -v $MPATH/FS_sync/FSstat${inod} $COPY_TO/FSstat

rm -rf ${dir}
mkdir ${dir}
for i in `seq -w 1 1 3000`; do mkdir ${dir}/child-$i& done
wait
for i in `seq -w 1 3 3000`; do rmdir ${dir}/child-$i& done
for i in `seq -w 2 3 3000`; do rmdir ${dir}/child-$i& done
wait
inod=`stat ${dir} -c %i`
cp $MPATH/sub_$(( $inod % 1000 ))/meta$inod $COPY_TO/meta_isdir

cp $0 $COPY_TO/
tar zcvf sample.tgz $COPY_TO
