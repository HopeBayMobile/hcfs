#!/bin/bash
function CLEAN_FUSE
{
	for i in `\ls /sys/fs/fuse/connections/`; 
	do
		echo 1 > /sys/fs/fuse/connections/$i/abort
	done
}


CLEAN_FUSE
{
	sleep $RERUN_TIMEOUT;
	echo "==="
	echo "==="
	echo "==="
	echo "=== Clean fuse mount to avoid stucking ==="
	echo "==="
	echo "==="
	echo "==="
	CLEAN_FUSE;
} &
