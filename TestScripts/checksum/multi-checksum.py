import os
import sys

def my_fork(action, taskPrefix, taskCnt, filetype, repetition, mountpoint):
	for i in range(taskCnt):
		child_pid = os.fork()
		if child_pid == 0:
			taskId = "%s"%taskPrefix+str(i)
			folder = mountpoint+"/%s"%taskId
			os.system("mkdir -p %s" % folder)
			os.system("./checksum.sh %s %s %s %s %s" % (action, taskId, filetype, repetition, folder))
			break

if __name__ == "__main__":
	if len(sys.argv) != 7:
		print len(sys.argv)
		print "usage  : python multi-checksum.py action taskPrefix taskCnt filetype repetition mountpint"
		print "example: python multi-checksum.py 10 -run MM 2 10 /mfsmnt/folder2"
		exit(1)

	my_fork(sys.argv[1], sys.argv[2], int(sys.argv[3]), sys.argv[4], sys.argv[5], sys.argv[6])


