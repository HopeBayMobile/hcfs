#/bin/bash

action=$1
id=$2
fileSize=$3
copyTimes=$4
mountPoint=$5

fileName=file${fileSize}
mkdir -p ./result
rm -rf ./result/checksumList${id}
rm -rf ./result/checksumErrorList${id}

# Show usage then exit
usage () {
	echo "Usage: ./checksum.sh {-action} {task id} {file size} {copy times} {mount point}\n"
	echo "for example: ./checksum.sh -run 1 2 10 /mnt/tmp"
	exit 1
}

# Remove all file under the mount point folder
clean () {
	echo "Removing files under ${mountPoint}..."
	sudo rm ${mountPoint}/*
#	ssh -t root@g6 "rm /mnt/sd2"
}


# Copy the file to mount point
copyfile () {
	DATE=`date +%Y.%m.%d.%H.%M.%S`
	echo "Starting time: ${DATE}" >> ./result/checksumList${id}
	for i in `seq 1 ${copyTimes}`; do
		newFileName=${fileName}_${i}
        	sudo cp file/${fileName} ${mountPoint}/${newFileName}
#		if [ ${i} = 1000 ]; then
#			echo "Stop g68 mfschunkserver" >>checksumList
#			ssh -t root@192.168.1.25 "sh ~/stopChunkServer.sh"
#			echo "Start rice04 mfschunkserver" >>checksumList
#			/usr/sbin/mfschunkserver start
#		fi

		echo "Copy ${fileName} ${i}/${copyTimes}..."
	done

	DATE=`date +%Y.%m.%d.%H.%M.%S`
	echo "Finish time: ${DATE}" >> ./result/checksumList${id}
	echo "Copy ${fileName} for ${copyTimes} to ${mountPoint} is finished."
}

# Save the chechsum of original file new create file in checksumList
checksum_create () {
	md5sum file/${fileName} > checksumTemp${id}
	oriChecksum=`cat checksumTemp${id} | cut -c 1-32`
	echo "Checksum                          file" >>./result/checksumList${id}
	cat checksumTemp${id} >>./result/checksumList${id}
	rm checksumTemp${id}

	for i in `seq 1 ${copyTimes}`; do
		md5sum ${mountPoint}/${fileName}_${i} > checksumTemp${id}
		newChecksum=`cat checksumTemp${id} | cut -c 1-32`
		md5sumResult=`md5sum ${mountPoint}/${fileName}_${i}`

		echo "Calculating checksum ${i}/${copyTimes}..."
		if [ "${oriChecksum}" = "${newChecksum}" ]; then
			result="T"
		else
			result="F"
			echo "${fileName}_${i} ${result}" >> ./result/checksumErrorList${id}
		fi

		echo "${md5sumResult} ${result}" >> ./result/checksumList${id}
			
	done
	rm checksumTemp${id}
}


info_save () {

	echo "g3 /mnt/sd3 file count:">>checksumList
	ssh -t nii@g3 "cd /mnt/sd3;ls | grep -c file" >> checksumList

	echo "g4 /mnt/sd3 file count:">>checksumList
        ssh -t nii@g4 "cd /mnt/sd3;ls | grep -c file" >> checksumList

	echo "g6 /mnt/sd3 file count:">>checksumList
        ssh -t nii@g6 "cd /mnt/sd3;ls | grep -c file" >> checksumList

	echo "g6 /mnt/sd3 file count:">>checksumList
        ssh -t nii@g6 "cd /mnt/sd3;ls | grep -c file" >> checksumList

	echo "g7 /mnt/sd3 file count:">>checksumList
        ssh -t nii@g7 "cd /mnt/sd3;ls | grep -c file" >> checksumList

	cat checksumList >> checksumHistory
	rm checksumList

	#ssh -t root@g6 "./../home/nii/Glusterfs_testing/layout.py SD rp g6:/mnt/sd3 g7:/mnt/sd3 none"
}

#case ${mountPoint} in
#/mnt/SDNAS|/mnt/dcloudNAS|/mnt/SDNAS3|none)
 #       ;;
#*)
#	echo "\nCannot find ${mountPoint}. Please check the mount point."
#        usage
#	;;
#esac

case ${fileSize} in
0|1|2|none)
        ;;
*)
	echo "\nThe file size is between 0 and 2."
        usage
	;;
esac

case ${action} in
-clean)
	clean
	;;

-copy)
	copyfile
#	checksum_create
#	info_save
	;;
-run)
	copyfile
	checksum_create
	;;
-check)
	checksum_create
        ;;
*)
	usage
	;;
esac



