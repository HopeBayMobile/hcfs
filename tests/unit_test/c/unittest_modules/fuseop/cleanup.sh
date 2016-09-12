#!/bin/bash
exec 2>/dev/null
CLEAN_FUSE()
{
	for i in `\ls /sys/fs/fuse/connections/`; 
	do
		echo 1 > /sys/fs/fuse/connections/$i/abort
	done
}


echo "Unittest will abort fuse session to avoid stucking."

{
	sleep $RERUN_TIMEOUT;
	CLEAN_FUSE;
} &
