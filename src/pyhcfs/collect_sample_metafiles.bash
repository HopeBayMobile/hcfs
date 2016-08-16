if [ `id -u` -ne 0 ];then
	echo not root!
	exit
fi
set -ex

target=sample
mp=/data/hcfs/metastorage
fifo=/storage/emulated/tmp.fifo
link=/data/data/tmp.link
reg=/storage/emulated/tmp.reg
dir=/storage/emulated/tmp.dir

rm -rf $target
mkdir $target

cp $mp/FS_sync/FSstat2 $target/
cp $mp/fsmgr $target/

rm -f ${fifo}
mkfifo ${fifo}
inod=`stat ${fifo} -c %i`
cp $mp/sub_$(( $inod % 1000 ))/meta$inod $target/meta_isfifo

rm -f ${link}
ln -s ${link}.tmp ${link}
inod=`stat ${link} -c %i`
cp $mp/sub_$(( $inod % 1000 ))/meta$inod $target/meta_islnk

rm -f ${reg}
rm -f ${reg}.large
dd if=/dev/zero of=${reg} bs=1M count=1
dd if=/dev/zero of=${reg}.large bs=1 count=1 seek=10G
cat ${reg} >> ${reg}.large
cat ${reg} >> ${reg}.large
cat ${reg} >> ${reg}.large
inod=`stat ${reg}.large -c %i`
find /data/hcfs/blockstorage/ | grep block${inod}_
cp $mp/sub_$(( $inod % 1000 ))/meta$inod $target/meta_isreg

rm -rf ${dir}
mkdir ${dir}
for i in `seq -w 1 1 3000`; do mkdir ${dir}/child-$i& done
wait
for i in `seq -w 1 3 3000`; do rmdir ${dir}/child-$i& done
for i in `seq -w 2 3 3000`; do rmdir ${dir}/child-$i& done
wait
inod=`stat ${dir} -c %i`
cp $mp/sub_$(( $inod % 1000 ))/meta$inod $target/meta_isdir

cp $0 $target/
tar zcvf sample.tgz $target
